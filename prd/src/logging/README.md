# logging/ — ログ出力

開発・デバッグ用のログ出力ヘルパ群。現状は config パーサのダンプ機能のみ。

> 全体設計は [`docs/architecture.md`](../../../docs/architecture.md) / [`docs/design.md`](../../../docs/design.md) を参照。

## 役割の境界

| 含むもの | 含まないもの |
|---|---|
| `Conf` の中身を整形してダンプ（開発時の動作確認） | 構造化ログ（JSON / level-based） |
| HTTP リクエスト/レスポンスのトレース（将来予定） | エラーハンドリング本体（→ 各レイヤ） |
| 接続イベントのログ（将来予定） | ファイル出力（現状は `std::cout` 直書き） |

## ファイル一覧

| ファイル | 行数 | 状態 | 責務 |
|---|---:|---|---|
| `logging.hpp` | 16 | 🟡 | `print_all_data(Conf&)` の宣言と「TCP/IP 用ログのヘルパを置く予定」のコメント |
| `logging.cpp` | 57 | ✅ | `print_all_data` 実装（ServerConfig と LocationConfig の全フィールドを `std::cout` に出力） |

## 公開 API

```cpp
void print_all_data(const Conf &conf);
```

`Conf` を受け取って、各 `ServerConfig` と内部の `LocationConfig` を **人間可読な形式** で標準出力にダンプする。`main.cpp` で `parsing_args` の直後に呼ばれて、config パースの結果確認に使われている。

### 出力例

```
--- server 0:
   name = example.com
   listen = 8080
   root = /var/www/html
   index = index.html
   body = 1m
   listing = on
   method =    GET POST
   error pages:
   error 404 = /404.html
   - location 0:
       dir = /
       root = /var/www/html
       index = index.html
       listing = off
       redir =
       methods = GET
```

## 他モジュールとの関係

- **依存先**:
  - `persing/persing_conf.hpp` → `Conf`, `ServerConfig`, `LocationConfig`
- **依存元**:
  - `main.cpp` → `parsing_args` 後の動作確認

logging は他レイヤから **片方向に呼ばれる** だけで、ここから他レイヤを呼ぶことはない。レイヤ依存ループを作らない設計。

## 設計判断

| 判断 | 理由 |
|---|---|
| **自由関数のみ** | 状態を持たないので class にする意義がない |
| **`std::cout` 直書き** | 開発フェーズの簡便性。将来必要に応じてストリームを引数化 |
| **`const Conf&` を受ける** | Conf の中身を変更しないことを型で明示 |

## 現状と TODO

- ✅ config ダンプ
- ❌ HTTP リクエスト / レスポンスのトレースログ（`logging.hpp` の `helper for logging TCP/IP connecting` というコメントが残されている）
- ❌ 接続イベント（accept / close）のログ
- ❌ ログレベル（DEBUG / INFO / WARN / ERROR）の導入
- ❌ stderr / stdout の使い分け
- ❌ ファイル出力（`prd/log/` ディレクトリに書き出す）
- 📝 命名小考：本リポジトリは `persing` (parsing) など typo が多いが、`logging` は OK

## 設計指針（拡張時の方針）

ログ拡張するときは以下を意識する：

1. **「何を起きたか」と「どのレイヤから」を両方残す**（例：`[server] new client fd=4`）
2. **出力先を将来切り替えられるようにストリーム引数化**（`std::ostream&` を受ける関数）
3. **`Conf` の中身をいじる動機が出たら、それは logging の範囲外**（logging はリードオンリーで居続ける）
4. **CGI のような外部プロセス出力との衝突に注意**（CGI が stdout に吐くので、サーバログは stderr 推奨かも）

## 関連ドキュメント

- [`docs/architecture.md`](../../../docs/architecture.md) — webserv 全体の責務分離
- [`docs/design.md`](../../../docs/design.md) — クラス図 / リクエストライフサイクル
- [`../persing/README.md`](../persing/README.md) — ダンプ対象の `Conf` / `ServerConfig` / `LocationConfig` の構造
