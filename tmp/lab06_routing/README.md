# lab06_routing — リクエストを「どの設定」「どこのファイル」に振り分ける

## このlabで学ぶこと

パース済みのリクエスト（`method` / `target` / `Host`）と `server { ... }` 設定群から、

1. **どの server ブロックの設定を使うか**（virtual host 解決）
2. **どの location ブロックの設定を使うか**（path-based dispatch）
3. **そのメソッドが許可されているか**（→ 405 の判定）
4. **target をディスクのどのファイルに対応させるか**（fs_path の合成）

を決めるレイヤを作ります。これが決まると、後段は「`fs_path` を `open` する」「405 ならそれを書く」など、I/O とテンプレ仕事だけになります。

```
            +---------+   parser output      +---------+
  socket -->| parser  |--------------------->|  route  |--> RouteResult
            +---------+   method/target/Host +---------+
                                                  ^
                                  ServerConf[]    |
                                  (config parser) +
```

---

## 動かし方

```sh
cd tmp/lab06_routing
make
./lab06
```

または `bash test.sh`。

---

## 読むときの注目ポイント

### 1. **server マッチは「Host ヘッダ → name 一致 → なければ先頭」**

```cpp
const ServerConf* pick_server(const std::vector<ServerConf>& s,
                              const std::string& host) {
    for (i ...) if (s[i].name == host) return &s[i];
    return &s[0];  // default server
}
```

実 nginx は `(listen ip:port, server_name)` のタプルで決まりますが、lab では「同じポートで listen している前提」に単純化して、`Host` ヘッダだけで選びます。

肝は **「未一致 → 先頭」**。これがあるおかげで `curl http://localhost/` で Host が一致しなくても何かが返る。本番でも `default_server` 概念があるのと同じ。

### 2. **location は「最長プレフィックス一致」**

```cpp
for each location:
    if target.startswith(location.dir) and segment-boundary OK:
        if dir.size() > best.size():
            best = location
```

なぜ最長か：
```
locations: "/" "/api" "/api/v2"
target:    /api/v2/items
```
ここで `/api` も `/api/v2` も両方マッチしてしまうので、**より具体的な方** を勝たせる。lab05 でメソッドを「単純比較」したのと違い、ここは **比較ロジックそのものに意味** がある。

### 3. **`/apix` が `/api` にマッチしてはいけない**

```cpp
if (dir.size() < target.size()
    && dir.back() != '/'
    && target[dir.size()] != '/')
    continue;
```

これがないと `target=/apix` が `dir=/api` に一致してしまう（単純な startswith は **文字列としての** 前方一致しか見ない）。`/api` というロケーションは「`/api/` 配下」を意味するべきなので、**セグメント境界** を確認します。

ありがちなバグ：素朴に `compare(0, dir.size(), dir) == 0` だけで判定して、`/api-internal` のような URL が `/api` の location に吸い込まれる。テスト `"/apix does NOT match /api"` はこれを刺します。

### 4. **methods が空のときは GET だけ許可**

```cpp
if (loc.methods.empty()) return method == "GET";
```

設定ファイルでメソッドが未指定の場合の **デフォルト挙動**。webserv の subject は GET/POST/DELETE のみ扱うので、デフォルトを GET にしておけば「設定し忘れて全許可」みたいな事故が起きない。失敗を安全側に倒す設計です。

### 5. **fs_path 合成 — location.root が空なら server.root**

```cpp
tail = target.substr(location.dir.size());
root = location.root.empty() ? server.root : location.root;
fs_path = join_path(root, tail);
```

```
target=/api/v2/items, location={dir:"/api/v2", root:"/srv/api/v2"}
  -> tail = "/items"
  -> fs_path = "/srv/api/v2/items"
```

nginx 流に言うと **`location.root` が effective root を上書き** する。

注意点：ここでは「target から `location.dir` を **そのまま** 剥がす」やり方を採っています。これは **`alias` ではなく `root` のセマンティクス** と一致するわけではない（厳密にはnginxの`root`は target 全体を後ろにつける）が、lab スコープでは subject の要求を満たす範囲で十分。本実装に組み込むときに `root` と `alias` の差を意識して書き直すこと。

### 6. **status は enum で「次に何をすべきか」を返す**

```cpp
enum RouteStatus {
    ROUTE_OK,                  // -> 普通にハンドラへ
    ROUTE_NOT_FOUND,           // -> 404
    ROUTE_METHOD_NOT_ALLOWED   // -> 405
};
```

lab05 と同じ思想：**呼び出し側が分岐できる粒度** で結果を返す。`bool` だと「なぜ失敗したか」が伝わらず、後段でレスポンスのステータスコードを決められない。

---

## prd/ にどう活かすか

`prd/src/rooting/rooting.cpp` は空ファイルです。lab06 の `routing.hpp/cpp` をそのままコピーするのではなく、

- `prd/src/persing/persing_conf.hpp` の `ServerConfig` / `LocationConfig` を **そのまま受け取る** インタフェースに書き直す（lab の `ServerConf` は簡略化版なので）
- `RequestParser` から `getMethod() / getTarget() / getHeader("Host")` を引いて `route()` に渡す
- I/O ループの「リクエスト完成イベント」で `route()` を呼び、結果を response writer に渡す

ここまでで「server」→「config 解釈」→「parser」→「routing」と一直線につながります。

---

## yourself: 自分で書くときの罠

1. **`target` が `/` だけのとき**：`pick_location` で `dir="/"` に一致するが、`tail = target.substr(1) = ""`。`join_path("/var/www", "")` の挙動を **テストで** 確認する
2. **`location.dir` が末尾スラッシュ付き** (`/api/`) のときと **なし** (`/api`) のとき、セグメント境界判定が両方通ること
3. **HEAD は GET と同じ扱いにする？** — 今回は別物として扱っています。subject 次第。
4. **クエリ文字列** `?a=1`：`target` に含まれる前提で考えるか、上流で剥がすか。lab06 では「上流で剥がした path 部分」を想定（→ パーサ側 or 受信ループでの責務）

---

## 次の lab への橋渡し

lab07 候補：
- **lab07_static_get**：`RouteResult.fs_path` を `open` して、`200 OK` + ヘッダ + body を組み立てるレスポンスビルダ
- **lab08_error_pages**：`ROUTE_METHOD_NOT_ALLOWED` や `ROUTE_NOT_FOUND` を受けて、設定の `error_pages` から該当HTMLを読む
- **lab09_redirect**：`location` に `return 301 ...;` があったときの先回り処理

lab06 が「**設定 + リクエスト → 何をすべきかの決定**」を担い、lab07 以降が「**決定を I/O に落とす**」担当、という分担で進めます。

---

## 後で読む（飛ばしてOK）

- **server マッチを `(listen, server_name)` で正しく** — マルチポート対応時に必要。lab06 は単純化のため Host だけ
- **`root` vs `alias`** — nginx のセマンティクスを厳密に再現したい場合
- **path 正規化** — `..` / `//` / `%2e%2e` 等のディレクトリトラバーサル防御。これは「routing の前段」（target サニタイズ）に置くのが筋
