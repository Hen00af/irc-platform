# server/ — ネットワーク I/O 層

TCP/IP ソケットの open / bind / listen / accept、および複数クライアントの送受信を担う最下層。HTTP プロトコル詳細やパースは持たない（コメントにも「HTTPのパースコンポーネント・バッファは持たない」と明記）。

> 全体設計は [`docs/architecture.md`](../../../docs/architecture.md) / [`docs/design.md`](../../../docs/design.md) 参照。

## 役割の境界

| 含むもの | 含まないもの |
|---|---|
| `socket` / `setsockopt` / `bind` / `listen` | HTTP リクエストのパース（→ `persing/`） |
| `accept` / `recv` / `send` / `close` | location マッチ（→ `rooting/`） |
| 多重 I/O（現状 `poll`、将来 `kqueue`） | レスポンスの中身生成（→ 将来 `handler/`） |
| クライアントごとの送受信 buffer 管理 | ファイル操作 |
| 非ブロッキング fd の管理 (`O_NONBLOCK`) | error_page の解決 |

## ファイル一覧

| ファイル | 行数 | 状態 | 責務 |
|---|---:|---|---|
| `server.hpp` | 40 | ✅ | `Server` クラス宣言（基本版） |
| `server.cpp` | 124 | 🔴 | **構文エラー (line 119: `}` 抜け)** で現状コンパイル不可。基本版実装 |
| `server_multi_io.hpp` | 57 | ✅ | `Server` クラス宣言（multi I/O 版） + `ClientState` 内部 struct |
| `server_multi_io.cpp` | 234 | 🟡 | `poll` ベースの多重 I/O 実装。レスポンスは現状ハードコード |
| `io-server.cpp` | 126 | 🔴 | **`server.cpp` とほぼ同一**（同名 `Server` クラスの第 3 重複定義） |

## ⚠️ 重大な構造上の問題

### 1. 同名 `Server` クラスが 2 つ宣言されている

| ヘッダ | 中身 |
|---|---|
| `server.hpp` | `class Server { ... 同名 ... }` |
| `server_multi_io.hpp` | `class Server { ... 同名 ... +ClientState }` |

両方の hpp で同じ名前空間に `class Server` が宣言されており、**両方をリンクすると ODR (one definition rule) 違反**。**どちらか一方に統一**が必要。

### 2. `Server` の実装が 3 ファイルに分散している

| ファイル | 実装内容 | 妥当性 |
|---|---|---|
| `server.cpp` | 単一接続 `accept` + ブロッキング recv ループ | 古い試作 |
| `io-server.cpp` | `server.cpp` とほぼ同一 | 重複 |
| `server_multi_io.cpp` | `poll` 多重 I/O | 現行 |

3 つを同時にビルドすると `Server::build_connection` 等のシンボル多重定義リンクエラー。**`server_multi_io.cpp` に統一**するのが妥当。

### 3. `server.cpp` の構文エラー

`server.cpp:118-119` に閉じ括弧 `}` が抜けており、`Server::~Server()` の body が次の `tcp(Conf &conf)` 関数とつながっている。**この時点でコンパイル不能**。

### 4. `std::stoi` の使用（C++98 違反）

`server_multi_io.cpp:43`、`server.cpp:39` 等で `std::stoi` を使用。C++11 以降の関数で、subject の **C++98 制約に違反**。`std::atoi` か自前パーサに置換すべき。

## 公開 API

```cpp
class Server {
public:
    Server();
    ~Server();
    void initServer(int port);
    int  getListenFd() const;
    void boot_server(Conf &conf);
    void build_connection(const std::vector<ServerConfig> &servers);

    // server_multi_io.hpp のみ
    struct ClientState {
        std::string read_buffer;
        std::string write_buffer;
    };
};

void tcp(Conf &conf);  // server.cpp 内で定義（おそらく main から呼ぶ予定）
```

## 現状の I/O フロー（`server_multi_io.cpp`）

```
build_connection() で socket / bind / listen
  ↓
poll(fds, ...) でイベント待ち
  ↓
listen_fd POLLIN
  → accept で client_fd 取得
  → ClientState を clients[client_fd] に挿入
  → fds に POLLIN で追加
  ↓
client_fd POLLIN
  → recv で read_buffer に追記
  → "\r\n\r\n" 検出で「ハードコードの 200 OK + Hello webserv」を作成
  → fds[i].events を POLLOUT に切替
  ↓
client_fd POLLOUT
  → send で write_buffer 送信
  → 完了したら close + clients.erase + fds.erase
```

## 他モジュールとの関係

- **依存先**:
  - `persing/persing_conf.hpp` → `Conf::get_servers()` で listen ポートを引く
- **依存元（将来）**:
  - `persing/` → 受信した read_buffer を `RequestParser::parseRequest` に渡す
  - `rooting/` → `route()` を呼び RouteResult を得る
  - `handler/`（未存在）→ static / CGI / redirect / error の各処理

## 現状と TODO

### 喫緊
- [ ] **`server.cpp` の構文エラー修正**（`}` 追加）
- [ ] **重複する `Server` 定義を統合**（`server_multi_io.{hpp,cpp}` に一本化、`server.{hpp,cpp}` と `io-server.cpp` 削除）
- [ ] **`std::stoi` を C++98 互換に置換**
- [ ] **`main.cpp` の include パス修正**（`source/persing/webserver.hpp` ← 該当ファイル無し）

### 機能拡張
- [ ] `poll` → `kqueue` への移行（subject 要件）
- [ ] 受信 buffer を `RequestParser` に渡す配線
- [ ] ハードコードレスポンスを `rooting/` + `handler/` 経由に置き換え
- [ ] `Connection` 構造体の導入（buffer + parser + route + response を統合）
- [ ] keep-alive 対応（現状は `Connection: close`）
- [ ] タイムアウト処理
- [ ] エラー時の適切なステータス返却

## 関連ドキュメント

- [`docs/architecture.md`](../../../docs/architecture.md) — webserv 全体の責務分離
- [`docs/design.md`](../../../docs/design.md) — クラス図 / リクエストライフサイクル（Connection 状態機械）
