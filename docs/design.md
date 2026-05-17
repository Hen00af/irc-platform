# webserv 設計図 — フローとデータ保持

このドキュメントは webserv の **全体構造** を 2 枚の Mermaid 図で表現する。

- **図 1 — リクエストライフサイクル**: 1 リクエストが受信から送信完了まで辿る経路（フローチャート）
- **図 2 — クラス図**: 各モジュールが何を保持し、何を参照するか（クラス図）

> 上位の責務分離（nginx と CGI/backend の境界）は [`architecture.md`](./architecture.md) を参照。学習ロードマップは [`../prd/src/rooting/LEARNING.md`](../prd/src/rooting/LEARNING.md) を参照。

オレンジ点線 / `<<planned>>` は未実装モジュール。

---

## 図 1 — リクエストライフサイクル

```mermaid
flowchart TD
    Start([webserv 起動]) --> ParseConf[Conf::read_file<br/>conf/nginx.conf をパース]
    ParseConf --> Bind[listen / bind<br/>kqueue 初期化]
    Bind --> Kq{{kqueue イベント待ち}}

    Kq -->|listen_fd EVFILT_READ| Accept[accept で client_fd 取得<br/>Connection を生成<br/>read イベント登録]
    Accept --> Kq

    Kq -->|client_fd EVFILT_READ| Read[recv で read_buffer に追記]
    Read --> Done{リクエスト<br/>完成?}
    Done -->|まだ| Kq

    Done -->|完成| Parse[RequestParser::parseRequest<br/>method / target / Host 抽出]
    Parse --> ParseOK{パース成功?}
    ParseOK -->|400/505| MakeErr1[error_page から body 構築]
    ParseOK -->|OK| Route[route servers, req]

    Route --> Branch{RouteStatus}

    Branch -->|ROUTE_OK + 静的| Static[StaticHandler<br/>fs_path を open / 読み込み]
    Branch -->|ROUTE_OK + CGI| Cgi[CGIHandler<br/>fork + exec + pipe]
    Branch -->|ROUTE_REDIRECT| Redir[301/302 レスポンス組み立て]
    Branch -->|ROUTE_NOT_FOUND| MakeErr2[404 error_page]
    Branch -->|ROUTE_METHOD_NOT_ALLOWED| MakeErr3[405 error_page]

    Static --> Build[ResponseBuilder<br/>status line + headers + body]
    Cgi --> Build
    Redir --> Build
    MakeErr1 --> Build
    MakeErr2 --> Build
    MakeErr3 --> Build

    Build --> SwitchWrite[write イベントに切り替え]
    SwitchWrite --> Kq

    Kq -->|client_fd EVFILT_WRITE| Write[send でレスポンス送信]
    Write --> WriteDone{全部書けた?}
    WriteDone -->|まだ| Kq
    WriteDone -->|完了| Close[close client_fd<br/>Connection 破棄]
    Close --> Kq

    classDef planned fill:#fff4e6,stroke:#ff9933,stroke-dasharray: 5 5
    class Cgi,Redir,MakeErr1,MakeErr2,MakeErr3,Build,Static planned
```

### 状態遷移として読むと

| フェーズ | kqueue が待つイベント | Connection の状態 | 何が増える |
|---|---|---|---|
| accept 後 | client_fd EVFILT_READ | READING | ClientState (空 buffer) |
| 受信中 | client_fd EVFILT_READ | READING | read_buffer に追記 |
| パース完了 | (同期処理) | ROUTING | RequestParser (Range が埋まる) |
| route 完了 | (同期処理) | HANDLING | RouteResult |
| handler 完了 | (同期処理) | WRITING | Response、write_buffer |
| 送信中 | client_fd EVFILT_WRITE | WRITING | write_buffer が減る |
| 送信完了 | — | CLOSING | (全破棄) |

---

## 図 2 — クラス図（何を保持するか）

```mermaid
classDiagram
  direction LR

  namespace persing {
    class Conf {
      -vector~ServerConfig~ _servers
      +read_file(name)
      +get_servers() vector~ServerConfig~
    }
    class ServerConfig {
      <<struct>>
      +string name
      +string listen
      +string root
      +vector~string~ methods
      +map error_pages
      +vector~LocationConfig~ locations
    }
    class LocationConfig {
      <<struct>>
      +string dir
      +string root
      +string index
      +string listing
      +string redir
      +vector~string~ methods
    }
    class RequestParser {
      -string _raw_request
      -Range _request_line
      -map~string,Range~ _headers
      +parseRequest(raw) bool
      +getMethod() string
      +getTarget() string
      +getHeader(key) string
    }
    class Range {
      <<struct>>
      +size_t start
      +size_t end
    }
  }

  namespace rooting {
    class RouteStatus {
      <<enum>>
      ROUTE_OK
      ROUTE_NOT_FOUND
      ROUTE_METHOD_NOT_ALLOWED
      ROUTE_REDIRECT
    }
    class RouteResult {
      <<struct>>
      +RouteStatus status
      +ServerConfig* server
      +LocationConfig* location
      +string fs_path
      +string redirect_to
    }
    class routing_fns {
      <<utility>>
      +route(servers, req) RouteResult
      +pick_server(servers, host) ServerConfig*
      +pick_location(server, target) LocationConfig*
    }
  }

  namespace io {
    class Server {
      -int _listen_fd
      -int _server_fd
      -map~int,Connection~ _clients
      +initServer(port)
      +boot_server(conf)
      +build_connection(servers)
    }
    class ClientState {
      <<struct>>
      +string read_buffer
      +string write_buffer
    }
  }

  namespace planned {
    class Connection {
      <<planned>>
      +int fd
      +ConnState state
      +ClientState io
      +RequestParser parser
      +RouteResult route
      +Response response
    }
    class ConnState {
      <<planned>>
      READING
      ROUTING
      HANDLING
      WRITING
      CLOSING
    }
    class StaticHandler {
      <<planned>>
      +handle(RouteResult) Response
    }
    class CGIHandler {
      <<planned>>
      +handle(RouteResult, parser) Response
    }
    class ResponseBuilder {
      <<planned>>
      +build_ok(body) Response
      +build_redirect(url) Response
      +build_error(code, error_pages) Response
    }
    class Response {
      <<planned>>
      +int status_code
      +map headers
      +string body
    }
  }

  Conf "1" *-- "1..*" ServerConfig : owns
  ServerConfig "1" *-- "0..*" LocationConfig : owns
  RequestParser "1" *-- "0..*" Range : holds

  Server "1" *-- "0..*" Connection : per client
  Server ..> Conf : reads
  Connection *-- ClientState : has io buffers
  Connection *-- RequestParser : has parser
  Connection o-- RouteResult : has decision
  Connection o-- Response : has response
  Connection --> ConnState : tracks state

  RouteResult --> ServerConfig : points to
  RouteResult --> LocationConfig : points to
  routing_fns ..> RequestParser : reads
  routing_fns ..> ServerConfig : reads
  routing_fns ..> RouteResult : creates

  StaticHandler ..> RouteResult : reads
  StaticHandler ..> Response : produces
  CGIHandler ..> RouteResult : reads
  CGIHandler ..> RequestParser : reads
  CGIHandler ..> Response : produces
  ResponseBuilder ..> Response : produces
```

---

## 主な設計判断

| 判断 | 内容 |
|---|---|
| **`Connection` を導入する** (planned) | 今の `Server::ClientState` は read/write buffer のみ。これに `RequestParser` / `RouteResult` / `Response` / `ConnState` を集約して **「1 接続の生涯」を 1 つの構造で表現** |
| **`ConnState` enum** | kqueue は edge-triggered。各接続が「次に何を待つか」を持つ必要があり、READING → ROUTING → HANDLING → WRITING の状態機械にする |
| **`routing_fns` は class でなく namespace 表現** | 状態を持たない関数群。class にする意味がない（cf. `LEARNING.md`） |
| **`RouteResult` は `ServerConfig*` / `LocationConfig*` を持つ（値ではない）** | Conf 内の vector が真の所有者。routing は「指紋」を返すだけで複製しない（コピー回避 + 設定が単一の真実の源泉） |
| **handler は class、`ResponseBuilder` も class** | 状態は持たないが、責務が違うものは class でまとめて検索性を上げる（読者のため） |
| **`Connection.parser` は composition (`*--`)** | parser のライフサイクルは Connection と同じ。所有関係 |
| **`Connection.route` / `.response` は aggregation (`o--`)** | 状態によっては未生成（READING フェーズでは route も response もまだ存在しない） |
| **handler kind (static/CGI) は `RouteResult` に持たない** | CGI 判定は `fs_path` の拡張子で決まる（subject 仕様）。dispatch 層の責務、routing は知らなくていい |

---

## 未解決の論点

1. **`Server` クラスが 2 つある** (`server.hpp` と `server_multi_io.hpp`) — 多重 I/O 版に統一する前提。図は統一後の姿
2. **`Connection` を `Server` の直接メンバ**にするか、`ConnectionPool` を切るか — 当面は直接メンバ（最小構成）
3. **CGI のタイムアウト / プロセス管理** — subject が要求する詳細は未確定。本図では CGIHandler の内部実装として隠蔽
4. **keep-alive / chunked transfer** — subject 範囲だが本図では明示せず。CLOSING ではなく READING に戻る枝が必要になる可能性
5. **`error_pages` の解決層** — 図では ResponseBuilder が担うが、`RouteResult` が候補パスを渡す案もある

---

## 関連ドキュメント

- [`architecture.md`](./architecture.md) — nginx vs backend の責務分離、subject 範囲外の明示
- [`../prd/src/rooting/LEARNING.md`](../prd/src/rooting/LEARNING.md) — routing 層の学習ロードマップ
- [`../tmp/lab06_routing/README.md`](../tmp/lab06_routing/README.md) — routing の小型実験実装と設計判断
