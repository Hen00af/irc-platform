# webserv 設計図 — フローとデータ保持

このドキュメントは webserv の **全体構造** を 2 枚の Mermaid 図で表現する。

- **図 1 — リクエストライフサイクル**: 1 リクエストが受信から送信完了まで辿る経路（フローチャート）
- **図 2 — クラス図**: 各モジュールが何を保持し、何を参照するか（クラス図）

> 上位の責務分離（nginx と CGI/backend の境界）は [`architecture.md`](./architecture.md) を参照。

オレンジ点線 / `<<planned>>` は未実装モジュール。

> **本ドキュメントは「目標形」を描く**。現状コードとのギャップは下記「現状とのギャップ」セクションに集約。

---

## 図 1 — リクエストライフサイクル

```mermaid
flowchart TD
    Start([webserv 起動]) --> ParseConf[Conf::read_file<br/>conf/nginx.conf をパース]
    ParseConf --> Bind[listen / bind<br/>poll 初期化]
    Bind --> Loop{{poll イベント待ち}}

    Loop -->|listen_fd POLLIN| Accept[accept で client_fd 取得<br/>Connection を生成<br/>POLLIN 登録]
    Accept --> Loop

    Loop -->|client_fd POLLIN| Read[recv で read_buffer に追記]
    Read --> Done{リクエスト<br/>完成?}
    Done -->|まだ| Loop

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

    Build --> SwitchWrite[POLLOUT に切り替え]
    SwitchWrite --> Loop

    Loop -->|client_fd POLLOUT| Write[send でレスポンス送信]
    Write --> WriteDone{全部書けた?}
    WriteDone -->|まだ| Loop
    WriteDone -->|完了| Close[close client_fd<br/>Connection 破棄]
    Close --> Loop

    classDef default fill:#e8eaf6,stroke:#3949ab,color:#1a237e
    classDef decision fill:#fff9c4,stroke:#f57f17,color:#e65100
    classDef planned fill:#ffe0b2,stroke:#e65100,color:#bf360c,stroke-dasharray: 5 5
    classDef loop fill:#c8e6c9,stroke:#2e7d32,color:#1b5e20
    class Cgi,Redir,MakeErr1,MakeErr2,MakeErr3,Build,Static planned
    class Done,ParseOK,Branch,WriteDone decision
    class Loop loop
```

> 多重 I/O は `poll()` で実装。subject は特定の方式（poll / select / kqueue / epoll 等）を強制しないので、当面 poll を維持する。

### 状態遷移として読むと

| フェーズ | poll が待つイベント | Connection の状態 | 何が増える |
|---|---|---|---|
| accept 後 | client_fd POLLIN | READING | ClientState (空 buffer) |
| 受信中 | client_fd POLLIN | READING | read_buffer に追記 |
| パース完了 | (同期処理) | ROUTING | RequestParser (Range が埋まる) |
| route 完了 | (同期処理) | HANDLING | RouteResult |
| handler 完了 | (同期処理) | WRITING | Response、write_buffer |
| 送信中 | client_fd POLLOUT | WRITING | write_buffer が減る |
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

  namespace http {
    class Request {
      <<planned>>
      +string method
      +string target
      +string version
      +map~string,string~ headers
      +string body
    }
    class Response {
      <<planned>>
      +int status_code
      +map~string,string~ headers
      +string body
    }
    class HttpStatus {
      <<planned>>
      +reason_phrase(code) string
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
    class rooting_h {
      <<header>>
      +route(servers, req) RouteResult
      +pick_server(servers, host) ServerConfig*
      +pick_location(server, target) LocationConfig*
    }
  }

  namespace server {
    class Server {
      -int _listen_fd
      -int _server_fd
      -int _port
      -int _client_fd
      +initServer(port)
      +build_connection(servers)
      +boot_server(conf)
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
      +handle(RouteResult, Request) Response
    }
    class ResponseBuilder {
      <<planned>>
      +build_ok(body) Response
      +build_redirect(url) Response
      +build_error(code, error_pages) Response
    }
  }

  Conf "1" *-- "1..*" ServerConfig : 所有
  ServerConfig "1" *-- "0..*" LocationConfig : 所有
  RequestParser "1" *-- "0..*" Range : 保持

  Server "1" *-- "0..*" Connection : 接続ごと
  Server ..> Conf : 参照
  Connection *-- ClientState : I/O buffer 保持
  Connection *-- RequestParser : parser 保持
  Connection o-- RouteResult : 判定結果保持
  Connection o-- Response : response 保持
  Connection o-- Request : (planned) パース結果保持
  Connection --> ConnState : 状態管理

  RequestParser ..> Request : (planned) 生成

  RouteResult --> ServerConfig : 参照
  RouteResult --> LocationConfig : 参照
  rooting_h ..> RequestParser : 参照
  rooting_h ..> ServerConfig : 参照
  rooting_h ..> RouteResult : 生成

  StaticHandler ..> RouteResult : 参照
  StaticHandler ..> Response : 生成
  CGIHandler ..> RouteResult : 参照
  CGIHandler ..> Request : 参照
  CGIHandler ..> Response : 生成
  ResponseBuilder ..> Response : 生成
  ResponseBuilder ..> HttpStatus : 参照

  note for rooting_h "free 関数群（class ではない）。<br/>状態を持たないので header 単位でまとめている"
  note for Server "現状コードでは server.hpp / server_multi_io.hpp / io-server.cpp に<br/>**同名 Server クラスの重複定義**あり。図は統合後の目標形"
  note for Request "現状は persing/RequestParser が文字列ベースで保持。<br/>http/request.hpp に値クラスとして移管予定"

  style Conf fill:#bbdefb,stroke:#1565c0,color:#0d47a1
  style ServerConfig fill:#bbdefb,stroke:#1565c0,color:#0d47a1
  style LocationConfig fill:#bbdefb,stroke:#1565c0,color:#0d47a1
  style RequestParser fill:#bbdefb,stroke:#1565c0,color:#0d47a1
  style Range fill:#bbdefb,stroke:#1565c0,color:#0d47a1

  style Request fill:#ffe0b2,stroke:#e65100,color:#bf360c
  style Response fill:#ffe0b2,stroke:#e65100,color:#bf360c
  style HttpStatus fill:#ffe0b2,stroke:#e65100,color:#bf360c

  style RouteStatus fill:#e1bee7,stroke:#6a1b9a,color:#4a148c
  style RouteResult fill:#e1bee7,stroke:#6a1b9a,color:#4a148c
  style rooting_h fill:#e1bee7,stroke:#6a1b9a,color:#4a148c

  style Server fill:#c8e6c9,stroke:#2e7d32,color:#1b5e20
  style ClientState fill:#c8e6c9,stroke:#2e7d32,color:#1b5e20

  style Connection fill:#ffe0b2,stroke:#e65100,color:#bf360c
  style ConnState fill:#ffe0b2,stroke:#e65100,color:#bf360c
  style StaticHandler fill:#ffe0b2,stroke:#e65100,color:#bf360c
  style CGIHandler fill:#ffe0b2,stroke:#e65100,color:#bf360c
  style ResponseBuilder fill:#ffe0b2,stroke:#e65100,color:#bf360c
```

> **色分け**: 青=persing（現状実装）／緑=server（現状実装）／紫=rooting（header 設計済み）／オレンジ=未実装 (`<<planned>>` 含む http も該当)

---

## 主な設計判断

| 判断 | 内容 |
|---|---|
| **`Connection` を導入する** (planned) | 今の `Server::ClientState` は read/write buffer のみ。これに `RequestParser` / `RouteResult` / `Response` / `ConnState` を集約して **「1 接続の生涯」を 1 つの構造で表現** |
| **`ConnState` enum** | 各接続が「次に何を待つか」を持つ必要があり、READING → ROUTING → HANDLING → WRITING の状態機械にする（多重 I/O の方式が poll でも他でも同じ思想） |
| **`rooting_h` は class でなく header 単位の関数群** | 状態を持たないので class にする意味がない。図上は `<<header>>` ステレオタイプでまとめる（実体は class ではない） |
| **`RouteResult` は `ServerConfig*` / `LocationConfig*` を持つ（値ではない）** | Conf 内の vector が真の所有者。routing は「指紋」を返すだけで複製しない（コピー回避 + 設定が単一の真実の源泉） |
| **handler は class、`ResponseBuilder` も class** | 状態は持たないが、責務が違うものは class でまとめて検索性を上げる（読者のため） |
| **`Connection.parser` は composition (`*--`)** | parser のライフサイクルは Connection と同じ。所有関係 |
| **`Connection.route` / `.response` は aggregation (`o--`)** | 状態によっては未生成（READING フェーズでは route も response もまだ存在しない） |
| **handler kind (static/CGI) は `RouteResult` に持たない** | CGI 判定は `fs_path` の拡張子で決まる（subject 仕様）。dispatch 層の責務、routing は知らなくていい |
| **HTTP 値クラスは `http/` に置く** | `Request` / `Response` / `HttpStatus` は http プロトコル層のもの。文字列 → 構造体の変換 (`RequestParser`) は `persing/` に残し、変換後の値オブジェクトを `http/` に集約 |

---

## 現状とのギャップ

図は **目標形**。現状コードとの主な差分：

| 図上 | 現実 |
|---|---|
| `namespace server` に `Server` 1 つ | `server.hpp` と `server_multi_io.hpp` で **同名 `Server` クラスが 2 重定義**。さらに `server.cpp` / `io-server.cpp` / `server_multi_io.cpp` の 3 ファイルが Server を実装（シンボル重複）|
| `poll イベント待ち` ノード | 実装と一致（poll ベース）。subject は多重 I/O 方式を強制しないため変更不要 |
| `Server` のメンバ `_listen_fd` / `_server_fd` 等 | 一致 |
| Server の Connection 保持 | **未実装**。現コードでは `boot_server` 内の local 変数 `std::vector<pollfd>` + `std::map<int, ClientState>` |
| `RequestParser` から `Request` 値クラス生成 | **未実装**。現状は `RequestParser` 内に文字列ベースで保持、`getMethod()` 等で都度返す |
| `http/` 配下の `Request` / `Response` / `HttpStatus` | **未実装**。`http/request.hpp/cpp` はプレースホルダ |
| `rooting_h` の関数群 | header は宣言済み (feat/routing) だが、`rooting.cpp` は現状スクラッチパッドの `main()` |
| handler / CGI / ResponseBuilder | **未実装**。現状は `server_multi_io.cpp` 内で `"Hello webserv\n"` ハードコード |

ファイル単位の現状は各ディレクトリの README に詳細：

- [`../prd/src/http/README.md`](../prd/src/http/README.md)
- [`../prd/src/persing/README.md`](../prd/src/persing/README.md)
- [`../prd/src/rooting/README.md`](../prd/src/rooting/README.md)
- [`../prd/src/server/README.md`](../prd/src/server/README.md)
- [`../prd/src/logging/README.md`](../prd/src/logging/README.md)

---

## 未解決の論点

1. **`Server` クラスの重複定義整理** — `server.hpp` か `server_multi_io.hpp` のどちらに統一するか
2. **`server.cpp` の構文エラー** (line 119 の `}` 抜け) — 修正後に重複定義整理
3. **`Connection` を `Server` の直接メンバ**にするか、`ConnectionPool` を切るか — 当面は直接メンバ（最小構成）
4. **CGI のタイムアウト / プロセス管理** — subject が要求する詳細は未確定。本図では CGIHandler の内部実装として隠蔽
5. **keep-alive / chunked transfer** — subject 範囲だが本図では明示せず。CLOSING ではなく READING に戻る枝が必要になる可能性
6. **`error_pages` の解決層** — 図では ResponseBuilder が担うが、`RouteResult` が候補パスを渡す案もある
7. **`RequestParser` を残すか、`http/Request` に統合するか** — 図では 2 つに分離（パーサと値クラス）。実装時に統合判断

---

## 関連ドキュメント

- [`architecture.md`](./architecture.md) — nginx vs backend の責務分離、subject 範囲外の明示
- [`../tmp/lab06_routing/README.md`](../tmp/lab06_routing/README.md) — routing の小型実験実装と設計判断
- 各ディレクトリの README（上記「現状とのギャップ」末尾参照）
