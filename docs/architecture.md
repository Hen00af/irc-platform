# webserv アーキテクチャ — nginx と backend の境界

このドキュメントは「**何を作っていて、どこまでが webserv の責務か**」を整理する設計メモ。実装に迷ったら最初にここを読む。

---

## 1. 実務における nginx ↔ backend の関係

普通の Web アプリの構成：

```
[client]
   │ HTTP
   ▼
[nginx]                ← front gateway (= HTTP server)
   ├── 静的ファイルは自分で返す
   ├── /api/* は backend に転送 (reverse proxy)
   ├── TLS 終端、ログ、レートリミット
   └── location ブロックで「どこに渡すか」を決定
   │ HTTP / fastcgi / unix socket
   ▼
[backend app]          ← Rails / Express / Django ...
   ├── アプリ内ルーティング (URL → controller)
   ├── DB アクセス
   └── ビジネスロジック
```

ここで重要なのは「**routing**」という言葉が **2層に存在** すること：

| 層 | 何を決めるか | 例 |
|---|---|---|
| nginx routing | URL → **どの転送先 / どの静的ファイル** か | `/api/* → backend`, `/static/* → /var/www` |
| backend routing | URL → **どのコントローラ関数** か | `/api/users → UsersController#index` |

**実務で「nginx と backend は分けるべき」と感じる感覚は、この 2 層の責務分離のこと**。これは正しい設計勘。

---

## 2. webserv は「nginx そのもの」を作るプロジェクト

webserv の subject は **HTTP server を作れ** と言っている。backend は作らない。webserv における「backend 相当のもの」= **CGI スクリプト**。

```
[client]
   │
   ▼
[webserv]              ← このリポジトリで作っているもの (= nginx 相当)
   ├── 受信ループ (kqueue)
   ├── HTTP リクエストパース
   ├── routing (location マッチ、method 判定)
   ├── 静的ファイル handler
   ├── CGI handler         ← ここから先が「外部 backend」
   ├── redirect handler
   └── error page / autoindex
   │ stdin/stdout (CGI 規約)
   ▼
[CGI script]           ← .php / .py / .pl など (= backend 相当)
   ├── 別プロセス (fork + exec)
   ├── 環境変数で REQUEST_METHOD/QUERY_STRING を受け取る
   └── stdout に HTTP body を吐く
```

つまり：

| 実務世界 | webserv |
|---|---|
| nginx | webserv 本体 |
| backend (Rails/Express) | CGI スクリプト |
| nginx routing | webserv の `routing` モジュール |
| backend routing | CGI スクリプト内のロジック (webserv は関与しない) |
| HTTP / fastcgi 境界 | fork + 環境変数 + stdin/stdout 境界 |

---

## 3. 「routing は webserv 側」「CGI 実行は backend 境界」

「nginx と backend を分けるべき」を webserv に翻訳すると：

> **routing と CGI 実行は分けるべき**

つまりこういう構造になる：

```
[parser] → [router] → [handler dispatch]
                         ├── static_handler   (ファイルを読む)
                         ├── cgi_handler      ← この入口までが webserv の関心
                         │     │
                         │     ▼
                         │   (fork/exec/pipe = backend 境界)
                         │     │
                         │     ▼
                         │   [CGI process]    ← 別プロセス、別世界
                         └── redirect_handler
```

- `router` は **「どの handler に渡すか」を決めるだけ**。CGI を起動したり、ファイルを開いたり、レスポンスを書いたりはしない
- `cgi_handler` の **入口 (どんなリクエストを CGI に渡すか)** は webserv の責務
- `cgi_handler` の **出口 (CGI が出力した内容)** を受け取って HTTP レスポンスにする部分も webserv の責務
- **その間 (CGI スクリプトの中身)** は完全にユーザー領域。webserv は中身を知らない

---

## 4. ディレクトリ構造との対応

```
prd/src/
├── server/        kqueue I/O ループ              ← nginx event loop 相当
├── persing/       HTTP request parser            ← nginx request parser 相当
│                  config parser
├── rooting/       location マッチ + dispatch     ← nginx location 相当 (= このドキュメントの主役)
├── http/          response builder / status      ← nginx response 相当
├── (cgi/)         CGI 起動 (今後追加)            ← nginx fastcgi 相当 = backend 境界
└── logging/       ログ                           ← nginx access/error log 相当
```

`rooting/` ディレクトリは **nginx routing** に対応する。**backend routing ではない**。中で「URL を controller に振る」のではなく、「URL を **どの handler (= 静的/CGI/redirect)** に振る」を決める。

---

## 5. webserv の routing が決めること / 決めないこと

### 決めること (nginx routing)

| 入力 | 出力 |
|---|---|
| `Host` ヘッダ | どの `server { ... }` ブロックの設定を使うか |
| request target (path) | どの `location { ... }` ブロックの設定を使うか |
| HTTP method | その location で許可されているか (→ 405 判定) |
| location の `root` / `alias` | ディスク上の実ファイルパス |
| location の `index` | ディレクトリアクセス時のデフォファイル |
| location の `return` (redir) | 301/302 の発火 |
| location の `autoindex` | ディレクトリ一覧を出すか |

これらは **設定ファイル + 受信したリクエスト** だけで決まる。**ファイルの中身は読まない**、**CGI も起動しない**、**ソケットにも書かない**。**純粋な決定ロジック**。

### 決めないこと (handler の仕事)

- ファイルを `open` して中身を読む
- CGI を `fork + exec` する
- HTTP レスポンスを構築してソケットに書く
- `Content-Type` を MIME から判定する (これは static handler の領分)
- ディレクトリ一覧の HTML を生成する

---

## 6. nginx を参考にすべき範囲

webserv subject の範囲で、nginx 公式ドキュメントを **つまみ読みするべき箇所**：

1. **[Server names](https://nginx.org/en/docs/http/server_names.html)** — `server_name` がどう解決されるか
2. **[Location](https://nginx.org/en/docs/http/ngx_http_core_module.html#location)** — `location` の優先順位 (`=` > `^~` > regex > prefix longest)
3. **`root` vs `alias`** — [`root` 解説](https://nginx.org/en/docs/http/ngx_http_core_module.html#root) と [`alias` 解説](https://nginx.org/en/docs/http/ngx_http_core_module.html#alias) の **挙動の違い**

それ以上 (upstream / proxy_pass / fastcgi / ssl / cache) は webserv subject の範囲外。**1〜2 時間で十分**。深入りすると subject から逸れる。

### `root` vs `alias` の違い (頻出の罠)

```nginx
location /api { root  /srv; }   # request /api/foo → file /srv/api/foo
location /api { alias /srv; }   # request /api/foo → file /srv/foo
```

- **`root`**: target 全体を root の下に置く (パスを **足す**)
- **`alias`**: location の prefix を root に **置き換える**

webserv subject の config は `root` 1 種類しかないが、**どちらのセマンティクスで実装するかは設計判断**。`alias` の方が直感的なので採用する実装が多い。

---

## 7. webserv が扱わないもの (subject 範囲外)

- HTTPS / TLS 終端
- HTTP/2, HTTP/3
- WebSocket
- reverse proxy / upstream
- load balancing
- caching layer
- アプリケーションフレームワーク的なルーティング (controller 振り分け)
- 認証・セッション管理

これらは **「nginx ならやるけど webserv ではやらない」** ゾーン。実装したくなっても subject 外なので保留する。

---

## 8. まとめ

- **webserv = nginx を作るプロジェクト**。backend は作らない
- **CGI が webserv の「backend 境界」**。ここから先は別プロセス
- **`rooting/` モジュール = nginx routing** (= location マッチ + dispatch 決定)。backend routing ではない
- routing は **純粋な決定ロジック**。I/O はしない
- nginx 公式は location / root / alias の 3 箇所だけ参考にすればよい

設計に迷ったら「これは nginx 側？ それとも CGI 側？」と自問すれば、責務の置き場所が見える。
