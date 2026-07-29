# 登録フロー Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** PASS/NICK/USER/CAP/PING/PONG ハンドラと Welcome Sequence を実装し、登録フローを完成させる。

**Architecture:** Server に Nickname 索引・Channel Map(器)・共有 Channel broadcast・登録試行を追加し、認証系ハンドラを `prd/handler/ServerAuthCommands.cpp` に実装。ディスパッチャの該当スタブ 6 個を置き換える。テストは Parser→dispatchCommand を通す統合形。

**Tech Stack:** C++98、自前 TestRunner、make。

**Spec:** `docs/superpowers/specs/2026-07-20-registration-flow-design.md`

## Global Constraints

- C++98 のみ。`-Wall -Wextra -Werror -std=c++98` で警告ゼロ
- 送信行は必ず `Reply::numeric()` / `Reply::command()` 経由(設計書06 §10)
- Numeric 定数は namespace `Numeric`(`Numerics` ではない)
- Numeric 本文は設計書06 §8 の表と完全一致(下のコードに埋め込み済み)
- `_clients` への挿入は `insert(std::make_pair(...))`。`_nickToFd`(map<string,int>)は `operator[]` 可
- コメントは既存スタイル(設計書 §番号引用の日本語ブロックコメント)
- コミット末尾: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`

---

### Task 1: 登録フロー一式 (Server 基盤 + 認証ハンドラ + テスト)

**Files:**
- Modify: `prd/interface/Server.hpp`
- Modify: `prd/interface/Server.cpp`
- Modify: `prd/interface/ServerRelations.cpp`
- Modify: `prd/interface/ServerDispatch.cpp`(スタブ 6 個を削除)
- Create: `prd/handler/ServerAuthCommands.cpp`
- Create: `tests/handler/auth/test_auth.cpp`
- Modify: `tests/test_main.cpp`, `tests/Makefile`, `prd/Makefile`

**Interfaces:**
- Consumes: `IrcUtil::ircCaseFold/normalizeChannelName/isValidNickname/toUpperAscii`、`Reply::numeric/command/clientPrefix/serverPrefix`、`Client` の登録系 API、`Channel::getMembers()`
- Produces: `findClientByNickname`(公開)、`findChannel`(公開)、private の `isNicknameAvailable/registerNickname/unregisterNickname/broadcastToSharedChannels/tryRegisterClient/sendWelcomeSequence/requireParams`、メンバ `_channels/_nickToFd/_serverStartTime`。サブプロジェクト②はこれらをそのまま使う

- [ ] **Step 1: 失敗するテストを書く**

`tests/handler/auth/test_auth.cpp` を新規作成:

```cpp
#include <cstddef>

#include "TestRunner.hpp"

#include "prd/domain/Message.hpp"
#include "prd/interface/Server.hpp"
#include "prd/util/Parser.hpp"

namespace
{
    /* 生の IRC 行を Parser 経由で dispatch する。実運用と同じ経路 */
    void dispatchLine(Server &server, int fd, const std::string &line)
    {
        Message message;

        if (Parser::parse(line, message))
            server.dispatchCommand(fd, message);
    }

    /* 送信バッファの内容を取り出して空にする */
    std::string takeOutput(Server &server, int fd)
    {
        Client *client = server.findClientByFd(fd);

        if (client == NULL)
            return "";

        std::string output = client->getSendBuffer();

        client->eraseSendPrefix(output.size());
        return output;
    }

    /* alice を fd で登録済みにし、Welcome を読み捨てる */
    void registerAlice(Server &server, int fd)
    {
        server.addClient(fd, "127.0.0.1");
        dispatchLine(server, fd, "PASS secret");
        dispatchLine(server, fd, "NICK alice");
        dispatchLine(server, fd, "USER u 0 * :Alice Example");
        takeOutput(server, fd);
    }

    const std::string WELCOME_HEAD =
        ":ircserv.local 001 alice :Welcome to the Internet Relay Network "
        "alice!u@127.0.0.1\r\n"
        ":ircserv.local 002 alice :Your host is ircserv.local, "
        "running version 1.0\r\n"
        ":ircserv.local 003 alice :This server was created ";
    const std::string WELCOME_TAIL =
        ":ircserv.local 004 alice ircserv.local 1.0 - itkol\r\n"
        ":ircserv.local 422 alice :MOTD File is missing\r\n";

    /* Welcome 5 行の検証。003 の時刻部分だけ前置一致で許容する */
    bool isWelcome(const std::string &output)
    {
        if (output.compare(0, WELCOME_HEAD.size(), WELCOME_HEAD) != 0)
            return false;
        if (output.size() < WELCOME_TAIL.size())
            return false;
        return output.compare(output.size() - WELCOME_TAIL.size(),
                              WELCOME_TAIL.size(), WELCOME_TAIL) == 0;
    }
}

void runAuthTests()
{
    TestRunner::beginSuite("Auth");

    /* ── 登録順序: 6 順列すべてで揃った瞬間に 1 回だけ Welcome ── */

    {
        const char *orders[6][3] = {
            { "PASS secret", "NICK alice", "USER u 0 * :Alice Example" },
            { "PASS secret", "USER u 0 * :Alice Example", "NICK alice" },
            { "NICK alice", "PASS secret", "USER u 0 * :Alice Example" },
            { "NICK alice", "USER u 0 * :Alice Example", "PASS secret" },
            { "USER u 0 * :Alice Example", "PASS secret", "NICK alice" },
            { "USER u 0 * :Alice Example", "NICK alice", "PASS secret" },
        };

        for (std::size_t i = 0; i < 6; ++i)
        {
            Server server(6667, "secret");

            server.addClient(3, "127.0.0.1");
            dispatchLine(server, 3, orders[i][0]);
            dispatchLine(server, 3, orders[i][1]);
            ASSERT_EQ("Auth: 2 Command では Welcome なし (順列)",
                      takeOutput(server, 3), "");
            dispatchLine(server, 3, orders[i][2]);
            ASSERT_TRUE("Auth: 3 つ揃って Welcome (順列)",
                        isWelcome(takeOutput(server, 3)));
        }
    }

    /* ── PASS ─────────────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "PASS");
        ASSERT_EQ("Auth: PASS Parameter なしは 461", takeOutput(server, 3),
                  ":ircserv.local 461 * PASS :Not enough parameters\r\n");

        dispatchLine(server, 3, "PASS wrong");
        ASSERT_EQ("Auth: PASS 不一致は 464", takeOutput(server, 3),
                  ":ircserv.local 464 * :Password incorrect\r\n");

        /* 不一致後も接続は維持され、正しい PASS で回復できる */
        dispatchLine(server, 3, "PASS secret");
        ASSERT_EQ("Auth: PASS 成功は単独 Reply なし", takeOutput(server, 3),
                  "");

        dispatchLine(server, 3, "NICK alice");
        dispatchLine(server, 3, "USER u 0 * :Alice Example");
        ASSERT_TRUE("Auth: 回復後に登録完了できる",
                    isWelcome(takeOutput(server, 3)));
    }

    /* ── NICK ─────────────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "NICK");
        ASSERT_EQ("Auth: NICK Parameter なしは 431", takeOutput(server, 3),
                  ":ircserv.local 431 * :No nickname given\r\n");

        dispatchLine(server, 3, "NICK 1abc");
        ASSERT_EQ("Auth: 数字始まりの NICK は 432", takeOutput(server, 3),
                  ":ircserv.local 432 * 1abc :Erroneous nickname\r\n");

        dispatchLine(server, 3, "NICK abcdefghij");
        ASSERT_EQ("Auth: 10 文字の NICK は 432", takeOutput(server, 3),
                  ":ircserv.local 432 * abcdefghij :Erroneous nickname\r\n");
    }

    {
        /* 重複は ircCaseFold で比較する (A-Z→a-z, []→{}, \→|, ^→~) */
        Server server(6667, "secret");

        registerAlice(server, 3);
        server.addClient(4, "10.0.0.1");
        dispatchLine(server, 4, "NICK ALICE");
        ASSERT_EQ("Auth: 大文字違いの重複 NICK は 433", takeOutput(server, 4),
                  ":ircserv.local 433 * ALICE :Nickname is already in use\r\n");

        dispatchLine(server, 4, "NICK [b]x");
        takeOutput(server, 4);
        server.addClient(5, "10.0.0.2");
        dispatchLine(server, 5, "NICK {b}x");
        ASSERT_EQ("Auth: []{} を同一視した重複は 433", takeOutput(server, 5),
                  ":ircserv.local 433 * {b}x :Nickname is already in use\r\n");
    }

    {
        /* 自分の Nickname への変更 (大小変更) は重複扱いしない */
        Server server(6667, "secret");

        registerAlice(server, 3);
        dispatchLine(server, 3, "NICK ALICE");
        ASSERT_EQ("Auth: 自分自身への NICK 変更は許可され通知される",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 NICK :ALICE\r\n");
    }

    {
        /* 変更で旧 Nickname が解放される */
        Server server(6667, "secret");

        registerAlice(server, 3);
        dispatchLine(server, 3, "NICK alicia");
        takeOutput(server, 3);

        server.addClient(4, "10.0.0.1");
        dispatchLine(server, 4, "NICK alice");
        ASSERT_EQ("Auth: 変更後の旧 NICK は再利用できる",
                  takeOutput(server, 4), "");

        ASSERT_TRUE("Auth: findClientByNickname が case-fold で引ける",
                    server.findClientByNickname("ALICIA") != NULL);
        ASSERT_EQ("Auth: 索引が変更後の FD を指す",
                  server.findClientByNickname("alicia")->getFd(), 3);
    }

    {
        /* removeClient で Nickname が解放される */
        Server server(6667, "secret");

        registerAlice(server, 3);
        server.removeClient(3);
        ASSERT_TRUE("Auth: removeClient で索引も消える",
                    server.findClientByNickname("alice") == NULL);

        server.addClient(4, "10.0.0.1");
        dispatchLine(server, 4, "NICK alice");
        ASSERT_EQ("Auth: 切断後の NICK は再利用できる",
                  takeOutput(server, 4), "");
    }

    /* ── USER ─────────────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "USER u 0 *");
        ASSERT_EQ("Auth: USER 3 Parameter は 461", takeOutput(server, 3),
                  ":ircserv.local 461 * USER :Not enough parameters\r\n");

        dispatchLine(server, 3, "USER u@h 0 * :Real");
        ASSERT_EQ("Auth: username の @ は 461", takeOutput(server, 3),
                  ":ircserv.local 461 * USER :Not enough parameters\r\n");

        dispatchLine(server, 3, "USER u 0 * :");
        ASSERT_EQ("Auth: 空 realname は許可", takeOutput(server, 3), "");

        dispatchLine(server, 3, "USER v 0 * :Again");
        ASSERT_EQ("Auth: 未登録でも USER 再実行は 462", takeOutput(server, 3),
                  ":ircserv.local 462 * :Unauthorized command "
                  "(already registered)\r\n");
    }

    /* ── CAP ──────────────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "CAP LS 302");
        ASSERT_EQ("Auth: CAP LS は空 Capability 一覧", takeOutput(server, 3),
                  ":ircserv.local CAP * LS :\r\n");

        dispatchLine(server, 3, "CAP REQ :sasl");
        ASSERT_EQ("Auth: CAP REQ は NAK", takeOutput(server, 3),
                  ":ircserv.local CAP * NAK :sasl\r\n");

        dispatchLine(server, 3, "CAP END");
        ASSERT_EQ("Auth: CAP END は応答なし", takeOutput(server, 3), "");

        dispatchLine(server, 3, "CAP");
        ASSERT_EQ("Auth: CAP 単独は応答なし", takeOutput(server, 3), "");
    }

    /* ── PING / PONG ──────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "PING tok123");
        ASSERT_EQ("Auth: 未登録でも PING に PONG", takeOutput(server, 3),
                  ":ircserv.local PONG ircserv.local :tok123\r\n");

        dispatchLine(server, 3, "PING");
        ASSERT_EQ("Auth: PING token なしは 409", takeOutput(server, 3),
                  ":ircserv.local 409 * :No origin specified\r\n");

        dispatchLine(server, 3, "PING :");
        ASSERT_EQ("Auth: PING 空 token も 409", takeOutput(server, 3),
                  ":ircserv.local 409 * :No origin specified\r\n");

        dispatchLine(server, 3, "PONG tok123");
        ASSERT_EQ("Auth: PONG は無視", takeOutput(server, 3), "");
    }

    /* ── Welcome 再送なし ─────────────────── */

    {
        Server server(6667, "secret");

        registerAlice(server, 3);
        dispatchLine(server, 3, "NICK alice2");
        takeOutput(server, 3);
        dispatchLine(server, 3, "PING x");
        ASSERT_EQ("Auth: 登録後に Welcome は再送されない",
                  takeOutput(server, 3),
                  ":ircserv.local PONG ircserv.local :x\r\n");
    }
}
```

`tests/test_main.cpp`: 宣言へ `void runAuthTests();` を追加し、dispatch ブロックの後へ:

```cpp
    if (all || suite == "auth")
    {
        runAuthTests();
        ran = true;
    }
```

Usage 行の末尾へ `|auth` を追加。

`tests/Makefile`:
- TARGET_SRCS 末尾へ `../prd/handler/ServerAuthCommands.cpp`
- TEST_SRCS 末尾へ `handler/auth/test_auth.cpp`
- vpath へ `handler/auth ../prd/handler`
- `test-dispatch:` の後へ:

```makefile
test-auth: $(NAME)
	./$(NAME) auth
```

- .PHONY へ `test-auth`

- [ ] **Step 2: テストが失敗することを確認する**

Run: `make -C tests test`
Expected: ビルドエラー(`ServerAuthCommands.cpp` が無い / `findClientByNickname` 未宣言)

- [ ] **Step 3: 実装する**

**`prd/interface/Server.hpp`** — include に `#include "../domain/Channel.hpp"` を追加。ヘッダコメントの「Channel・Nickname 索引はまだ持たない」の段落を次へ差し替え:

```
 * 移行期の骨格である。ネットワーク層 (listen socket・poll ループ・
 * ServerNetwork.cpp) はまだ持たない。設計書 02 §4.2 のうち未使用の
 * メンバ (_listenFd, _running, _pollFds, _pendingDisconnects) は、
 * それらを使う層の実装時に追加する。設計書 02 §4.3 の Destructor
 * (全 Client FD と _listenFd の close) も同時に追加すること。現状は
 * FD を所有しないため Default の Destructor で正しい。
```

public の `findClientByFd` の直後へ:

```cpp
    /* Nickname 検索 (設計書 02 §4.6)。ircCaseFold した Key で索引を引く。
       失敗時は NULL */
    Client       *findClientByNickname(const std::string &nickname);
    const Client *findClientByNickname(const std::string &nickname) const;

    /* Channel 検索 (設計書 02 §4.6)。normalizeChannelName した Key で
       Map を引く。失敗時は NULL */
    Channel       *findChannel(const std::string &name);
    const Channel *findChannel(const std::string &name) const;
```

private の typedef の直後へ:

```cpp
    /* ── Nickname 索引 (設計書 02 §4.8) ─────
       不変条件: _nickToFd の内容は _clients 内の Nickname 設定済み
       Client と常に 1 対 1 (設計書 02 §13)。更新は Handler が検証を
       すべて通過した後に行う */
    bool isNicknameAvailable(const std::string &nickname, int exceptFd) const;
    void registerNickname(int fd, const std::string &nickname);
    void unregisterNickname(const std::string &nickname);

    /* ── 送信 (設計書 02 §4.9) ──────────────
       client と Channel を共有する全 Client へ queue する。client 自身
       には送らない (QUIT の規則。NICK のように自分へも送る場合は
       Handler が別途 queueToClient する)。複数 Channel 共有時も FD
       集合で重複排除して 1 回だけ送る */
    void broadcastToSharedChannels(const Client &client,
                                   const std::string &message);

    /* ── 登録 (設計書 04 §5) ────────────────
       PASS/NICK/USER の各 Handler の最後で呼ぶ。この呼び出しで初めて
       登録完了したときだけ Welcome Sequence を送る */
    void tryRegisterClient(int fd);
    /* 001 002 003 004 422 を順に queue する (設計書 06 §6) */
    void sendWelcomeSequence(Client &client);

    /* ── 共通検証 (設計書 04 §4) ────────────
       Parameter 不足なら 461 を queue して false を返す */
    bool requireParams(int fd, const Message &message, std::size_t count);
```

メンバ変数へ追加(`_clients` の後):

```cpp
    std::map<std::string, Channel> _channels;  /* 正規化名 → Channel (02 §4.2) */
    std::map<std::string, int>     _nickToFd;  /* 正規化 Nick → FD (02 §4.2) */
    std::string                    _serverStartTime; /* 003 用 (06 §6) */
```

**`prd/interface/Server.cpp`** — 全体を差し替え:

```cpp
#include <ctime>

#include "Server.hpp"

Server::Server(int port, const std::string &password)
    : _port(port),
      _password(password),
      _serverName("ircserv.local")
{
    /* 003 RPL_CREATED 用の開始時刻 (設計書 06 §6)。書式は spec で
       "%Y-%m-%d %H:%M:%S" と規定 */
    std::time_t now = std::time(NULL);
    char        buffer[32];

    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
                      std::localtime(&now)) != 0)
        _serverStartTime = buffer;
    else
        _serverStartTime = "unknown";
}
```

**`prd/interface/ServerRelations.cpp`** — include へ `#include <set>` と `#include "../util/IrcUtil.hpp"` を追加。`removeClient` を差し替え:

```cpp
void Server::removeClient(int fd)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    /* 不変条件「Nickname 索引と Client Map が常に一致する」(設計書 02
       §13) を保つため、索引も同時に削除する */
    if (!client->getNickname().empty())
        unregisterNickname(client->getNickname());
    _clients.erase(fd);
}
```

ファイル末尾へ追加:

```cpp
Client *Server::findClientByNickname(const std::string &nickname)
{
    std::map<std::string, int>::const_iterator it =
        _nickToFd.find(IrcUtil::ircCaseFold(nickname));

    if (it == _nickToFd.end())
        return NULL;
    return findClientByFd(it->second);
}

const Client *Server::findClientByNickname(const std::string &nickname) const
{
    std::map<std::string, int>::const_iterator it =
        _nickToFd.find(IrcUtil::ircCaseFold(nickname));

    if (it == _nickToFd.end())
        return NULL;
    return findClientByFd(it->second);
}

Channel *Server::findChannel(const std::string &name)
{
    std::map<std::string, Channel>::iterator it =
        _channels.find(IrcUtil::normalizeChannelName(name));

    if (it == _channels.end())
        return NULL;
    return &it->second;
}

const Channel *Server::findChannel(const std::string &name) const
{
    std::map<std::string, Channel>::const_iterator it =
        _channels.find(IrcUtil::normalizeChannelName(name));

    if (it == _channels.end())
        return NULL;
    return &it->second;
}

bool Server::isNicknameAvailable(const std::string &nickname,
                                 int                exceptFd) const
{
    std::map<std::string, int>::const_iterator it =
        _nickToFd.find(IrcUtil::ircCaseFold(nickname));

    if (it == _nickToFd.end())
        return true;
    return it->second == exceptFd;
}

void Server::registerNickname(int fd, const std::string &nickname)
{
    _nickToFd[IrcUtil::ircCaseFold(nickname)] = fd;
}

void Server::unregisterNickname(const std::string &nickname)
{
    _nickToFd.erase(IrcUtil::ircCaseFold(nickname));
}

void Server::broadcastToSharedChannels(const Client      &client,
                                       const std::string &message)
{
    std::set<int>                targets;
    const std::set<std::string> &joined = client.getJoinedChannels();

    for (std::set<std::string>::const_iterator it = joined.begin();
         it != joined.end(); ++it)
    {
        const Channel *channel = findChannel(*it);

        if (channel == NULL)
            continue;

        const std::set<int> &members = channel->getMembers();

        targets.insert(members.begin(), members.end());
    }
    targets.erase(client.getFd());
    for (std::set<int>::const_iterator it = targets.begin();
         it != targets.end(); ++it)
        queueToClient(*it, message);
}
```

**`prd/interface/ServerDispatch.cpp`** — `handlePass` `handleNick` `handleUser` `handleCap` `handlePing` `handlePong` の 6 スタブ定義を削除する(`handleJoin` 以下 8 個は残す)。スタブ節のブロックコメントを次へ更新:

```cpp
/* ============================================================
 * Handler スタブ (設計書 02 §4.10)
 *
 * 認証系 (PASS/NICK/USER/CAP/PING/PONG) は handler/
 * ServerAuthCommands.cpp に実装済み。残りはチャンネル系・MODE の
 * 実装タスクで置き換える。
 * ============================================================ */
```

**`prd/handler/ServerAuthCommands.cpp`** を新規作成:

```cpp
#include <cstddef>

#include "../interface/Server.hpp"
#include "../util/IrcUtil.hpp"
#include "../util/Numerics.hpp"
#include "../util/Reply.hpp"

/* ============================================================
 * 認証系 Command Handler (設計書 04 §5〜§9, §17, §18)
 *
 * PASS / NICK / USER の受信順序は固定しない。3 つが揃った時点で
 * tryRegisterClient() が 1 回だけ Welcome Sequence を送る。
 * ============================================================ */

bool Server::requireParams(int fd, const Message &message, std::size_t count)
{
    if (message.params.size() >= count)
        return true;

    Client *client = findClientByFd(fd);

    if (client != NULL)
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NEEDMOREPARAMS,
                                         message.command
                                             + " :Not enough parameters"));
    return false;
}

void Server::tryRegisterClient(int fd)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (client->tryCompleteRegistration())
        sendWelcomeSequence(*client);
}

void Server::sendWelcomeSequence(Client &client)
{
    int fd = client.getFd();

    /* 001〜004, 422 の本文は設計書 06 §6 と完全一致させる */
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::RPL_WELCOME,
        ":Welcome to the Internet Relay Network "
            + Reply::clientPrefix(client)));
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::RPL_YOURHOST,
        ":Your host is " + _serverName + ", running version 1.0"));
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::RPL_CREATED,
        ":This server was created " + _serverStartTime));
    /* User Mode は実装しないため "-" (設計書 06 §6) */
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::RPL_MYINFO,
        _serverName + " 1.0 - itkol"));
    /* MOTD を実装しないことを明示して登録シーケンスを終える */
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::ERR_NOMOTD,
        ":MOTD File is missing"));
}

/* ── PASS (設計書 04 §6) ────────────────── */

void Server::handlePass(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    /* 登録済みの 462 は Dispatcher が処理済み (設計書 04 §3) */
    if (!requireParams(fd, message, 1))
        return;
    if (message.params[0] != _password)
    {
        /* 不一致でも接続は維持し、再 PASS を許す (設計書 04 §6) */
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_PASSWDMISMATCH,
                                         ":Password incorrect"));
        return;
    }
    client->acceptPassword();
    tryRegisterClient(fd);
}

/* ── NICK (設計書 04 §7) ────────────────── */

void Server::handleNick(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (message.params.empty())
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NONICKNAMEGIVEN,
                                         ":No nickname given"));
        return;
    }

    const std::string &nickname = message.params[0];

    if (!IrcUtil::isValidNickname(nickname))
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_ERRONEUSNICKNAME,
                                         nickname + " :Erroneous nickname"));
        return;
    }
    if (!isNicknameAvailable(nickname, fd))
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NICKNAMEINUSE,
                                         nickname
                                             + " :Nickname is already in use"));
        return;
    }

    /* 通知は旧 Nickname の Prefix で送るため、変更前に組み立てる
       (設計書 04 §7) */
    std::string oldPrefix     = Reply::clientPrefix(*client);
    bool        wasRegistered = client->isRegistered();

    if (!client->getNickname().empty())
        unregisterNickname(client->getNickname());
    registerNickname(fd, nickname);
    client->setNickname(nickname);

    if (wasRegistered)
    {
        /* 自分と共有 Channel の Client へ 1 回ずつ (設計書 04 §20) */
        std::string notice = Reply::command(oldPrefix, "NICK",
                                            ":" + nickname);

        queueToClient(fd, notice);
        broadcastToSharedChannels(*client, notice);
    }
    else
        tryRegisterClient(fd);
}

/* ── USER (設計書 04 §8) ────────────────── */

void Server::handleUser(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    /* 未登録でも USER 受信済みなら再実行として 462 (設計書 04 §8)。
       登録済みの 462 は Dispatcher が処理済み */
    if (client->hasUser())
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_ALREADYREGISTRED,
                                         ":Unauthorized command "
                                         "(already registered)"));
        return;
    }
    if (!requireParams(fd, message, 4))
        return;

    const std::string &username = message.params[0];

    /* username 不正の Numeric は設計書 04 §8 に規定がないため、
       Parameter 不備として 461 を返す (spec の決定事項) */
    if (username.empty()
        || username.find_first_of(" @\r\n") != std::string::npos
        || username.find('\0') != std::string::npos)
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NEEDMOREPARAMS,
                                         message.command
                                             + " :Not enough parameters"));
        return;
    }
    /* mode (params[1]) と unused (params[2]) は保存せず無視する。
       realname は空でも許可する (設計書 04 §8) */
    client->setUser(username, message.params[3]);
    tryRegisterClient(fd);
}

/* ── CAP (設計書 04 §9) ─────────────────── */

void Server::handleCap(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL || message.params.empty())
        return;

    /* 提供 Capability は 0 件。実クライアントの Negotiation を最低限
       終了させるだけで、CAP 状態は登録完了条件に含めない */
    std::string subcommand = IrcUtil::toUpperAscii(message.params[0]);

    if (subcommand == "LS")
        queueToClient(fd, Reply::command(Reply::serverPrefix(_serverName),
                                         "CAP", "* LS :"));
    else if (subcommand == "REQ")
    {
        std::string requested;

        if (message.params.size() >= 2)
            requested = message.params[1];
        queueToClient(fd, Reply::command(Reply::serverPrefix(_serverName),
                                         "CAP", "* NAK :" + requested));
    }
    /* END とその他の Subcommand は応答なし */
}

/* ── PING (設計書 04 §17) ───────────────── */

void Server::handlePing(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    /* 空 token も「origin なし」として 409 (spec の決定事項) */
    if (message.params.empty() || message.params[0].empty())
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NOORIGIN,
                                         ":No origin specified"));
        return;
    }
    queueToClient(fd, Reply::command(Reply::serverPrefix(_serverName), "PONG",
                                     _serverName + " :" + message.params[0]));
}

/* ── PONG (設計書 04 §18) ───────────────── */

void Server::handlePong(int fd, const Message &message)
{
    /* Server から定期 PING を送らないため、Parameter の有無に
       かかわらず受信して無視する。将来の keepalive に備え Handler
       だけ独立させている */
    (void)fd;
    (void)message;
}
```

- [ ] **Step 4: テストが通ることを確認する**

Run: `make -C tests test`
Expected: 全 suite PASS(Auth suite 40 項目を含む)、exit 0

Run: `make -C tests test-auth`
Expected: Auth suite のみ実行され PASS

- [ ] **Step 5: 提出物ビルドへ組み込む**

`prd/Makefile` の SRCS 末尾へ `handler/ServerAuthCommands.cpp` を追加。

Run: `make -C prd re`
Expected: 全オブジェクト警告ゼロでコンパイル成功

- [ ] **Step 6: コミット**

```bash
git add prd/interface/Server.hpp prd/interface/Server.cpp \
        prd/interface/ServerRelations.cpp prd/interface/ServerDispatch.cpp \
        prd/handler/ServerAuthCommands.cpp \
        tests/handler/auth/test_auth.cpp tests/test_main.cpp tests/Makefile \
        prd/Makefile
git commit -m "feat(handler): 登録フロー (PASS/NICK/USER/CAP/PING/PONG) と Welcome を実装

設計書 04 §5〜§9, §17, §18 と 06 §6 に基づき、認証系 Handler・
Nickname 索引・Welcome Sequence を実装。

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```
