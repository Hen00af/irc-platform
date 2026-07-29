# Webserv

42Tokyoのカリキュラム向けに作成した、C++98製のNginx風のWebサーバプロジェクトです。


# 概要
このサーバはシンプルなステートマシンをベースにしています。
各サーバは locationのリストを持ちます。　各locationはdirectiveのリストを持ちます。魯クエストを受け取ると、サーバはリクエストURIがどの location に一致するかを探します。
一致する location　が見つかると、その location　に定義された directive をリクエストに通用します。


# 開発環境のセットアップ

`clang-format` と `cpplint` を使ってコードスタイルをチェック・自動整形します。
初回のみ、以下のスクリプトを実行してください（macOS / Linux 対応、冪等）。

```sh
cd prd
./setup.sh
```

このスクリプトは:
- `clang-format` を **brew (macOS)** か **apt (Linux)** で導入
- `cpplint` を **pip3** で導入
- 設定ファイル (`.clang-format`, `CPPLINT.cfg`, `lint.sh`) の存在を確認
- すでに入っているものはスキップ

設定ファイル:
- `.clang-format` — Google ベースで既存スタイルに合わせて調整 (4 スペース・Allman ブレース・行幅 100)
- `CPPLINT.cfg` — Google C++ Style Guide のうち本プロジェクトに合わないルールを除外


# Lint / フォーマットの使い方

```sh
cd prd

./lint.sh          # チェックのみ (整形不要なファイル一覧 + cpplint 警告)
./lint.sh fix      # clang-format で in-place 整形
```

対象は `src/` と `mock/` 配下の `*.cpp` / `*.hpp`。
`tmp/` `lab/` `sample/` などの実験ディレクトリは対象外です。

**初回 `./lint.sh fix` は大量の差分が出るので、別コミットに分けて入れることを推奨します。**

