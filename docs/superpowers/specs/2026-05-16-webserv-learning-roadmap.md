# Webserv 学習ロードマップ

作成日: 2026-05-16
対象: ケンニーさん (`kenny2@ollo.jp`)
最終ゴール: `prd/` を 42 webserv サブジェクト要件を満たす状態まで自力で書き上げ、自分一人でも HTTP サーバを書ける実力を身につける

---

## 1. ロードマップの背景

### 1.1 現状（2026-05-16時点）

`prd/` の完成度は全体の 15〜20% 程度。

| 領域 | 状態 |
|---|---|
| Configパーサ (`persing_conf.cpp`) | 🟡 60%。構造体・読み込みは動く |
| サーバ起動 (`server.cpp`) | 🟡 30%。socket→bind→listen→accept 1接続まで |
| non-blocking I/O + kqueue/poll | 🔴 0%。完全ブロッキング |
| 複数ポート / 複数 server | 🔴 0% |
| HTTPリクエストパース | 🔴 5%。`persing_request.cpp` は構文エラーで未完成 |
| HTTPレスポンス組み立て | 🔴 0%。`server.cpp` 内でコメントアウト |
| GET/POST/DELETE | 🔴 0% |
| 静的ファイル配信 | 🔴 0% |
| ファイルアップロード | 🔴 0% |
| CGI | 🔴 0% |
| ルーティング (`rooting.cpp`) | 🔴 0%。ファイル0行 |
| エラーページ / リダイレクト / autoindex | 🔴 0% |

加えて、`main.cpp:23` の `server(conf)` typo、`persing_request.cpp` の構文エラー、`server.cpp:101` の未定義変数 `sent` により、現状はコンパイルが通らない。

### 1.2 学習方針の合意事項

ユーザーとの対話で以下を確定：

- **スタート地点**: `prd/` を最終ゴールとして据える（フェーズB以降で本格着手）
- **役割分担**: 私は `tmp/` に **参考例** を書く。`prd/` のコードはケンニーさんが自分の手で書く。コピペ厳禁（42 サブジェクトのAIガイドラインに準拠）
- **粒度**: ミニマルステップ（1ステップ＝1コンセプト、30〜80行/ファイル）
- **順序**: Vertical slice — 薄く全層を貫通してから深掘り

---

## 2. ロードマップ全体構造（13ステップ）

各 lab は `tmp/lab0X_<topic>/` 配下の独立ディレクトリ。中身は `main.cpp` + `Makefile` + `README.md` + `test.sh`。

| # | ディレクトリ | 学ぶ核心 | 出来上がるもの |
|---|---|---|---|
| 0 | `lab00_setup` | Makefile・コンパイラフラグ・ディレクトリ規約 | サブジェクト準拠の `Makefile` 雛形 |
| 1 | `lab01_blocking_echo` | `socket/bind/listen/accept/recv/send` | ブロッキング・1接続・固定レスポンス |
| 2 | `lab02_kqueue_echo` | `kqueue/kevent/EVFILT_READ` で複数fd待ち | kqueueベースの複数接続TCP echo |
| 3 | `lab03_nonblock` | `fcntl(O_NONBLOCK)`・EAGAIN・1つの kevent ループに統合 | non-blocking event loop の雛形 |
| 4 | `lab04_min_http` | HTTPレスポンスの構造 (status line + headers + body) | 固定レスポンスを返す HTTP サーバ |
| 5 | `lab05_req_line` | request line のトークン分割 | `GET /x HTTP/1.1` → 3要素構造体 |
| 6 | `lab06_headers` | ヘッダパース（大文字小文字、空行終端） | `std::map<string,string>` 化 |
| 7 | `lab07_routing` | location の最長一致 prefix matching | `(uri, locations) → matched*` |
| 8 | `lab08_static` | root + uri、`open/read/stat`、index、404 | 静的ファイル配信 |
| 9 | `lab09_post` | Content-Length と body 受信 | POST受信 |
| 10 | `lab10_cgi` | `fork/pipe/dup2/execve/waitpid` + CGI env | PHP/Python CGI |
| 11 | `lab11_multi` | 複数 listen socket、conf駆動 | 複数 server / port |
| 12 | `lab12_polish` | デフォルトエラーページ、autoindex、redirect、upload | サブジェクト要件の仕上げ |

**フェーズ分け**:
- **フェーズA（lab00〜04）**: 薄く全層貫通。最小のHTTPサーバを一周させる。
- **フェーズB（lab05〜09）**: 各層を深掘り。ちゃんとしたHTTPサーバに近づく。
- **フェーズC（lab10〜12）**: 仕上げ。残りのサブジェクト要件を埋める。

---

## 3. 各 lab の詳細

### フェーズA：薄く貫通

#### lab00_setup

- **目的**: Makefile とサブジェクト準拠のコンパイラフラグを確立する
- **内容**: `NAME / all / clean / fclean / re` ルール、`-Wall -Wextra -Werror -std=c++98`、placeholder の `main.cpp`
- **学ぶ核心**: 再リンク抑止、`.PHONY`、`*.o` の置き場
- **prd/反映**: `prd/Makefile` を同じ作りに揃える

#### lab01_blocking_echo

- **目的**: 最小のTCPサーバを動かす
- **内容**: 50行で `socket → bind → listen → accept → recv → send → close`。返すのは固定 `HTTP/1.1 200 OK\r\n\r\nhello\n`
- **学ぶ核心**: errno の読み方、`SO_REUSEADDR`、`htons`
- **prd/反映**: 既存 `server.cpp:14-56` の build_connection と照合し、何が省ける/足りないかを確認

#### lab02_kqueue_echo

- **目的**: 複数fdを1つのループで捌く
- **内容**: listen fd と複数 client fd を1つの `kevent()` ループで処理。返答は echo（受け取った文字列をそのまま返す）
- **学ぶ核心**: `EV_SET / EVFILT_READ / EV_ADD`、`kevent()` の戻りループの読み方

#### lab03_nonblock

- **目的**: サブジェクトの厳格な non-blocking ルールに従う
- **内容**: lab02 に `fcntl(F_SETFL, O_NONBLOCK)` と EAGAIN ハンドリングを追加
- **学ぶ核心**: 「準備完了通知後にしか read/write しない」というルール、accept 失敗時の挙動、partial recv の扱い

#### lab04_min_http

- **目的**: HTTPレスポンスの構造を理解する
- **内容**: lab03 上で、受け取ったバイト列に対し **固定の** HTTPレスポンス（`Content-Length` 付き）を返す。パースはまだしない
- **学ぶ核心**: response の3要素構造、`Connection: close` vs `keep-alive`、なぜ `Content-Length` が必須か
- **このステップ完了時点**: kqueueベースで動く固定レスポンスHTTPサーバが完成

### フェーズB：深掘り

#### lab05_req_line

- **目的**: HTTP リクエストの先頭行をパース
- **内容**: `parse_request_line(const std::string&) → struct{method, target, version}`
- **学ぶ核心**: トークン区切りの厳密さ、`HTTP/1.1` 以外をどう拒否するか、400 を返す条件

#### lab06_headers

- **目的**: HTTP ヘッダをマップ化
- **内容**: ヘッダ行を `\r\n` 区切りでパースして `std::map<string, string>` に
- **学ぶ核心**: key の大文字小文字非依存（lower 化）、行折り返しは非対応（RFC上 deprecated）、`Content-Length` と `Transfer-Encoding` の存在チェック、空行 `\r\n\r\n` で終端
- **prd/反映**: 壊れている `prd/src/persing/persing_request.cpp` を置き換える

#### lab07_routing

- **目的**: URI から location を引く
- **内容**: `match_location(uri, locations) → const Location*` を最長 prefix 一致で
- **学ぶ核心**: location の優先順位、`/` location の役割、URI 正規化（`..` の扱い）
- **prd/反映**: 空の `prd/src/rooting/rooting.cpp` にこの関数を置く

#### lab08_static

- **目的**: 静的ファイル配信
- **内容**: root + uri からファイルを開いて返す。index ファイル、403、404、`open/stat/read`
- **学ぶ核心**: サブジェクト「通常ディスクファイルは poll の対象外」の意味、`stat` でディレクトリ判定→index 試行、MIME タイプは最小限（`.html → text/html` だけでOK）

#### lab09_post

- **目的**: POST body の受信
- **内容**: Content-Length ぶん body を読む。body のバッファリング戦略
- **学ぶ核心**: ヘッダ末尾の `\r\n\r\n` 以降が body、`client_max_body_size`、recv が複数回に分かれる前提

### フェーズC：仕上げ

#### lab10_cgi

- **目的**: CGI 実行
- **内容**: `fork/pipe/dup2/execve/waitpid` で PHP-CGI or Python を叩く
- **学ぶ核心**: env 配列の組み立て（`PATH_INFO/SCRIPT_NAME/REQUEST_METHOD/CONTENT_LENGTH`）、stdin に body を流す、stdout を read してレスポンスに変換
- **罠**: fork した子の `close()` 忘れで fd リーク、kqueue 対象外なので waitpid のタイミングに注意

#### lab11_multi

- **目的**: 複数 server / port 対応
- **内容**: 複数 listen socket を1つの kqueue で。conf 駆動
- **学ぶ核心**: server selection（Host ヘッダ）はサブジェクト対象外なので、ここではポートだけで分ければ OK

#### lab12_polish

- **目的**: サブジェクト残要件を埋める
- **内容**: デフォルトエラーページ、autoindex（ディレクトリ一覧HTML生成）、redirect (`return 301 ...`)、ファイルアップロード保存
- **学ぶ核心**: 仕上げ要素は単独だと小さいが、組み合わさると複雑になることを体感

---

## 4. prd/ への統合タイミング

| タイミング | 統合内容 |
|---|---|
| lab04 終了時 | prd/ の event loop を書き直す最初のチャンス（kqueue ベースへ） |
| lab06 終了時 | `prd/src/persing/persing_request.cpp` の壊れた stub を置き換える |
| lab08 終了時 | prd/ で初めて「ブラウザで開いて静的ファイルが見える」状態に |
| lab09 以降 | prd/ を主戦場、tmp/ は試作場所に |

---

## 5. 進め方のルール

### 5.1 私が各 lab で用意するもの

`tmp/lab0X_<topic>/` に以下4点：

1. **`main.cpp`** （+必要に応じて1〜2ファイル）— 動くコード、30〜80行
2. **`Makefile`** — `make` で `./a.out` ができる最小構成
3. **`README.md`** — 「何を学ぶか」「読むときの注目ポイント3つ」「prd/にどう活かすか」「次の lab への橋渡し」
4. **`test.sh`** — `nc 127.0.0.1 <port>` でリクエストを投げて結果を見る再現手順（既存 `prd/src/test/_MonoServer_test.sh` のスタイルを踏襲）

コメントは「なぜそう書くか」が非自明な箇所だけ。「何をしているか」は変数名と構造で読めるように書く。

### 5.2 ケンニーさんがやること

- tmp の参考例を読む
- 動かしてみる（`make && ./a.out`、別ターミナルで `bash test.sh`）
- 質問する／違うやり方を試す
- 納得したら **prd/ に自分の言葉で書く**（コピペ厳禁）
- 「次行く前に詰めたい」と思ったら遠慮なく止める

### 5.3 止まる条件 / 進む条件

- 各 lab の終わりに、私から「次に進む？それとも〜について確認する？」と聞く
- ケンニーさんが prd/ 側に応用できた感触があれば次へ
- 詰まったら lab を追加するか分割する

### 5.4 私が **やらないこと**

- prd/ のコードを直接編集する（明示的なリクエストがあった時だけ）
- ロードマップ外の機能を勝手に足す
- ブラックボックスとしてコードを渡す（必ず読んで質問してもらう前提）

---

## 6. このロードマップで扱わないこと（YAGNI）

- バーチャルホスト（Host ヘッダによる server selection）— サブジェクト対象外
- HTTPS / TLS — サブジェクト対象外
- HTTP/2 — サブジェクト対象外
- Boost や外部ライブラリ — サブジェクト禁止
- chunked transfer encoding — 必要が出てきたら lab10 の後に lab09b として追加検討
- パフォーマンスチューニング — サブジェクト要件「ストレステストで落ちない」を満たした時点で完了とする
