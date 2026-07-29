# ft_irc Mandatory コマンド詳細設計書

## 1. 文書の目的と対象コマンド

本書は、IRC Command DispatcherとMODE以外のCommand Handlerを実装するための詳細設計を定義する。

Mandatory:

- `PASS`
- `NICK`
- `USER`
- `JOIN`
- `PRIVMSG`
- `KICK`
- `INVITE`
- `TOPIC`

実クライアント互換の補助Command:

- `PING`
- `PONG`
- `QUIT`
- `PART`
- `CAP`

`MODE`はMODE詳細設計書で定義する。

## 2. Message入力形式

ParserはTrailingも含めたすべてのParameterを`Message::params`へ格納する。

```cpp
struct Message
{
    std::string                 prefix;
    std::string                 command;
    std::vector<std::string>    params;
};
```

例:

```text
USER alice 0 * :Alice Example
```

```text
command = "USER"
params  = ["alice", "0", "*", "Alice Example"]
```

Clientが送信したPrefixは認証情報として使用しない。送信元は常にsocket FDから特定する。

## 3. Dispatcher

```cpp
void Server::dispatchCommand(
    int fd,
    const Message &message
);
```

Command名はParserがASCII大文字へ変換済みとする。

Dispatcher表:

| Command | 登録前 | 登録後 | Handler |
|---|---|---|---|
| `PASS` | 可 | 462 | `handlePass` |
| `NICK` | 可 | 可 | `handleNick` |
| `USER` | 可 | 462 | `handleUser` |
| `CAP` | 可 | 可 | `handleCap` |
| `PING` | 可 | 可 | `handlePing` |
| `PONG` | 可 | 可 | `handlePong` |
| `QUIT` | 可 | 可 | `handleQuit` |
| `JOIN` | 451 | 可 | `handleJoin` |
| `PRIVMSG` | 451 | 可 | `handlePrivmsg` |
| `KICK` | 451 | 可 | `handleKick` |
| `INVITE` | 451 | 可 | `handleInvite` |
| `TOPIC` | 451 | 可 | `handleTopic` |
| `MODE` | 451 | 可 | `handleMode` |
| `PART` | 451 | 可 | `handlePart` |
| その他 | 登録前は無視 | 421 | なし |

登録前に未知Commandを無視するのは、クライアント固有の初期交渉で登録を妨げないためである。登録後は`ERR_UNKNOWNCOMMAND`を返す。

## 4. Handler共通処理

共通検証関数:

```cpp
bool requireRegistered(int fd);
bool requireParams(int fd,
                   const Message &message,
                   std::size_t count);
bool requireChannelMember(int fd,
                          const Channel &channel);
bool requireChannelOperator(int fd,
                            const Channel &channel);
```

Handler処理順序:

1. Client存在確認
2. 登録状態確認
3. 必須Parameter数確認
4. 文字列形式の検証
5. 対象ClientまたはChannel検索
6. 参加状態と権限確認
7. 状態変更
8. Replyまたは通知をqueue

状態変更前に失敗した場合は、部分的な変更を残さない。

## 5. Client登録フロー

登録完了条件:

```text
passwordAccepted
&& nickname設定済み
&& USER受信済み
```

`PASS`、`NICK`、`USER`の受信順序は固定しない。ただし登録完了はPASS成功後だけとする。

各Handlerの最後で次を呼ぶ。

```cpp
void Server::tryRegisterClient(int fd);
```

```cpp
if (client.tryCompleteRegistration())
    sendWelcomeSequence(client);
```

図の元データ:

- `../diagrams/registration_command_flow_ja.mmd`

Welcome Sequence:

```text
001 RPL_WELCOME
002 RPL_YOURHOST
003 RPL_CREATED
004 RPL_MYINFO
422 ERR_NOMOTD
```

## 6. PASS

形式:

```text
PASS <password>
```

前提:

- 未登録Client

検証と処理:

1. `params.size() >= 1`を確認する
2. 登録済みなら462を返す
3. `params[0]`とServer passwordを完全一致で比較する
4. 不一致なら464を返し、接続は維持する
5. 一致なら`Client::acceptPassword()`を呼ぶ
6. `tryRegisterClient()`を呼ぶ

成功時に単独Replyは送らない。

エラー:

| 条件 | Numeric |
|---|---|
| Parameterなし | 461 `PASS :Not enough parameters` |
| 登録済み | 462 `:Unauthorized command (already registered)` |
| Password不一致 | 464 `:Password incorrect` |

## 7. NICK

形式:

```text
NICK <nickname>
```

Nickname検証:

- 1文字以上9文字以下
- 先頭はASCII英字または`[]\`_^{|}`のいずれか
- 2文字目以降は先頭許可文字、ASCII数字、`-`を許可
- 空白、comma、colon、CR、LF、NULを許可しない
- 重複比較には`ircCaseFold()`を使用する

処理:

1. Parameter存在確認
2. Nickname形式確認
3. 正規化Nicknameが他Clientに使用されていないことを確認
4. 旧Nicknameと登録状態を保存
5. ServerのNickname索引を更新
6. ClientのNicknameを更新
7. 登録済みなら共有ChannelのClientへNICK通知
8. 未登録なら`tryRegisterClient()`を呼ぶ

登録後の通知:

```text
:<oldnick>!<user>@<host> NICK :<newnick>
```

通知先:

- 自分自身
- 1つ以上のChannelを共有するClient
- 複数Channelを共有していても各FDへ1回だけ

エラー:

| 条件 | Numeric |
|---|---|
| Parameterなし | 431 |
| 形式不正 | 432 `<nick> :Erroneous nickname` |
| 重複 | 433 `<nick> :Nickname is already in use` |

## 8. USER

形式:

```text
USER <username> <mode> <unused> :<realname>
```

前提:

- 未登録Client
- USER未受信

検証:

- Parameterが4個以上
- usernameが空でない
- usernameに空白、`@`、CR、LF、NULを含まない
- realnameは空でも許可する

処理:

1. 登録済みまたはUSER受信済みなら462
2. `params[0]`をUsernameへ保存
3. 最後の`params[3]`をRealnameへ保存
4. modeとunusedは保存せず無視する
5. `tryRegisterClient()`を呼ぶ

エラー:

| 条件 | Numeric |
|---|---|
| Parameter不足 | 461 `USER :Not enough parameters` |
| 再実行 | 462 |

## 9. CAP

実クライアントのCapability Negotiationを最低限終了させる。

形式例:

```text
CAP LS 302
CAP END
```

処理:

| Subcommand | 応答 |
|---|---|
| `LS` | `:<server> CAP * LS :` |
| `REQ` | `:<server> CAP * NAK :<requested>` |
| `END` | 応答なし |
| その他 | 応答なし |

本設計では提供Capabilityは0件である。CAP状態は登録完了条件に含めない。

## 10. JOIN

形式:

```text
JOIN <channel>{,<channel>} [<key>{,<key>}]
```

補助形式:

```text
JOIN 0
```

対応範囲:

- comma区切りの複数Channel
- 同じ位置のcomma区切りKey
- `JOIN 0`による全Channel退出

Channel名検証:

- `#`で始まる
- 2文字以上50文字以下
- 空白、comma、colon、BELL、CR、LF、NULを含まない
- Map検索には正規化名を使用する

Channelごとの処理:

1. Channel名を検証する
2. Clientが既に参加済みならそのChannelは何もせず次へ進む
3. Channelが存在しない場合は生成する
4. 既存Channelなら`+i`、`+k`、`+l`を順に検証する
5. Invite-onlyでInviteされていなければ473
6. Key不一致なら475
7. Limit到達なら471
8. ChannelへMember FDを追加する
9. Clientへ正規化Channel名を追加する
10. 新規ChannelならClientをOperatorへ追加する
11. Inviteを消費して削除する
12. 全MemberへJOIN通知を送る
13. 参加ClientへTopic Replyを送る
14. 参加ClientへNames Replyを送る

新規Channel生成に失敗したChannel名はServerへ残さない。

JOIN通知:

```text
:<nick>!<user>@<host> JOIN :<channel>
```

Topic:

```text
331 <nick> <channel> :No topic is set
```

または:

```text
332 <nick> <channel> :<topic>
```

Names:

```text
353 <nick> = <channel> :@operator normalUser
366 <nick> <channel> :End of NAMES list
```

Operator Nicknameには`@`を付ける。Nickname順序はFDの昇順とする。

図の元データ:

- `../diagrams/join_command_detail_ja.mmd`

エラー:

| 条件 | Numeric |
|---|---|
| Parameter不足 | 461 |
| Channel名不正 | 403 |
| Inviteなし | 473 |
| Key不一致 | 475 |
| Limit到達 | 471 |

複数Channel指定では、1Channelの失敗で他Channel処理を中止しない。

## 11. PRIVMSG

形式:

```text
PRIVMSG <target>{,<target>} :<text>
```

target:

- `#`で始まる場合はChannel
- それ以外はNickname

処理:

1. target Parameterがなければ411
2. text Parameterがない、または空なら412
3. target一覧をcommaで分割する
4. 各targetを独立して処理する

Client宛:

1. NicknameでClient検索
2. 存在しなければ401
3. 対象ClientへPRIVMSGをqueue
4. 送信者自身がtargetでも1回queueする

```text
:<sender>!<user>@<host> PRIVMSG <targetNick> :<text>
```

Channel宛:

1. Channel検索
2. 存在しなければ403
3. 送信者がMemberでなければ404
4. 送信者以外の全Memberへqueue

```text
:<sender>!<user>@<host> PRIVMSG <channel> :<text>
```

送信者にはChannel messageを返送しない。

図の元データ:

- `../diagrams/privmsg_command_detail_ja.mmd`

エラー:

| 条件 | Numeric |
|---|---|
| targetなし | 411 |
| textなし | 412 |
| Nicknameなし | 401 |
| Channelなし | 403 |
| Channel未参加 | 404 |

## 12. KICK

形式:

```text
KICK <channel> <nickname> [:<reason>]
```

本設計では1Commandにつき1Channel、1Clientを処理する。

処理:

1. Parameterが2個以上あることを確認
2. Channel検索
3. 実行ClientがChannel Memberであることを確認
4. 実行ClientがOperatorであることを確認
5. 対象NicknameをClient検索
6. 対象ClientがChannel Memberであることを確認
7. reasonがあれば使用し、なければ実行者Nicknameを使用
8. 削除前の全MemberへKICK通知をqueue
9. Channelから対象FDとOperator権限を削除
10. 対象ClientからChannel名を削除
11. Channelが空なら削除

```text
:<operatorPrefix> KICK <channel> <targetNick> :<reason>
```

対象が唯一のOperatorでも自動移譲しない。

エラー:

| 条件 | Numeric |
|---|---|
| Parameter不足 | 461 |
| Channelなし | 403 |
| 実行者が未参加 | 442 |
| 実行者がOperatorでない | 482 |
| 対象Nicknameなし | 401 |
| 対象がChannelにいない | 441 |

## 13. INVITE

形式:

```text
INVITE <nickname> <channel>
```

課題文でOperator固有Commandとして指定されているため、Channel Modeに関係なくOperatorだけが実行できる。

処理:

1. Parameterが2個以上あることを確認
2. 対象Client検索
3. Channel検索
4. 実行ClientがChannel Memberであることを確認
5. 実行ClientがOperatorであることを確認
6. 対象Clientが既にMemberなら443
7. ChannelのInvite集合へ対象FDを追加
8. 実行Clientへ341を返す
9. 対象ClientへINVITE通知をqueue

```text
341 <inviterNick> <channel> <targetNick>
```

```text
:<inviterPrefix> INVITE <targetNick> :<channel>
```

同じClientを再度Inviteした場合も成功扱いとし、通知を再送する。

エラー:

| 条件 | Numeric |
|---|---|
| Parameter不足 | 461 |
| 対象Nicknameなし | 401 |
| Channelなし | 403 |
| 実行者が未参加 | 442 |
| 実行者がOperatorでない | 482 |
| 対象が既に参加 | 443 |

## 14. TOPIC

照会:

```text
TOPIC <channel>
```

変更:

```text
TOPIC <channel> :<topic>
```

処理:

1. Channel Parameter確認
2. Channel検索
3. 実行ClientがMemberであることを確認
4. topic Parameterがなければ照会
5. topic Parameterがある場合は変更

照会:

- Topicが空なら331
- Topicがあれば332

変更:

- `+t`有効かつ実行者がOperatorでなければ482
- 空文字topicはTopic削除
- ChannelのTopicを更新
- 全MemberへTOPIC通知

```text
:<clientPrefix> TOPIC <channel> :<topic>
```

エラー:

| 条件 | Numeric |
|---|---|
| Parameter不足 | 461 |
| Channelなし | 403 |
| 実行者が未参加 | 442 |
| `+t`でOperatorでない | 482 |

## 15. PART

形式:

```text
PART <channel>{,<channel>} [:<reason>]
```

処理:

1. Channel Parameter確認
2. comma区切りでChannel一覧を作る
3. 各Channelを独立処理する
4. Channelがなければ403
5. ClientがMemberでなければ442
6. reasonがなければClient Nicknameを使用
7. 削除前の全MemberへPART通知
8. 双方向関係を削除
9. 空Channelを削除

```text
:<clientPrefix> PART <channel> :<reason>
```

## 16. QUIT

形式:

```text
QUIT [:<message>]
```

処理:

1. messageがあれば使用する
2. なければ`Client Quit`を使用する
3. `disconnectClient(fd, message)`を即時実行せず、切断理由を保存して切断予約する
4. 現在のCommand列の残りを処理しない

QUIT通知は共有Channelの各Clientへ1回だけ送る。

```text
:<clientPrefix> QUIT :<message>
```

QUITを送ったClientへQUIT通知は送らない。

## 17. PING

形式:

```text
PING <token>
```

登録前でも処理する。

Parameterがなければ409を返す。

成功:

```text
:<serverName> PONG <serverName> :<token>
```

`PING`受信時にClient状態を変更しない。

## 18. PONG

形式:

```text
PONG <token>
```

本設計ではServerから定期PINGを送らないため、Parameterの有無にかかわらず受信して無視する。

将来keepaliveを追加する場合に備えHandlerを独立させる。

## 19. Commandごとの状態変更表

| Command | Client変更 | Channel変更 | Server索引変更 |
|---|---|---|---|
| PASS | Password状態 | なし | なし |
| NICK | Nickname | なし | Nick索引 |
| USER | Username/Realname | なし | なし |
| JOIN | joinedChannels | Member/Operator/Invite | Channel生成 |
| PRIVMSG | なし | なし | なし |
| KICK | 対象joinedChannels | Member/Operator | 空Channel削除 |
| INVITE | なし | Invite | なし |
| TOPIC | なし | Topic | なし |
| PART | joinedChannels | Member/Operator | 空Channel削除 |
| QUIT | 削除 | 全関係削除 | Nick/Client/FD削除 |
| PING/PONG/CAP | なし | なし | なし |

## 20. 通知先表

| Event | 送信先 |
|---|---|
| 登録完了 | 登録Client |
| NICK変更 | 自分と共有Channel Client |
| JOIN | 参加後の全Member |
| Channel PRIVMSG | 送信者以外の全Member |
| Direct PRIVMSG | 対象Client |
| KICK | 削除前の全Member |
| INVITE成功Numeric | 実行Client |
| INVITE通知 | 対象Client |
| TOPIC変更 | 全Member |
| PART | 削除前の全Member |
| QUIT | 共有Channel Clientへ重複なし |

## 21. 実装完了条件

- 登録順序が前後してもPASS、NICK、USERが揃った時点で1回だけ登録完了する
- Nicknameの重複比較がIRC case mappingで行われる
- JOINでChannelが自動生成され最初のClientがOperatorになる
- JOIN成功後にJOIN、Topic、Namesが送信される
- PRIVMSGがDirectとChannelの両方で動作する
- KICK、INVITE、TOPICが権限検証後に状態を更新する
- PART、QUIT、異常切断が同じ関係削除処理を共有する
- 複数Channelまたはtarget指定の一部失敗が他要素を停止させない
- 失敗時に部分的な状態変更が残らない
- 補助Commandにより参照IRCクライアントの登録と接続維持を妨げない

