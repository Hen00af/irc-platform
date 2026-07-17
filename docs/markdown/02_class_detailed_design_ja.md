# ft_irc Mandatory クラス詳細設計書

## 1. 文書の目的と前提

本書は、全体設計で定義したクラス構成をC++98のヘッダーと実装ファイルへ直接落とし込むための詳細設計を定義する。

対象:

- `Server`
- `Client`
- `Channel`
- `Message`
- `Parser`
- `Reply`
- 補助構造体と共通型

対象外:

- socket APIの詳細な呼び出し順序
- 各IRCコマンドの業務ロジック
- MODE文字列の解析詳細
- Numeric Reply本文の全一覧
- Bonus

前提となる設計判断:

- `Server`がすべての`Client`と`Channel`を値として所有する
- Clientは参加中Channelの正規化名を保持する
- ChannelはClient FDを保持する
- 中間テーブルを使用しない
- ClientとChannel間に生ポインタを保持しない
- 関係変更は`Server`の共通処理だけが実行する
- C++98で使用できる標準コンテナを使用する

## 2. 命名・型・可視性規約

クラス名と構造体名はPascalCase、メンバ変数は先頭に`_`を付ける。

```cpp
class Client;
struct Message;
```

```cpp
int _listenFd;
std::string _nickname;
```

Getterは`getXxx()`、真偽値は`isXxx()`または`hasXxx()`、状態変更は動詞で始める。

```cpp
int getFd() const;
bool isRegistered() const;
void appendReceiveBuffer(const char *data, std::size_t length);
```

コンテナの型:

```cpp
typedef std::map<int, Client> ClientMap;
typedef std::map<std::string, Channel> ChannelMap;
typedef std::vector<struct pollfd> PollFdList;
typedef std::set<int> ClientFdSet;
typedef std::set<std::string> ChannelNameSet;
```

IDの意味:

| ID | 保存場所 | 内容 |
|---|---|---|
| Client ID | `int` | OSが`accept()`で返したsocket FD |
| Channel ID | `std::string` | IRC case mappingで正規化したチャンネル名 |
| Nickname ID | `std::string` | IRC case mappingで正規化したNickname |

公開メソッドは、外部から必要な参照と単体状態の変更だけに限定する。ClientとChannelの所属関係を直接変更するメソッドは`Server`からのみ呼ぶ。

## 3. クラス関係と所有権

```text
main
  -> Serverを1つ生成

Server
  -> map<int, Client>を所有
  -> map<string, Channel>を所有
  -> vector<pollfd>を所有
  -> ParserとReplyのstatic機能を利用

Client
  -> Channel名だけを保持

Channel
  -> Client FDだけを保持
```

図の元データ:

- `../diagrams/detailed_class_relationship_ja.mmd`

所有権規則:

- `Server`のデストラクタがlisten FDと残存Client FDを閉じる
- `Client`と`Channel`はFDを所有しないため、デストラクタで`close()`しない
- STLコンテナの要素は値として保持し、手動`new`と`delete`を使用しない
- `Parser`と`Reply`は状態を持たないため、インスタンス化しない

参照の有効期間:

- `std::map`要素への参照は、その要素を`erase()`するまで有効
- Handler内で取得した`Client&`または`Channel&`を、`erase()`後に使用しない
- 切断処理では参加中Channel名一覧をコピーしてから元集合を変更する
- Channel削除の可能性がある処理では、削除後にChannel参照を使用しない

## 4. Serverクラス

### 4.1 責務

- 起動引数から受け取ったportとpasswordを保持する
- listen socketを初期化する
- `pollfd`一覧を管理する
- ClientとChannelを所有する
- NicknameからClient FDへの検索索引を管理する
- イベントループを実行する
- Parserへ受信行を渡す
- Command HandlerへMessageを振り分ける
- ClientとChannelの双方向関係を更新する
- ReplyをClient送信バッファへ追加する
- 切断とChannel削除を安全な順序で実行する

### 4.2 メンバ変数

```cpp
class Server
{
private:
    int                             _listenFd;
    int                             _port;
    std::string                     _password;
    std::string                     _serverName;
    bool                            _running;

    std::vector<struct pollfd>      _pollFds;
    std::map<int, Client>           _clients;
    std::map<std::string, Channel>  _channels;
    std::map<std::string, int>      _nickToFd;
    std::set<int>                   _pendingDisconnects;
};
```

| メンバ | 初期値 | 説明 |
|---|---|---|
| `_listenFd` | `-1` | 接続受付専用FD |
| `_port` | 起動引数 | 1から65535の待受port |
| `_password` | 起動引数 | 接続認証Password |
| `_serverName` | `"ircserv.local"` | PrefixとNumeric Replyで使用 |
| `_running` | `true` | メインループ継続フラグ |
| `_pollFds` | 空 | 先頭要素をlisten FDとする |
| `_clients` | 空 | FDからClientへの所有Map |
| `_channels` | 空 | 正規化Channel名からChannelへの所有Map |
| `_nickToFd` | 空 | 正規化NicknameからFDへの索引 |
| `_pendingDisconnects` | 空 | poll走査後に切断するFD |

### 4.3 ConstructorとDestructor

```cpp
Server(int port, const std::string &password);
~Server();
```

Constructor:

1. メンバを初期化する
2. socket作成は`setupListeningSocket()`で行う
3. 初期化途中で失敗した場合、既に開いたFDを閉じて例外を送出する

Destructor:

1. `_clients`内の全FDを`close()`する
2. `_listenFd >= 0`なら閉じる
3. コンテナは自動破棄に任せる
4. IRC通知の送信は行わない

### 4.4 公開メソッド

```cpp
void run();
void stop();
```

| メソッド | 事前条件 | 事後条件 |
|---|---|---|
| `run()` | listen socket初期化済み | `stop()`または致命的エラーまでイベントループを実行 |
| `stop()` | なし | `_running`を`false`にする |

### 4.5 ネットワーク内部メソッド

```cpp
void setupListeningSocket();
void eventLoop();
void acceptClient();
void receiveFromClient(int fd);
void flushSendBuffer(int fd);
void updatePollEvents(int fd);
void scheduleDisconnect(int fd);
void processPendingDisconnects();
void disconnectClient(int fd, const std::string &reason);
```

詳細な処理順序はネットワーク・バッファ詳細設計書に定義する。

### 4.6 検索メソッド

```cpp
Client *findClientByFd(int fd);
const Client *findClientByFd(int fd) const;
Client *findClientByNickname(const std::string &nickname);
const Client *findClientByNickname(const std::string &nickname) const;
Channel *findChannel(const std::string &name);
const Channel *findChannel(const std::string &name) const;
```

検索に失敗した場合は`NULL`を返す。検索メソッド内で例外を送出しない。

Nickname検索:

```text
nickname
  -> ircCaseFold()
  -> _nickToFd.find()
  -> _clients.find(fd)
```

Channel検索:

```text
channelName
  -> normalizeChannelName()
  -> _channels.find()
```

### 4.7 所属関係メソッド

```cpp
bool joinChannel(int clientFd, const std::string &channelKey);
bool leaveChannel(int clientFd,
                  const std::string &channelKey,
                  const std::string &reason,
                  bool sendPart);
void removeClientFromAllChannels(int clientFd,
                                 const std::string &quitMessage);
void deleteChannelIfEmpty(const std::string &channelKey);
```

`joinChannel()`の更新順序:

1. ClientとChannelの存在を確認する
2. ChannelへClient FDを追加する
3. Clientへ正規化Channel名を追加する
4. どちらか一方の追加が不要だった場合も、最終的に両側が一致することを確認する

`leaveChannel()`の更新順序:

1. 必要な通知文字列を先に生成する
2. ChannelからMember、Operator、Inviteを削除する
3. ClientからChannel名を削除する
4. 通知を送信する
5. Channelが空なら削除する

図の元データ:

- `../diagrams/relationship_update_sequence_ja.mmd`

### 4.8 Nickname索引メソッド

```cpp
bool isNicknameAvailable(const std::string &nickname,
                         int exceptFd) const;
void registerNickname(int fd, const std::string &nickname);
void unregisterNickname(const std::string &nickname);
```

`exceptFd`はNickname変更時に自分自身を重複対象から除外するために使用する。

Nickname変更時:

1. 新Nicknameの正規化Keyを作る
2. 重複を確認する
3. 旧Nicknameの索引を削除する
4. ClientのNicknameを変更する
5. 新Nicknameの索引を追加する

### 4.9 送信メソッド

```cpp
void queueToClient(int fd, const std::string &message);
void broadcastToChannel(const Channel &channel,
                        const std::string &message,
                        int excludeFd);
void broadcastToSharedChannels(const Client &client,
                               const std::string &message);
```

規則:

- `queueToClient()`は末尾がCRLFでない場合にCRLFを追加する
- `queueToClient()`は`send()`を直接呼ばない
- `broadcastToChannel()`はChannelのMember FDを走査する
- `excludeFd == -1`なら除外しない
- 同一Clientへ複数共有Channel経由で通知する場合はFD集合で重複排除する

### 4.10 Command Dispatcher

```cpp
void dispatchCommand(int fd, const Message &message);
```

Handler宣言:

```cpp
void handlePass(int fd, const Message &message);
void handleNick(int fd, const Message &message);
void handleUser(int fd, const Message &message);
void handleJoin(int fd, const Message &message);
void handlePrivmsg(int fd, const Message &message);
void handleKick(int fd, const Message &message);
void handleInvite(int fd, const Message &message);
void handleTopic(int fd, const Message &message);
void handleMode(int fd, const Message &message);
void handlePing(int fd, const Message &message);
void handlePong(int fd, const Message &message);
void handleQuit(int fd, const Message &message);
void handlePart(int fd, const Message &message);
void handleCap(int fd, const Message &message);
```

## 5. Clientクラス

### 5.1 責務

- 1接続分のFDとIRC登録情報を保持する
- 受信途中と送信待ちのバイト列を保持する
- PASS、NICK、USERの登録状態を保持する
- 参加中Channelの正規化名を保持する
- 自身の単体状態だけを変更する

### 5.2 メンバ変数

```cpp
class Client
{
private:
    int                     _fd;
    std::string             _nickname;
    std::string             _username;
    std::string             _realname;
    std::string             _hostname;
    std::string             _receiveBuffer;
    std::string             _sendBuffer;
    bool                    _passwordAccepted;
    bool                    _userReceived;
    bool                    _registered;
    std::set<std::string>   _joinedChannels;
};
```

| メンバ | 接続直後の値 |
|---|---|
| `_fd` | `accept()`が返したFD |
| `_nickname` | 空 |
| `_username` | 空 |
| `_realname` | 空 |
| `_hostname` | 接続元の数値IP |
| `_receiveBuffer` | 空 |
| `_sendBuffer` | 空 |
| `_passwordAccepted` | `false` |
| `_userReceived` | `false` |
| `_registered` | `false` |
| `_joinedChannels` | 空 |

### 5.3 Constructor

```cpp
Client();
Client(int fd, const std::string &hostname);
```

Default Constructorは`std::map::operator[]`依存を避けるため原則使用しない。必要な場合は`_fd = -1`で生成する。

### 5.4 識別情報API

```cpp
int getFd() const;
const std::string &getNickname() const;
const std::string &getUsername() const;
const std::string &getRealname() const;
const std::string &getHostname() const;

void setNickname(const std::string &nickname);
void setUser(const std::string &username,
             const std::string &realname);
```

`setUser()`は`_username`、`_realname`、`_userReceived`を同時に更新する。

### 5.5 登録状態API

```cpp
bool isPasswordAccepted() const;
bool hasNickname() const;
bool hasUser() const;
bool isRegistered() const;

void acceptPassword();
bool tryCompleteRegistration();
```

`tryCompleteRegistration()`:

```text
if registered:
    return false

if passwordAccepted && hasNickname && userReceived:
    registered = true
    return true

return false
```

戻り値`true`は、この呼び出しで初めて登録完了になったことを表す。CallerはWelcome Replyを1回だけ送信する。

### 5.6 Buffer API

```cpp
const std::string &getReceiveBuffer() const;
const std::string &getSendBuffer() const;
bool hasPendingOutput() const;

void appendReceiveBuffer(const char *data, std::size_t length);
void eraseReceivePrefix(std::size_t length);
void appendSendBuffer(const std::string &data);
void eraseSendPrefix(std::size_t length);
void clearBuffers();
```

Clientは行解析を行わない。CRLFの検索と完成行の抽出はServerの受信処理またはBuffer Utilityが担当する。

### 5.7 Channel関係API

```cpp
const std::set<std::string> &getJoinedChannels() const;
bool hasJoinedChannel(const std::string &channelKey) const;
bool addJoinedChannel(const std::string &channelKey);
bool removeJoinedChannel(const std::string &channelKey);
```

戻り値は集合が実際に変更されたかを表す。

## 6. Channelクラス

### 6.1 責務

- 1チャンネル分の表示名と正規化Keyを保持する
- Member、Operator、InviteのFD集合を保持する
- TopicとMandatory Mode状態を保持する
- 単体Channel内の集合操作とMode状態変更を提供する

### 6.2 メンバ変数

```cpp
class Channel
{
private:
    std::string     _name;
    std::string     _keyName;
    std::string     _topic;
    std::set<int>   _members;
    std::set<int>   _operators;
    std::set<int>   _invited;
    bool            _inviteOnly;
    bool            _topicRestricted;
    bool            _keyEnabled;
    std::string     _channelKey;
    bool            _limitEnabled;
    std::size_t     _userLimit;
};
```

| メンバ | 新規Channelの初期値 |
|---|---|
| `_name` | 最初のJOINで指定された表示名 |
| `_keyName` | 正規化Channel名 |
| `_topic` | 空 |
| `_members` | 空 |
| `_operators` | 空 |
| `_invited` | 空 |
| `_inviteOnly` | `false` |
| `_topicRestricted` | `true` |
| `_keyEnabled` | `false` |
| `_channelKey` | 空 |
| `_limitEnabled` | `false` |
| `_userLimit` | `0` |

新規Channelでは`t`を有効にする。最初の参加者をOperatorへ追加する。

### 6.3 Constructor

```cpp
Channel();
Channel(const std::string &name, const std::string &keyName);
```

`keyName`には`IrcUtil::normalizeChannelName(name)`の結果を渡す。Channelから`IrcUtil`へ依存させないため、正規化は呼び出し側で行う。12章の依存方向を維持するためである。

Default ConstructorはClientの5.3と同じ理由で原則使用しない。`std::map::operator[]`は`_name`と`_keyName`が空のChannelを生成し、11章の不変条件4を破る。Channel生成には次を使用する。

```cpp
_channels.insert(std::make_pair(
    keyName,
    Channel(displayName, keyName)
));
```

### 6.4 基本API

```cpp
const std::string &getName() const;
const std::string &getKeyName() const;
const std::string &getTopic() const;
void setTopic(const std::string &topic);
bool hasTopic() const;
```

### 6.5 Member API

```cpp
const std::set<int> &getMembers() const;
std::size_t getMemberCount() const;
bool isEmpty() const;
bool hasMember(int fd) const;
bool addMember(int fd);
bool removeMember(int fd);
```

`removeMember()`は安全のためOperator集合からも同じFDを削除する。Inviteは参加許可情報であり、参加成功時または切断時にServerが明示的に削除する。

### 6.6 Operator API

```cpp
bool isOperator(int fd) const;
bool addOperator(int fd);
bool removeOperator(int fd);
```

`addOperator()`は対象FDがMemberでない場合`false`を返し、追加しない。

最後のOperatorが退出または`-o`された場合、自動昇格は行わない。課題要件にOperator移譲規則がないため、暗黙の権限変更を避ける。

### 6.7 Invite API

```cpp
bool isInvited(int fd) const;
bool addInvite(int fd);
bool removeInvite(int fd);
```

InviteはChannelが存在する間だけ有効とする。Client切断時に全参加Channelだけでなく、全ChannelのInvite集合からFDを削除する。

### 6.8 Mode API

```cpp
bool isInviteOnly() const;
void setInviteOnly(bool enabled);

bool isTopicRestricted() const;
void setTopicRestricted(bool enabled);

bool hasKey() const;
const std::string &getChannelKey() const;
void setChannelKey(const std::string &key);
void clearChannelKey();
bool matchesKey(const std::string &key) const;

bool hasUserLimit() const;
std::size_t getUserLimit() const;
void setUserLimit(std::size_t limit);
void clearUserLimit();
bool isFull() const;
```

Mode文字列生成:

```cpp
std::string buildModeString() const;
std::vector<std::string> buildModeParameters() const;
```

表示順序は`itkl`とする。`o`はChannel全体の単一状態ではなくMemberごとの権限のため、Mode照会の文字列には含めない。MODE詳細設計書の4章と一致させる。

## 7. Message構造体

```cpp
struct Message
{
    std::string                 prefix;
    std::string                 command;
    std::vector<std::string>    params;
};
```

Trailing parameterも`params`の最後へ格納する。Parser利用側が通常ParameterとTrailingを区別する必要はないためである。

例:

```text
入力:
PRIVMSG #general :hello world

結果:
prefix  = ""
command = "PRIVMSG"
params  = ["#general", "hello world"]
```

制約:

- Parameter数は最大15
- CommandはASCII大文字へ正規化する
- Clientから送られたPrefixは信頼せず、Handlerでは使用しない

## 8. Parserクラス

```cpp
class Parser
{
public:
    static bool parse(const std::string &line, Message &out);

private:
    Parser();
};
```

`parse()`の戻り値:

- `true`: Commandを含むMessageを生成した
- `false`: 空行または構文上処理できない行

Parserは以下を行わない:

- Client検索
- 登録状態確認
- Parameter数のコマンド別検証
- Numeric Reply生成
- 状態変更

## 9. Replyクラス

```cpp
class Reply
{
public:
    static std::string clientPrefix(const Client &client);
    static std::string serverPrefix(const std::string &serverName);
    static std::string numeric(const std::string &serverName,
                               const Client &target,
                               int code,
                               const std::string &parameters);
    static std::string command(const std::string &prefix,
                               const std::string &command,
                               const std::string &parameters);

private:
    Reply();
};
```

Client Prefix:

```text
<nickname>!<username>@<hostname>
```

`clientPrefix()`と`serverPrefix()`は先頭colonを付けずに返す。行頭のcolonは`command()`が付与する。Error・Reply詳細設計書の5章の契約に合わせるためである。返り値をそのまま`command()`のprefix引数へ渡せる。

```cpp
Reply::command(Reply::clientPrefix(client), "JOIN", ":#general");
// -> ":alice!a@127.0.0.1 JOIN :#general"
```

Error・Reply詳細設計書の3章がcolon付きで示しているのは、送信される行の形式であり、`clientPrefix()`の返り値ではない。

未登録ClientへのNumeric Replyではtargetを`*`とする。

Replyが返す文字列はCRLFを含めない。CRLF付与を`Server::queueToClient()`へ一元化する。

## 10. 共通Utility

`IrcUtil`はIRCの文字列規則だけを扱う。状態を持たず、`Client`・`Channel`・`Server`のいずれにも依存しない。

```cpp
namespace IrcUtil
{
    const std::size_t IRC_MAX_LINE = 512;
    const std::size_t IRC_MAX_CONTENT = 510;
    const std::size_t NICKNAME_MAX_LENGTH = 9;
    const std::size_t CHANNEL_NAME_MAX_LENGTH = 50;

    std::string toUpperAscii(const std::string &value);
    std::string ircCaseFold(const std::string &value);
    std::string normalizeChannelName(const std::string &value);
    bool isValidNickname(const std::string &value);
    bool isValidChannelName(const std::string &value);
    bool parsePositiveSize(const std::string &value,
                           std::size_t &result);
    bool fitsIrcLine(const std::string &withoutCrlf);
    bool isSafeToken(const std::string &value);
    std::string sanitizeMessageText(const std::string &value);
    std::vector<std::string> splitCommaList(const std::string &value);
}
```

`fitsIrcLine()`・`isSafeToken()`・`sanitizeMessageText()`の用途はError・Reply詳細設計書の13章と17章に定義する。`isSafeToken()`はReplyのPrefix・Command・target検査のほか、Serverが起動時にserverNameを検査するのにも使う。

`parsePositiveSize()`は数字のみ・0以外・overflowなしだけを判定する汎用関数である。MODE `l`の上限100000のようなCommand固有の範囲検査は、呼び出し側のHandlerが行う。

`splitCommaList()`はJOINのChannelとKeyを位置で対応させるため空要素を残す。padding は行わないため、Key一覧がChannel一覧より短くなり得る。呼び出し側が添字の範囲を確認する。

### 10.1 BufferUtil

受信バッファからの行切り出しは責務が異なるため、5.6で言う「Buffer Utility」として分離する。

```cpp
namespace BufferUtil
{
    enum LineStatus
    {
        LINE_INCOMPLETE,
        LINE_EXTRACTED,
        LINE_TOO_LONG
    };

    bool hasCompleteLine(const std::string &buffer);
    LineStatus findLine(const std::string &buffer,
                        std::string &out,
                        std::size_t &consumed);
}
```

`findLine()`はbufferを変更しない。`consumed`へ取り除くべきバイト数を返し、呼び出し側が`Client::eraseReceivePrefix()`で取り除く。行長判定の詳細はネットワーク・バッファ詳細設計書の11章に定義する。

`BufferUtil`は行長定数のためだけに`IrcUtil`へ依存する。

### 10.2 IRC case mapping

`ircCaseFold()`はRFC 2812のIRC case mappingを使用する。

```text
A-Z -> a-z
[   -> {
]   -> }
\   -> |
^   -> ~
```

表示文字列は変更せず、MapとSetの検索Keyだけを正規化する。

## 11. 不変条件

実装中は次の条件を常に維持する。

1. `_clients`のKeyと`Client::_fd`が一致する
2. `_nickToFd`に存在するFDは必ず`_clients`に存在する
3. 登録済みClientのNicknameは空ではない
4. `_channels`のKeyと`Channel::_keyName`が一致する
5. ClientがChannel名を持つなら、該当Channelが存在しMemberにClient FDを持つ
6. ChannelがMember FDを持つなら、Clientが存在し該当Channel名を持つ
7. Operator FDは必ずMember FDでもある
8. 送信バッファが空のClientは`POLLOUT`を監視しない
9. `_pendingDisconnects`内のFDを新しいHandler処理へ渡さない
10. ChannelのMember数が0ならイベント処理終了時までに削除する

Debug用整合性確認:

```cpp
bool Server::checkInvariants() const;
```

本番処理では毎回呼ばず、Debugログまたはテストから使用する。

## 12. ファイル構成

```text
prd/
  main.cpp
  Makefile

  domain/
    Client.hpp / Client.cpp
    Channel.hpp / Channel.cpp
    Message.hpp

  util/
    Parser.hpp / Parser.cpp
    Reply.hpp / Reply.cpp
    IrcUtil.hpp / IrcUtil.cpp
    BufferUtil.hpp / BufferUtil.cpp
    Numerics.hpp

  interface/
    Server.hpp / Server.cpp
    ServerNetwork.cpp
    ServerRelations.cpp
    ServerDispatch.cpp

  handler/
    ServerAuthCommands.cpp
    ServerChannelCommands.cpp
    ServerMessageCommands.cpp
    ServerMode.cpp

tests/
  単体テスト。提出物とは独立したMakefileを持つ
```

ヘッダーと実装ファイルは同じディレクトリへ置く。テスト用`main()`が提出物のビルドへ混入しないよう、`tests/`は`prd/Makefile`から参照しない。

依存方向:

```text
Client / Channel / Message
          ^
          |
Parser / Reply / IrcUtil / BufferUtil
          ^
          |
        Server
          ^
          |
         main
```

`IrcUtil`はどのクラスにも依存しない。`Reply`は`Client`へ依存する。`BufferUtil`は行長定数のため`IrcUtil`へ依存する。`Channel`は`IrcUtil`へ依存しないため、正規化済みChannel名は呼び出し側が渡す。

図の元データ:

- `../diagrams/module_dependency_ja.mmd`

循環includeを避けるため、可能な箇所では前方宣言を使用する。`Server.hpp`は所有する型の完全定義が必要なため`Client.hpp`と`Channel.hpp`をincludeする。

## 13. 実装完了条件

- すべての宣言がC++98でコンパイルできる
- 所有関係に`new`、`delete`、相互生ポインタを使用していない
- Server、Client、Channelの責務が設計どおり分離されている
- ClientとChannelの関係変更がServerへ集約されている
- Nickname索引とClient Mapが常に一致する
- Getterに不要なコピーがない
- `const`版検索APIが用意されている
- DestructorでFDリークが発生しない
- Debug整合性確認で全不変条件を検証できる

