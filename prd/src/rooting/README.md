# rooting/ — routing 層

パース済みリクエスト（method / target / Host）と設定 (`ServerConfig[]`) を突き合わせて、**「どのハンドラに渡すか / どのファイルパスか / 405 か」を決める** レイヤ。I/O はしない、ファイルも開かない。

> ディレクトリ名の `rooting` は本来 "routing" の typo（リポジトリ全体での命名整理は後回し）。
> 全体設計は [`docs/architecture.md`](../../../docs/architecture.md) / [`docs/design.md`](../../../docs/design.md) を参照。

## 役割の境界

| 含むもの | 含まないもの |
|---|---|
| `Host` ヘッダ → `ServerConfig` の選択 | リクエスト文字列のパース（→ `persing/`） |
| request target → `LocationConfig` の選択（最長プレフィックス一致） | ソケット I/O（→ `server/`） |
| メソッド許可判定（→ 405 のヒント） | ファイル open / read（→ 将来 `handler/`） |
| `LocationConfig.redir` のリダイレクト判定 | レスポンスの組み立て |
| `root` + tail から fs_path 合成 | error_page の HTML 読み込み |
| **純粋な決定ロジック**（状態なし） | CGI 起動 |

## ファイル一覧

| ファイル | 行数 | 状態 | 責務 |
|---|---:|---|---|
| `rooting.hpp` | — | 🟡 | `RouteStatus` enum / `RouteResult` struct / `route()` 等の宣言（[feat/routing](https://github.com/Hen00af/Webserv/tree/feat/routing) で設計確定済み） |
| `rooting.cpp` | — | 🔴 | 現状は `main()` を持つスクラッチパッド。本実装に書き直す必要あり |

## 設計判断（feat/routing にて確定済み）

| 判断 | 内容 |
|---|---|
| **class でなく free function + struct** | 状態を持たないので class にする意義がない |
| **入力は `vector<ServerConfig>` + `RequestParser&`** | `Conf` 全体を引きずらず、設定の **読み取り済み結果** だけ受ける |
| **戻り値は `RouteResult` + `RouteStatus` enum** | `bool` だと 404 / 405 / 301 が潰れるので 4 値で区別 |
| **`ROUTE_REDIRECT` を含む** | `LocationConfig.redir` の判定は routing 層の責務（response builder ではない） |
| **handler kind (static / CGI) は持たない** | CGI 判定は `fs_path` の拡張子で決まる。dispatch 層の責務 |
| **`RouteResult` は `ServerConfig*` / `LocationConfig*`（値ではない）** | Conf 内 vector が真の所有者。routing は「指紋」を返すだけで複製しない |

## 公開 API（予定）

```cpp
enum RouteStatus {
    ROUTE_OK,
    ROUTE_NOT_FOUND,
    ROUTE_METHOD_NOT_ALLOWED,
    ROUTE_REDIRECT
};

struct RouteResult {
    RouteStatus              status;
    const ServerConfig*      server;
    const LocationConfig*    location;
    std::string              fs_path;
    std::string              redirect_to;
};

const ServerConfig* pick_server(
    const std::vector<ServerConfig>& servers,
    const std::string& host);

const LocationConfig* pick_location(
    const ServerConfig& server,
    const std::string& target);

RouteResult route(
    const std::vector<ServerConfig>& servers,
    const RequestParser& req);

const char* route_status_str(RouteStatus s);
```

## ルーティングルール（lab06 で検証済み）

### Server 選択
1. `Host` ヘッダと一致する `ServerConfig.name` を持つものを返す
2. 一致が無ければ **先頭の `ServerConfig`** を返す（nginx の default_server 相当）

### Location 選択
1. `target` に対する **最長プレフィックス一致** で `LocationConfig.dir` を選ぶ
2. ただし **セグメント境界** をチェック（`/apix` が `/api` にマッチしないように）

### メソッド許可判定
1. `LocationConfig.methods` に request method が含まれる → 許可
2. `methods` が空の場合は **`GET` のみ許可**（安全側デフォルト）

### fs_path 合成
1. `tail = target` から `location.dir` を剥がす
2. `root = location.root.empty() ? server.root : location.root`
3. `fs_path = root + tail`

> `root` と `alias` のセマンティクス差は nginx と完全一致していない。lab06 / docs/architecture.md の「`root` vs `alias`」セクション参照。実装に組み込む際は要再検討。

## 他モジュールとの関係

- **依存先**:
  - `persing/persing_conf.hpp` → `ServerConfig`, `LocationConfig`
  - `persing/persing_request.hpp` → `RequestParser`
- **依存元（将来）**:
  - `server/` → リクエスト受信完了時に `route()` を呼ぶ
  - 将来の `handler/` → `RouteResult` を受けて static/CGI/redirect/error を分岐

## 学習用の小型実装

routing の小型版が [`tmp/lab06_routing/`](../../../tmp/lab06_routing/) にあります（lab スタイル）。簡略化した `ServerConf` で動作確認＋設計判断のリハーサル。本実装に組み込む前に挙動を固めるのに使う。

## 現状と TODO

- ✅ ヘッダの設計確定（`rooting.hpp` on `feat/routing`）
- ✅ lab06 で参考実装（15/15 テスト通過）
- ❌ `rooting.cpp` の本実装（現状はスクラッチパッド `main()`）
- ❌ `prd/persing` の本物の型を受けるインタフェースへの書き直し（lab06 は簡略型）
- ❌ `server/` からの呼び出し配線
- ❌ `root` vs `alias` セマンティクスの確定

## 関連ドキュメント

- [`docs/architecture.md`](../../../docs/architecture.md) — webserv 全体の責務分離（nginx 内 routing と backend 内 routing の区別）
- [`docs/design.md`](../../../docs/design.md) — クラス図 / リクエストライフサイクル
- [`tmp/lab06_routing/README.md`](../../../tmp/lab06_routing/README.md) — 小型実装の設計判断詳説
- nginx 公式 [Location](https://nginx.org/en/docs/http/ngx_http_core_module.html#location) / [root](https://nginx.org/en/docs/http/ngx_http_core_module.html#root) / [alias](https://nginx.org/en/docs/http/ngx_http_core_module.html#alias)
