# http/ — HTTP プロトコル層

HTTP プロトコル固有の型と振る舞いを置くディレクトリ。「**HTTP の世界**」に属するもの（Request / Response / Header / Status / MIME など）はここに集約する。

> webserv 全体の責務分離は [`docs/architecture.md`](../../../docs/architecture.md)、システム設計は [`docs/design.md`](../../../docs/design.md) を参照。

## 役割の境界

| 含むもの | 含まないもの |
|---|---|
| HTTP メッセージの型（Request / Response / Header） | 文字列 → 構造体への変換ロジック（→ `persing/`） |
| HTTP ステータスコード、reason phrase | TCP/IP のソケット操作（→ `server/`） |
| MIME type 判定 | location マッチ（→ `rooting/`） |
| Header の値解釈（`Connection: close`, `Transfer-Encoding: chunked` 等） | CGI 起動（→ 将来 `cgi/`） |

## ファイル一覧

| ファイル | 行数 | 状態 | 責務 |
|---|---:|---|---|
| `request.hpp` | 86 | 🟡 ほぼ空（コメントのみ） | HTTP Request 型を置く予定。nginx の最適化スタイル（zero-copy / Range 参照）を取り入れる意図のメモあり |
| `request.cpp` | 1 | 🟡 空（include のみ） | 同上の実装 |

## 現状

このディレクトリは **設計枠だけ存在し、実装は未着手**。

`request.hpp` には開発者メモとして以下のコメントが残っている：

> 処理の根幹に関わる部分。Bitmapや受け取ったバッファを複製することなく、ポインタの位置での参照で管理するなどのnginxが使用している最適化を積極的に取り入れたい。

これは将来の HTTP メッセージ型を **zero-copy 寄りに設計したい** という宣言。現在は同等機能を `persing/persing_request.cpp` が `std::string` ベースで担っているため、http/ への移行は段階的に進める方針が妥当。

## 想定する将来構造

| 予定ファイル | 中身 |
|---|---|
| `request.hpp` / `request.cpp` | HTTP Request 型本体（method / target / version / headers / body） |
| `response.hpp` / `response.cpp` | HTTP Response 型 + ResponseBuilder |
| `status.hpp` | ステータスコード ↔ reason phrase の対応（200 OK / 404 Not Found / 405 ... ） |
| `header.hpp` | Header マップの正規化（大文字小文字 / 多値ヘッダ） |
| `mime.hpp` | 拡張子 → MIME type 判定 |

## 他モジュールとの関係

- **依存先**: なし（プロトコル層なので他に依存しない）
- **依存元（将来）**:
  - `persing/persing_request.cpp` → 文字列をパースして `Request` を組み立てる
  - `rooting/` → `Request` を読んで `RouteResult` を決める
  - 将来の `handler/` → `Response` を組み立てる
  - `server/` → 完成した `Response` をソケットに書く

## TODO

- [ ] `Request` 型の設計（`persing/RequestParser` のフィールドを class として整理）
- [ ] `Response` 型の設計
- [ ] ステータスコード辞書
- [ ] MIME 判定（最低限 `.html` `.css` `.js` `.png` `.jpg` `.txt`）
- [ ] zero-copy 化は **後回し**（[`parser-design-decision`](../../../.claude/projects/-Users-shattori-Documents-Webserv/memory/parser-design-decision.md) — 2 段階方針に従う）

## 関連ドキュメント

- [`docs/architecture.md`](../../../docs/architecture.md) — webserv 全体の責務分離
- [`docs/design.md`](../../../docs/design.md) — クラス図 / リクエストライフサイクル
- [`../persing/persing_request.hpp`](../persing/persing_request.hpp) — 現状の Request パース（将来移管対象）
