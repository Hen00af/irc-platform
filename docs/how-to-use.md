# 使い方

Relay はブラウザから IRC を使うためのチャットです。アカウント登録はなく、
ニックネームとチャンネル名を入れればすぐ参加できます。

公開URL: <https://irc-platform.onrender.com/chat/>

サイト側にも機能の一覧ページがあります:
<https://irc-platform.onrender.com/>

## ブラウザで使う

1. 上のURLを開くと接続ダイアログが出ます。
2. **Nickname** に表示名を入れます。IRCサーバにそのまま送られます。
3. **Channel** は `#lobby` のままで構いません。別の部屋を使うなら書き換えます。
4. **Enter channel** を押すと接続され、入力欄が使えるようになります。

左のレール（Browser → Gateway → IRC）が順に点灯すれば接続完了です。ここが
途中で止まる場合は[うまく動かないとき](#うまく動かないとき)を見てください。

ニックネームはブラウザに保存され、次回から初期値として入ります。

### 入力できる値

| 項目 | ルール | 例 |
| --- | --- | --- |
| ニックネーム | 英字で始まり、英数と `_ - [ ] { } \` ^` が使える。最大9文字 | `alice`, `bob_42` |
| チャンネル | `#` で始まり、英数と `_ - ` のみ。2〜50文字 | `#lobby`, `#dev-team` |
| メッセージ | 1〜420バイト（日本語は1文字3バイト） | |

規則から外れると、送信前にダイアログが赤字で理由を返します。

### 画面の操作

| 操作 | 方法 |
| --- | --- |
| メッセージ送信 | 入力して **Enter** |
| 改行 | **Shift + Enter** |
| チャンネル移動 | ヘッダーの **Change channel**（今のチャンネルは自動でPART） |
| 名前の変更 | ヘッダーの **Change nick** |
| 参加者の確認 | 右側のメンバー一覧（入退室で自動更新） |

### 複数人で試す

1人でも動作確認はできます。別のタブ、別のブラウザ、スマートフォンなどから
同じURLを開き、**違うニックネーム**で同じチャンネルに入ってください。同じ
ニックネームは IRC 側が拒否します。

### 送れないもの

ブラウザからは接続・発言・JOIN・PART・NICK だけが実行できます。`/mode` や
`/kick` のような任意の IRC コマンドはゲートウェイが受け付けません。サーバ
パスワードもブラウザには渡していません。個人宛のダイレクトメッセージは
プロトコル上は可能ですが、画面には用意していません。

連投を防ぐため **5秒あたり12操作**を超えると `Slow down and try again` を
返します。少し待てば解除されます。

## 履歴の扱い

- 発言はチャンネルごとに**最大100件**メモリに保持します。
- チャンネルに参加すると**直近50件**が再生されます。
- **サーバが再起動すると全部消えます**。保存用のデータベースはありません。

Render の無料プランはアクセスが途切れるとインスタンスが停止するため、履歴も
そのタイミングで消えます。意図した設計で、故障ではありません。

## 動作確認ページ

<https://irc-platform.onrender.com/admin/> で Webserv の状態を確認できます。

- **Endpoint board** の **Probe all** で、静的配信・ヘルスチェック・Python
  CGI・PHP CGI の応答コードと所要時間を測ります。
- **Load profile** の負荷生成は localhost からのみ有効です。公開環境では
  無効と表示されますが、プローブは使えます。

ヘルスチェックだけを見るなら <https://irc-platform.onrender.com/health/> と
<https://irc-platform.onrender.com/gateway/health> が使えます。

## ネイティブIRCクライアントから使う

Render は HTTP 以外のポートを公開できないため、**Render のデプロイには irssi
などのIRCクライアントでは接続できません**。ブラウザ経由のみです。

Fly.io にデプロイした場合は 6697 番（TLS）が開きます。

```sh
irssi -c <fly-app>.fly.dev -p 6697 -w <IRC_PASSWORD>
```

## 手元で動かす

Docker があれば3つのプロセスがまとめて起動します。

```sh
IRC_PASSWORD=choose-a-password \
  docker compose -f deploy/docker-compose.yml up --build
```

- チャット: <http://localhost:8080/chat/>
- 管理ページ: <http://localhost:8080/admin/>（負荷生成もここでは使えます）
- ネイティブIRC: `localhost:6667`

停止は `docker compose -f deploy/docker-compose.yml down` です。

## うまく動かないとき

| 症状 | 原因と対処 |
| --- | --- |
| 最初のアクセスが数十秒返らない | 無料プランのインスタンスが停止していて起動中。待てば開きます |
| `The chat gateway is unavailable` | `ALLOWED_ORIGINS` が実際のURLと一致していないと接続が拒否されます。Render の環境変数を確認してください |
| 接続はできるが発言が出ない | ニックネーム重複の可能性。別の名前で入り直してください |
| メッセージ欄が空 | 履歴が消えた直後か、まだ誰も発言していない状態です |
| `Slow down and try again` | レート制限。数秒待ってください |
| `/admin/` のCGIが504 | CPUの少ない環境ではインタプリタ起動が遅く時間切れになります。`WEBSERV_CONFIG=config/deploy.conf` を設定してください |

構成そのものは [`architecture.md`](architecture.md)、デプロイ手順は
[`../README.md`](../README.md) を参照してください。
