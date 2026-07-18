# ft_irc Mandatory 設計書索引

## 設計書

1. `01_overall_design_ja.md`
   - システム全体、要件、アーキテクチャ、主要ライフサイクル
2. `02_class_detailed_design_ja.md`
   - クラスのメンバ変数、API、所有権、不変条件、ファイル構成
3. `03_network_buffer_detailed_design_ja.md`
   - socket、poll、accept、recv、send、Buffer、切断処理
4. `04_command_detailed_design_ja.md`
   - PASS、NICK、USER、JOIN、PRIVMSG、KICK、INVITE、TOPIC、補助Command
5. `05_mode_detailed_design_ja.md`
   - MODE解析、i、t、k、o、l、通知とError
6. `06_error_reply_detailed_design_ja.md`
   - Prefix、Numeric Reply、Command通知、入力・system call Error

## 補助文書

- `decisions.md` — 設計判断 (DD-001〜DD-013)
- `requirements_extract.md` — 課題要件の抜粋
- `tasks.md` — 設計・実装タスク
- `01_overall_design.md` — 全体設計の英語版

## PDF

各Markdownに対応する日本語PDFをリポジトリ直下へ配置する。

Markdownが生成元である。Markdownを修正したらPDFを再生成すること。PDFだけを修正しない。

## 今後作成する文書

- テスト仕様書
- 開発運用書

## 対象外

- ファイル転送Bonus
- Bot Bonus

BonusはMandatory実装と検証の完了後に別途設計する。
