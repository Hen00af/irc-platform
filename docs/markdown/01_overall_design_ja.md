# ft_irc Mandatory 全体設計書

## 1. 文書の目的と対象範囲

本書は、`ft_irc` 課題のMandatory部分を実装するための全体設計を定義する。

詳細設計書と合わせて参照することで、実装時に大きな設計判断を残さず、設計内容をC++98のコードへ落とし込める状態を目標とする。

要件の参照元:

- ft_irc subject Version 11.0

### 対象範囲

- C++98によるIRCサーバー
- 複数IRCクライアントの同時接続
- TCP/IPソケット通信
- ノンブロッキングI/O
- 1つの`poll()`を中心としたイベントループ
- 接続認証とユーザー登録
- チャンネルの生成、参加、管理、削除
- ユーザー宛およびチャンネル宛メッセージ
- 課題で指定されたOperatorコマンド

### 対象外

- IRCクライアントの実装
- IRCサーバー間通信
- SSL/TLS
- データベースやファイルへの永続化
- GUI
- ファイル転送Bonus
- Bot Bonus

## 2. 課題要件の要約

プログラム名:

```text
ircserv
```

実行形式:

```bash
./ircserv <port> <password>
```

提出対象:

- `Makefile`
- `*.h`
- `*.hpp`
- `*.cpp`
- `*.tpp`
- `*.ipp`
- 必要な場合のみ設定ファイル

Makefileの必須ルール:

- `$(NAME)`
- `all`
- `clean`
- `fclean`
- `re`

必須動作:

- 複数クライアントを同時に処理できること
- `fork()`を使用しないこと
- すべてのソケットをノンブロッキングで扱うこと
- 新規接続、読み込み、書き込みを1つの`poll()`ループで管理すること
- TCP/IPで通信すること
- 実在するIRCクライアントがエラーなく接続できること
- コードが読みやすく保守可能であること

ビルド制約:

- `c++`を使用する
- `-Wall -Wextra -Werror`を使用する
- C++98に準拠する
- `-std=c++98`を追加してもコンパイルできること
- 外部ライブラリとBoostを使用しないこと

使用可能な外部関数:

```text
socket, close, setsockopt, getsockname,
getprotobyname, gethostbyname, getaddrinfo,
freeaddrinfo, bind, connect, listen, accept,
htons, htonl, ntohs, ntohl, inet_addr, inet_ntoa,
inet_ntop, send, recv, signal, sigaction,
sigemptyset, sigfillset, sigaddset, sigdelset,
sigismember, lseek, fstat, fcntl, poll
```

課題では`select()`、`kqueue()`、`epoll()`などの`poll()`相当機能も許可されているが、本設計では`poll()`を採用する。

macOS固有の制約:

```cpp
fcntl(fd, F_SETFL, O_NONBLOCK);
```

macOSでは、ノンブロッキング設定のために上記形式の`fcntl()`のみを使用する。

Mandatory IRC機能:

- 認証と登録: `PASS`、`NICK`、`USER`
- チャンネル参加: `JOIN`
- メッセージ: `PRIVMSG`
- 権限: 一般ユーザーとチャンネルOperator
- Operatorコマンド: `KICK`、`INVITE`、`TOPIC`、`MODE`
- チャンネルMode: `i`、`t`、`k`、`o`、`l`

実クライアントとの安定動作のために追加する補助コマンド:

- `PING`
- `PONG`
- `QUIT`
- `PART`

これらはMandatoryの中心機能ではないが、実在するIRCクライアントとの接続維持や正常な退出処理に必要となる可能性が高いため、実装対象に含める。

## 3. システム概要

本課題で実装するのはIRCサーバーのみである。

```text
IRCクライアント
      |
      | TCP/IPソケット
      v
   ircserv
```

サーバーは接続したクライアントからテキスト形式のIRCコマンドを受信し、内部状態を更新した後、IRC形式のReplyまたは転送メッセージを送信する。

サーバーの主要責務:

- TCPクライアント接続の受付
- 接続中Clientの管理
- 存在するChannelの管理
- IRCコマンドの解析
- コマンド処理の実行
- Replyとブロードキャストの送信
- 切断時のClient、Channel、FDの整理

図の元データ:

- `../diagrams/system_context_ja.mmd`

## 4. アーキテクチャ方針

サーバーはイベント駆動型アーキテクチャを採用する。

メインループは`poll()`を繰り返し、OSから通知されたイベントに応じて処理を行う。

- listen socketが読み込み可能: 新しいClientを`accept()`する
- Client socketが読み込み可能: データを`recv()`する
- Client socketが書き込み可能: 送信バッファを`send()`する
- Client socketにエラーまたは切断通知: Clientを切断処理する

主要な設計判断:

- `Server`を全体状態の所有者とする
- `Client`はサーバー内の接続ユーザー1人を表す
- `Channel`はIRCチャンネル1つを表す
- `Parser`は受信行を構造化された`Message`へ変換する
- `Reply`はIRC形式の送信文字列を生成する
- ClientとChannelは相互にIDだけを保持する
- 中間テーブルまたはMembershipクラスは使用しない
- ClientとChannel間で生ポインタを保持しない
- ClientとChannelの関係変更は必ず`Server`を経由する

## 5. データ所有と参照方針

`Server`がすべての`Client`と`Channel`オブジェクトを所有する。

```cpp
std::map<int, Client> clients;
std::map<std::string, Channel> channels;
```

`clients`のキーにはClient socketのFDを使用する。

`channels`のキーには正規化済みチャンネル名を使用する。

ClientとChannelの関係:

```text
Client
  joinedChannels: set<string>

Channel
  members: set<int>
  operators: set<int>
  invited: set<int>
```

オブジェクトへのポインタではなくIDを保存する。

- Clientは参加中のチャンネル名を保存する
- ChannelはClientのFDを保存する

この方針を採用する理由:

- ダングリングポインタを避ける
- 手動のメモリ所有関係を増やさない
- オブジェクト検索と状態整合性の責務を`Server`へ集約する
- ClientおよびChannelの削除順序を安全にする
- Clientから所属Channel、Channelから所属Clientを直接逆引きできる

中間テーブルは使用しない。Client側とChannel側のID集合で多対多関係を十分に表現でき、中間テーブルを追加すると管理対象と同期箇所が増えるためである。

## 6. クラス構成

主要クラスと構造体:

| 名前 | 責務 |
|---|---|
| `Server` | ソケット初期化、イベントループ、Client・Channel管理、コマンド振り分け |
| `Client` | 接続ユーザー1人の通信状態とIRC登録情報 |
| `Channel` | チャンネル1つのメンバー、権限、Topic、Mode |
| `Message` | 解析済みIRCコマンド |
| `Parser` | 受信行から`Message`への変換 |
| `Reply` | Numeric Reply、Prefix、通知メッセージの生成 |

図の元データ:

- `../diagrams/class_model_ja.mmd`

## 7. Server設計概要

`Server`はアプリケーション全体の集約ルートとする。

Serverが所有する状態:

- listen socket FD
- 接続パスワード
- 実行中フラグ
- `pollfd`一覧
- Client一覧
- Channel一覧

主な責務:

- サーバーソケットを初期化する
- ソケットをノンブロッキングにする
- `bind()`と`listen()`を実行する
- `poll()`イベントループを実行する
- 新しいClientを受け付ける
- Clientからデータを受信する
- Clientの送信バッファを送信する
- 解析済みコマンドをHandlerへ振り分ける
- ClientとChannelを検索する
- ClientとChannelの所属関係を更新する
- Clientを安全に切断する
- 空になったChannelを削除する

所属関係を変更する処理例:

- `joinChannel(clientFd, channelName)`
- `leaveChannel(clientFd, channelName)`
- `kickClient(operatorFd, targetFd, channelName)`
- `disconnectClient(clientFd)`

ClientとChannelが個別に関係を変更することは許可せず、上記のServer共通処理を通すことで双方向データの整合性を保証する。

## 8. Client設計概要

`Client`はサーバーに接続しているIRCユーザー1人の状態を表す。

IRCクライアントアプリケーション自体を実装するクラスではない。

保持予定の状態:

- FD
- Nickname
- Username
- Realname
- Hostname
- 受信バッファ
- 送信バッファ
- PASS認証成功フラグ
- 登録完了フラグ
- 参加中チャンネル名一覧

登録完了条件:

- `PASS`が成功している
- `NICK`が設定されている
- `USER`が設定されている

Clientが参加中チャンネル名を保存することで、切断時に全Channelを走査せず、実際に参加しているChannelだけを整理できる。

Clientは`Channel*`を保持しない。

## 9. Channel設計概要

`Channel`はIRCチャンネル1つの状態を表す。

保持予定の状態:

- チャンネル名
- Topic
- MemberのFD一覧
- OperatorのFD一覧
- Invite済みClientのFD一覧
- Invite-only Mode
- Topic制限Mode
- チャンネルKey
- ユーザー数Limit

存在しないチャンネル名に最初のClientが`JOIN`した時点でChannelを生成する。

Channelを作成した最初のClientをOperatorにする。

Memberが0人になった時点でChannelを削除する。

## 10. MessageとParser設計概要

IRCコマンドはテキスト行として受信する。

入力例:

```text
PRIVMSG #general :hello world
```

Parserの出力例:

```text
prefix   = ""
command  = PRIVMSG
params   = ["#general", "hello world"]
```

Trailing parameterも`params`の末尾へ格納する。利用側が通常ParameterとTrailingを区別する必要がないためである。クラス詳細設計書の7章を参照する。

`Parser`の責務は文字列解析だけとし、Client、Channel、Serverの状態を変更しない。

`Message`が保持する情報:

- 任意のPrefix
- Command
- Parameters (Trailing parameterを末尾へ含む)

基本解析ルール:

- コマンド終端のCRLFまたはLFを除去する
- 空白でCommandとParametersを分割する
- 最初に`:`で始まるParameter以降をTrailingとして扱う
- Command名を大文字へ正規化する
- Trailing内の空白はIRCルールに従って保持する
- 不正または空の入力は状態を変更せずに破棄またはエラー応答する

## 11. ネットワークとイベントループ設計概要

サーバーはTCPソケットを使用する。

初期化フロー:

```text
socket()
  -> setsockopt(SO_REUSEADDR)
  -> fcntl(O_NONBLOCK)
  -> bind()
  -> listen()
  -> poll()
```

ノンブロッキング設定は`bind()`より前に行う。`listen()`の後に設定すると、その間に到着した接続をブロッキングのlisten socketで受けることになる。ネットワーク・バッファ詳細設計書の3章を参照する。

イベントループ:

```text
poll()
  |
  +-- listen fd + POLLIN  -> acceptClient()
  +-- client fd + POLLIN  -> receiveFromClient()
  +-- client fd + POLLOUT -> flushSendBuffer()
  +-- error / hangup      -> disconnectClient()
```

重要方針:

- `recv()`は`poll()`が読み込み可能を通知したFDに対してのみ呼ぶ
- `send()`はClientの送信バッファと`POLLOUT`を通じて行う
- `poll()`を経由せずに`recv()`または`send()`を繰り返さない
- 読み書きの次処理判断を`errno`だけに依存しない
- listen socketとすべてのClient socketを同じ`poll()`で監視する

図の元データ:

- `../diagrams/event_loop_ja.mmd`

## 12. バッファ設計概要

TCPはメッセージ単位ではなく、順序付きのバイトストリームを提供する。

1つのIRCコマンドが複数回の`recv()`に分割される場合がある。

複数のIRCコマンドが1回の`recv()`でまとめて届く場合もある。

そのため、各Clientが独立した受信バッファを所有する。

受信処理:

```text
recv()
  -> client.receiveBufferへ追記
  -> CRLFまたはLFで終わる完成行をすべて切り出す
  -> 完成行を1行ずつParserへ渡す
  -> 未完成データは受信バッファへ残す
```

各Clientは送信バッファも所有する。

送信処理:

```text
送信文字列をclient.sendBufferへ追記
  -> POLLOUT監視を有効化
  -> 送信可能な範囲をsend()
  -> 送信済みバイトを削除
  -> 未送信データを保持
  -> 空になったらPOLLOUT監視を無効化
```

この設計により、ノンブロッキングソケットで発生する部分送信に対応する。

## 13. コマンド処理設計概要

コマンド処理はDispatcherを入口として一元管理する。

```text
Message
  -> dispatchCommand()
  -> handlePass()
  -> handleNick()
  -> handleUser()
  -> handleJoin()
  -> handlePrivmsg()
  -> handleKick()
  -> handleInvite()
  -> handleTopic()
  -> handleMode()
```

各Command Handlerは、原則として次の順序で処理する。

1. 登録状態を確認する
2. Parameter数を検証する
3. 必要なClientまたはChannelを検索する
4. 参加状態と権限を確認する
5. Server経由で状態を更新する
6. ReplyまたはChannel通知を送信する

Mandatoryコマンド群:

- 登録: `PASS`、`NICK`、`USER`
- チャンネル参加: `JOIN`
- メッセージ: `PRIVMSG`
- Operator管理: `KICK`、`INVITE`、`TOPIC`、`MODE`

補助コマンド群:

- 接続維持: `PING`、`PONG`
- 退出と整理: `QUIT`、`PART`

代表的なコマンドシーケンス図:

- `../diagrams/join_sequence_ja.mmd`
- `../diagrams/privmsg_sequence_ja.mmd`

## 14. ライフサイクル設計概要

Clientのライフサイクル:

```text
接続
  -> PASS成功
  -> NICK設定
  -> USER設定
  -> 登録完了
  -> Channel参加
  -> QUITまたは通信切断
  -> 削除
```

図の元データ:

- `../diagrams/client_lifecycle_ja.mmd`

Channelのライフサイクル:

```text
未作成
  -> 最初のJOIN
  -> Channel生成
  -> Memberの参加と退出
  -> Memberが0人
  -> Channel削除
```

Client切断時の整理順序:

1. Clientの参加中チャンネル名一覧をコピーする
2. 共有Channelの他Member FDを重複なしで収集する
3. 収集した宛先へQUITまたはPART通知をqueueする
4. 各ChannelからMemberとOperatorを削除する
5. 全ChannelのInvite集合からClient FDを削除する
6. Clientからチャンネル名を削除する
7. 空になったChannelを削除する
8. Nickname索引からClientを削除する
9. `pollfd`一覧からClient FDを削除する
10. Client FDを`close()`する
11. ServerのClient一覧からClientを削除する

通知先の確定と送信は、Channelからの削除より前に行う。削除してから通知すると宛先が残っておらず、QUITが誰にも届かない。ネットワーク・バッファ詳細設計書の18章と19章を参照する。

Invite集合の掃除は参加中Channelだけでなく全Channelを対象とする。参加していないChannelへInviteされたまま切断される場合があるためである。

切断整理図の元データ:

- `../diagrams/quit_cleanup_ja.mmd`

## 15. エラー処理と完成条件

通常操作だけでなく、不正入力や予期しない切断が発生してもサーバーをクラッシュさせない。

主なエラー処理対象:

- 起動引数が不正
- ポート番号が不正
- サーバーソケット初期化に失敗
- システムコールに失敗
- IRCコマンドが不正
- 必須Parameterが不足
- Nicknameが重複
- Passwordが不一致
- 未登録Clientが登録後専用コマンドを実行
- Channelが存在しない
- 対象Clientが存在しない
- Channelに参加していない
- Operator権限が不足
- 送受信エラー
- Clientが予期せず切断
- 受信または送信バッファが上限を超過

Mandatory完成条件:

- `c++ -Wall -Wextra -Werror`およびC++98でビルドできる
- `./ircserv <port> <password>`で起動できる
- 複数Clientを同時に接続できる
- すべてのソケットがノンブロッキングである
- 1つの`poll()`イベントループで接続、読み込み、書き込みを処理する
- 部分受信と複数コマンド一括受信を処理できる
- 部分送信を処理できる
- Mandatoryコマンドをすべて実装している
- `i`、`t`、`k`、`o`、`l`のModeを実装している
- Channelを自動生成し、空になったら削除できる
- ClientとChannelの双方向ID関係が常に一致する
- 選定した実在IRCクライアントから接続できる
- メモリリークがない
- FDリークがない
- 異常切断や不正入力でクラッシュしない

README完成条件:

- リポジトリルートに`README.md`が存在する
- 先頭行が課題指定の42 curriculum文である
- `Description`、`Instructions`、`Resources`を含む
- AIを利用した箇所と方法を説明する
- README本文は英語で記述する

### 次に作成する設計書

本書の次に、以下の詳細資料を作成する。

- クラス詳細設計書
- ネットワーク・バッファ詳細設計書
- コマンド詳細設計書
- MODE詳細設計書
- Error・Reply詳細設計書
- テスト仕様書
- 開発運用書
