# 登録フロー (PASS/NICK/USER/CAP/PING/PONG + Welcome) 設計 (2026-07-20)

## 目的

設計書04 §5〜§9, §17, §18 の認証系 Command Handler と Welcome Sequence を
実装し、PASS/NICK/USER が揃った Client が登録完了できるようにする。
残りのサブプロジェクト: ②チャンネル系 (JOIN/PART/PRIVMSG/KICK/INVITE/
TOPIC/QUIT)、③MODE、④ネットワーク層。

## スコープ

- Server への追加: Nickname 索引・Channel Map(器のみ)・検索・共有
  Channel broadcast・登録試行・Welcome Sequence・Server 開始時刻
- Handler 実装: handlePass / handleNick / handleUser / handleCap /
  handlePing / handlePong(ディスパッチ済みスタブを本実装へ置換)
- 共通検証: requireParams()(設計書04 §4)
- 単体テスト(ソケット不使用)

スコープ外: JOIN 系・QUIT(切断処理はサブプロジェクト②)・MODE・
ネットワーク I/O。handleQuit ほかチャンネル系 6 個はスタブのまま。

## ファイル構成 (設計書02 §12)

```
prd/interface/Server.hpp        宣言追加
prd/interface/ServerRelations.cpp  索引・検索・broadcast 追加
prd/handler/ServerAuthCommands.cpp 新規: 認証系 Handler + Welcome
tests/handler/auth/test_auth.cpp   新規: suite "auth"
```

## Server クラスへの追加

```cpp
public:
    /* 設計書 02 §4.6。失敗時 NULL */
    Client       *findClientByNickname(const std::string &nickname);
    const Client *findClientByNickname(const std::string &nickname) const;
    Channel       *findChannel(const std::string &name);
    const Channel *findChannel(const std::string &name) const;

private:
    /* 設計書 02 §4.8 */
    bool isNicknameAvailable(const std::string &nickname, int exceptFd) const;
    void registerNickname(int fd, const std::string &nickname);
    void unregisterNickname(const std::string &nickname);

    /* 設計書 02 §4.9。client と Channel を共有する全 Client へ queue する。
       client 自身には送らない (QUIT の「送信者へ送らない」規則に合わせる。
       NICK のように自分にも送る場合は Handler が別途 queueToClient する)。
       複数 Channel 共有時も FD 集合で重複排除して 1 回だけ送る */
    void broadcastToSharedChannels(const Client &client,
                                   const std::string &message);

    /* 設計書 04 §5。tryCompleteRegistration() が true を返したときだけ
       sendWelcomeSequence() を呼ぶ */
    void tryRegisterClient(int fd);
    void sendWelcomeSequence(Client &client);

    /* 設計書 04 §4。不足なら 461 <command> :Not enough parameters を
       queue して false */
    bool requireParams(int fd, const Message &message, std::size_t count);

    std::map<std::string, Channel> _channels;  /* 正規化名 → Channel (02 §4.2) */
    std::map<std::string, int>     _nickToFd;  /* 正規化 Nick → FD (02 §4.2) */
    std::string                    _serverStartTime; /* 06 §6 003 用 */
```

- `_nickToFd` / `_channels` の Key は `IrcUtil::ircCaseFold()` /
  `IrcUtil::normalizeChannelName()` で正規化する
- `removeClient(fd)` は Nickname 索引も同時に削除する(不変条件
  「Nickname 索引と Client Map が常に一致する」設計書02 §13)
- `_serverStartTime` は Constructor で `std::time()` + `std::strftime()`
  により `"%Y-%m-%d %H:%M:%S"` 形式で保存する(設計書06 §6 は関数のみ
  指定で書式は未規定のためここで確定する)
- Server の version 文字列は `"1.0"` 固定(設計書06 §6)

## Handler 仕様

Numeric 本文はすべて設計書06 §8 の表と完全一致させる。組み立ては
`Reply::numeric()` / `Reply::command()` のみ。

### PASS (設計書04 §6)

1. `requireParams(fd, message, 1)` — 不足なら 461 `PASS :Not enough parameters`
2. 登録済み 462 は Dispatcher が処理済み(表で rejectsWhenRegistered)
3. `params[0]` を `_password` と完全一致比較。不一致 → 464
   `:Password incorrect`(接続維持、passwordAccepted は変更しない)
4. 一致 → `acceptPassword()` → `tryRegisterClient(fd)`
5. 成功時の単独 Reply なし。不一致後の再 PASS 成功を許す

### NICK (設計書04 §7)

1. params なし → 431 `:No nickname given`
2. `IrcUtil::isValidNickname()` で検証 → 不正なら 432
   `<nick> :Erroneous nickname`
3. `isNicknameAvailable(nick, fd)`(ircCaseFold 比較、自 FD 除外)→
   使用中なら 433 `<nick> :Nickname is already in use`
4. 旧 Prefix(`Reply::clientPrefix`)と登録状態を先に保存
5. 旧 Nickname があれば `unregisterNickname(旧)` → `registerNickname(fd, 新)`
   → `setNickname(新)`
6. 登録済みなら `:<旧prefix> NICK :<新nick>` を自分へ queue +
   `broadcastToSharedChannels()` で共有 Channel Client へ
7. 未登録なら `tryRegisterClient(fd)`

### USER (設計書04 §8)

1. `requireParams(fd, message, 4)` — 不足なら 461 `USER :Not enough parameters`
2. 登録済み 462 は Dispatcher 処理済み。未登録でも `hasUser()` なら 462
   (再実行)
3. username 検証: 空でない、空白・`@`・CR・LF・NUL を含まない。
   不正なら 461 `USER :Not enough parameters`(設計書04 §8 は不正
   username の Numeric を規定しないため Parameter 不備として扱う。
   本 spec での決定事項)
4. `setUser(params[0], params[3])`。mode / unused (params[1], params[2])
   は無視。realname は空でも許可
5. `tryRegisterClient(fd)`

### CAP (設計書04 §9)

Subcommand は `params[0]` を `toUpperAscii()` して判定する。

| Subcommand | 応答 |
|---|---|
| `LS` | `:ircserv.local CAP * LS :` |
| `REQ` | `:ircserv.local CAP * NAK :<requested>`(requested = params[1]、無ければ空) |
| `END`・その他・params なし | 応答なし |

CAP 状態は登録完了条件に含めない。

### PING (設計書04 §17)

- params なし、または `params[0]` が空 → 409 `:No origin specified`
- 成功: `:ircserv.local PONG ircserv.local :<token>`
  (`Reply::command(Reply::serverPrefix(_serverName), "PONG",
  _serverName + " :" + token)`)
- Client 状態は変更しない。登録前でも処理する(Dispatcher 表で許可済み)

### PONG (設計書04 §18)

Parameter の有無にかかわらず受信して無視する(空実装のまま、コメントで
意図を明記)。

## Welcome Sequence (設計書04 §5, 06 §6)

`tryRegisterClient()` → `tryCompleteRegistration()` が true のときだけ、
以下を順に queue する:

```
:ircserv.local 001 <nick> :Welcome to the Internet Relay Network <nick>!<user>@<host>
:ircserv.local 002 <nick> :Your host is ircserv.local, running version 1.0
:ircserv.local 003 <nick> :This server was created <server-start-time>
:ircserv.local 004 <nick> ircserv.local 1.0 - itkol
:ircserv.local 422 <nick> :MOTD File is missing
```

- 001 の `<nick>!<user>@<host>` は `Reply::clientPrefix()`
- 004 の User Mode は未実装のため `-`、Channel Mode は `itkol`
- PASS/NICK/USER の受信順序 6 通りすべてで、揃った瞬間に 1 回だけ送る

## エラー処理

- 索引の更新は「検証がすべて通ってから」行い、失敗時に部分変更を残さない
  (設計書04 §4)
- `findClientByNickname` は `ircCaseFold` した Key で `_nickToFd` を引き、
  見つかった FD で `_clients` を引く。索引不整合(FD が _clients に無い)
  は起こらない前提とし、NULL を返す

## テスト計画

`tests/handler/auth/test_auth.cpp`、suite 名 "auth"。主なケース:

| 分類 | ケース |
|---|---|
| 登録順序 | PASS→NICK→USER / PASS→USER→NICK / NICK→USER→PASS など 6 順列で、最後の 1 つが揃った瞬間だけ Welcome 5 行が queue される |
| Welcome | 001/002/004/422 は完全一致、003 は `:ircserv.local 003 nick :This server was created ` 前置一致 |
| PASS | 461 / 464(不一致後に正しい PASS で回復)/ 成功時 Reply なし |
| NICK | 431 / 432(不正 9 種例)/ 433(ircCaseFold 重複: `ALICE` vs `alice`, `[]` vs `{}`)/ 変更で旧 Nick が解放される / 登録済み変更で旧 Prefix の NICK 通知が自分へ届く |
| USER | 461(3 個以下)/ 未登録での再 USER → 462 / username 不正 → 461 / realname 空許可 |
| CAP | LS / REQ(NAK)/ END(無応答)/ params なし(無応答) |
| PING | 409(なし・空)/ PONG 応答形式 / 未登録でも応答 |
| PONG | 何も起きない |
| 索引 | removeClient で Nickname が解放される / findClientByNickname が case-fold で引ける |

## 設計書との差分 (意図的)

1. username 不正時の Numeric を 461 と定める(設計書04 §8 に規定なし)
2. `_serverStartTime` の書式を `"%Y-%m-%d %H:%M:%S"` と定める(同上)
3. `broadcastToSharedChannels()` は送信 Client 自身を除外する(02 §4.9 に
   明記なし。QUIT の規則と NICK の実装都合から決定)
4. PING の空 token(`PING :`)も 409 とする(設計書04 §17 は「なし」のみ規定)
