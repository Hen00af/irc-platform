# persing/ — パース層

文字列を構造に変換するレイヤ。**設定ファイル** (`nginx.conf`) と **HTTP リクエスト** の両方のパースを担う。

> 「persing」は本来 "parsing" の typo。直近では命名整理は後回しとして残置。
> 全体設計は [`docs/architecture.md`](../../../docs/architecture.md) / [`docs/design.md`](../../../docs/design.md) 参照。

## 役割の境界

| 含むもの | 含まないもの |
|---|---|
| `conf/*.conf` を読んで `vector<ServerConfig>` に変換 | TCP/IP 接続（→ `server/`） |
| 受信した HTTP リクエスト文字列を `method/target/version/headers/body` に分解 | location マッチ（→ `rooting/`） |
| 妥当性検証（メソッド名、ポート番号、directive の重複） | ファイルシステム操作（→ 将来 `handler/`） |
| HTTP プロトコル違反の検出（400 / 505 等の前段） | レスポンス組み立て |

## ファイル一覧

### config パーサ

| ファイル | 行数 | 状態 | 責務 |
|---|---:|---|---|
| `persing_conf.hpp` | 124 | ✅ | `Conf` クラス / `ServerConfig` / `LocationConfig` の宣言、例外クラス一式 |
| `persing_conf.cpp` | 300 | ✅ | `Conf::read_file`, `check_directive`, `stock_data`, `check_data` 等の実装 |
| `persing_conf_util.cpp` | 37 | ⚠️ | `split_words` / `trim` / `my_atoi` / `is_valid_method` / `is_valid_listing_value` 等のヘルパ |
| `persing.cpp` | 13 | ✅ | `parsing_args(argc, argv, conf)` — config パースの入口関数 |
| `_conf.md` | — | 📝 | config パーサ実装メモ |

### HTTP request パーサ

| ファイル | 行数 | 状態 | 責務 |
|---|---:|---|---|
| `persing_request.hpp` | 56 | ✅ | `RequestParser` クラス / `Range` struct / `BadRequest` / `VersionNotSupported` 例外 |
| `persing_request.cpp` | 243 | ✅ | `parseRequest`, `parseRequestLine`, `parseHeaders`, `parseBody`, `isAllowedMethod` |

## 公開 API

### config

```cpp
void parsing_args(int argc, char **argv, Conf &conf);

class Conf {
    void read_file(std::string name);
    void check_directive();
    void stock_data();
    void check_data();
    const std::vector<ServerConfig>& get_servers() const;
};

bool my_atoi(const std::string &str);
std::string trim(const std::string &input);
bool is_valid_method(const std::string &method);
bool is_valid_listing_value(const std::string &value);
```

### HTTP

```cpp
class RequestParser {
    bool parseRequest(const std::string& raw_request);
    std::string getMethod() const;
    std::string getTarget() const;
    std::string getVersion() const;
    std::string getHeader(const std::string& key) const;
};
```

## データの流れ

```
argv[1]
  ↓ Conf::read_file
ファイルを vector<string> _file に格納
  ↓ check_directive / is_directive
directive の妥当性検証
  ↓ stock_data / add_server / stock_server / stock_location
ServerConfig / LocationConfig に値を詰める
  ↓ check_data
最終整合性チェック
  ↓ get_servers()
const vector<ServerConfig>& として server/ へ渡す
```

```
受信バッファ (std::string)
  ↓ RequestParser::parseRequest
_raw_request にコピー保持
  ↓ parseRequestLine / parseHeaders / parseBody
Range で _method / _target / _version / _headers / _body の境界記録
  ↓ getMethod / getTarget / getHeader
呼び出し側にコピーで返却
```

## 他モジュールとの関係

- **依存先**: なし（外部の `<string>` `<vector>` `<map>` `<fstream>` のみ）
- **依存元**:
  - `logging/` → `Conf` を受けて `ServerConfig` をダンプ
  - `server/` → `Conf::get_servers()` を受けて listen ポート決定 + `ServerConfig` を保持
  - `rooting/` → `RequestParser` の出力と `ServerConfig`/`LocationConfig` を突き合わせる

## 既知の問題 / TODO

- ⚠️ **`persing_conf_util.cpp:1` の include**：`#include "conf.hpp"` となっているが該当ファイル存在せず。`persing_conf.hpp` に修正必要
- ⚠️ **`std::stoi` 利用**：`persing_conf.cpp` / `server/` で使用。C++98 環境では使えない（C++11 以降）。`std::atoi` か手書きパーサに置換する
- ⚠️ **`RequestParser` が `server/` から呼ばれていない**：実装はあるが、`server_multi_io.cpp` の応答ハードコード（`"Hello webserv\n"`）を経由していて配線されていない
- 🔧 **HTTP request の zero-copy 化**：[`parser-design-decision`](../../../.claude/projects/-Users-shattori-Documents-Webserv/memory/parser-design-decision.md) に従い 2 段階で `http/` に移管予定
- 📝 **命名**：`persing` → `parsing` のリネームは後回し

## 関連ドキュメント

- [`docs/architecture.md`](../../../docs/architecture.md) — webserv 全体の責務分離
- [`docs/design.md`](../../../docs/design.md) — クラス図 / リクエストライフサイクル
- [`_conf.md`](./_conf.md) — config パーサ実装メモ
- [`../../tmp/lab05_req_line/README.md`](../../../tmp/lab05_req_line/README.md) — request line パースの学習版実装
