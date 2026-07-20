# Command Dispatcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 設計書04 §3 の Command Dispatcher を、既存 Domain / Util 層の上に実装する(ハンドラ本体とネットワーク層はスコープ外、空スタブ)。

**Architecture:** Server クラスの最小骨格(Client 所有 Map・検索・送信 queue)を `prd/interface/` に新設し、`dispatchCommand()` が静的な Dispatcher 表で登録前後のゲーティング(451/462/421/登録前無視)を行い private ハンドラスタブへ振り分ける。テストはソケット不使用で、送信バッファの中身を検査する。

**Tech Stack:** C++98、自前 TestRunner(外部ライブラリ禁止)、make。

**Spec:** `docs/superpowers/specs/2026-07-20-command-dispatch-design.md`

## Global Constraints

- C++98 のみ。`-Wall -Wextra -Werror -std=c++98` で警告ゼロ
- 外部ライブラリ禁止(テストも自前 TestRunner を使う)
- `_clients` への挿入は `insert(std::make_pair(...))`。`operator[]` 禁止(設計書02 §5.3)
- 送信行の組み立ては必ず `Reply::numeric()` / `Reply::command()` 経由。生の文字列連結禁止(設計書06 §10)
- Numeric 定数は `prd/util/Numerics.hpp` の **namespace `Numeric`**(`Numerics` ではない)
- `_serverName` は固定値 `"ircserv.local"`(設計書02 §4.2)
- 462 の本文は `:Unauthorized command (already registered)`(設計書06 §16)
- コメントは既存コードのスタイル(設計書の §番号を引用する日本語ブロックコメント)に合わせる
- コミットは各タスク末尾で行い、`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>` を付ける

---

### Task 1: Server 骨格 (Constructor / 接続管理 / 検索 / queueToClient)

**Files:**
- Create: `prd/interface/Server.hpp`
- Create: `prd/interface/Server.cpp`
- Create: `prd/interface/ServerRelations.cpp`
- Create: `tests/interface/dispatch/test_dispatch.cpp`
- Modify: `tests/test_main.cpp`
- Modify: `tests/Makefile`

**Interfaces:**
- Consumes: `Client(int fd, const std::string &hostname)`, `Client::getFd()`, `Client::getHostname()`, `Client::getSendBuffer()`, `Client::appendSendBuffer(const std::string&)` (既存 `prd/domain/Client.hpp`)
- Produces: `Server(int port, const std::string &password)`, `bool Server::addClient(int fd, const std::string &hostname)`, `void Server::removeClient(int fd)`, `Client *Server::findClientByFd(int fd)` / const 版, `void Server::queueToClient(int fd, const std::string &message)`。Server.hpp には Task 2 が実装する `void dispatchCommand(int fd, const Message &message)` と 14 ハンドラの宣言も含める(ヘッダはこのタスクで完成させる)

- [ ] **Step 1: 失敗するテストを書く**

`tests/interface/dispatch/test_dispatch.cpp` を新規作成:

```cpp
#include "TestRunner.hpp"

#include "prd/domain/Message.hpp"
#include "prd/interface/Server.hpp"

void runDispatchTests()
{
    TestRunner::beginSuite("Dispatch");

    /* ── addClient / removeClient / findClientByFd ── */

    {
        Server server(6667, "pass");

        ASSERT_TRUE("Server: addClient 新規", server.addClient(3, "127.0.0.1"));
        ASSERT_FALSE("Server: addClient 重複 FD は拒否",
                     server.addClient(3, "10.0.0.1"));

        Client *client = server.findClientByFd(3);

        ASSERT_TRUE("Server: findClientByFd で取得できる", client != NULL);
        ASSERT_EQ("Server: Client の FD", client->getFd(), 3);
        ASSERT_EQ("Server: Client の Hostname", client->getHostname(),
                  "127.0.0.1");
        ASSERT_TRUE("Server: 不在 FD は NULL",
                    server.findClientByFd(99) == NULL);

        server.removeClient(3);
        ASSERT_TRUE("Server: removeClient 後は NULL",
                    server.findClientByFd(3) == NULL);
    }

    {
        Server server(6667, "pass");

        server.addClient(3, "127.0.0.1");

        const Server &constServer = server;

        ASSERT_TRUE("Server: const 版 findClientByFd",
                    constServer.findClientByFd(3) != NULL);
        ASSERT_TRUE("Server: const 版 findClientByFd 不在は NULL",
                    constServer.findClientByFd(99) == NULL);
    }

    /* ── queueToClient (設計書 02 §4.9) ────── */

    {
        Server server(6667, "pass");

        server.addClient(3, "127.0.0.1");

        server.queueToClient(3, "PING :x");
        ASSERT_EQ("Server: queueToClient は CRLF を付与する",
                  server.findClientByFd(3)->getSendBuffer(), "PING :x\r\n");

        server.queueToClient(3, "PONG :y\r\n");
        ASSERT_EQ("Server: 既に CRLF 付きなら二重付与しない",
                  server.findClientByFd(3)->getSendBuffer(),
                  "PING :x\r\nPONG :y\r\n");

        /* クラッシュしないことの確認 */
        server.queueToClient(99, "IGNORED");
        ASSERT_TRUE("Server: 不在 FD への queue は無視",
                    server.findClientByFd(99) == NULL);
    }
}
```

`tests/test_main.cpp` を修正。suite 宣言へ 1 行追加:

```cpp
void runReplyTests();
void runDispatchTests();
```

`main()` の reply ブロックの後へ追加:

```cpp
    if (all || suite == "dispatch")
    {
        runDispatchTests();
        ran = true;
    }
```

Usage 行を更新:

```cpp
        std::cerr << "Usage: " << argv[0]
                  << " [all|ircutil|bufferutil|client|channel|parser|reply|dispatch]" << std::endl;
```

`tests/Makefile` を修正(4 箇所 + ターゲット追加):

```makefile
TARGET_SRCS = ../prd/domain/Client.cpp \
              ../prd/domain/Channel.cpp \
              ../prd/util/IrcUtil.cpp \
              ../prd/util/BufferUtil.cpp \
              ../prd/util/Parser.cpp \
              ../prd/util/Reply.cpp \
              ../prd/interface/Server.cpp \
              ../prd/interface/ServerRelations.cpp
```

```makefile
TEST_SRCS   = test_main.cpp \
              framework/TestRunner.cpp \
              domain/client/test_client.cpp \
              domain/channel/test_channel.cpp \
              util/ircutil/test_ircutil.cpp \
              util/bufferutil/test_bufferutil.cpp \
              util/parser/test_parser.cpp \
              util/reply/test_reply.cpp \
              interface/dispatch/test_dispatch.cpp
```

HEADERS へ追加:

```makefile
HEADERS     = framework/TestRunner.hpp \
              ../prd/domain/Client.hpp \
              ../prd/domain/Channel.hpp \
              ../prd/domain/Message.hpp \
              ../prd/util/IrcUtil.hpp \
              ../prd/util/BufferUtil.hpp \
              ../prd/util/Parser.hpp \
              ../prd/util/Numerics.hpp \
              ../prd/util/Reply.hpp \
              ../prd/interface/Server.hpp
```

vpath へ追加:

```makefile
vpath %.cpp . framework \
             domain/client domain/channel \
             util/ircutil util/bufferutil util/parser util/reply \
             interface/dispatch \
             ../prd/domain ../prd/util ../prd/interface
```

`test-reply:` ターゲットの後へ追加し、.PHONY にも `test-dispatch` を足す:

```makefile
test-dispatch: $(NAME)
	./$(NAME) dispatch
```

- [ ] **Step 2: テストが失敗することを確認する**

Run: `make -C tests test`
Expected: コンパイルエラー `'prd/interface/Server.hpp' file not found`(Server.hpp 未作成のため)

- [ ] **Step 3: 最小実装を書く**

`prd/interface/Server.hpp` を新規作成:

```cpp
#pragma once

#include <map>
#include <string>

#include "../domain/Client.hpp"
#include "../domain/Message.hpp"

/* ============================================================
 * Server
 *
 * Client の所有と Command のディスパッチを担う (設計書 02 §4)。
 *
 * 移行期の骨格である。ネットワーク層 (listen socket・poll ループ・
 * ServerNetwork.cpp) と Channel・Nickname 索引はまだ持たない。
 * 設計書 02 §4.2 のうち未使用のメンバ (_listenFd, _running, _pollFds,
 * _channels, _nickToFd, _pendingDisconnects) は、それらを使う層の
 * 実装時に追加する。
 * ============================================================ */
class Server
{
public:
    /* 設計書 02 §4.3。socket 作成 (setupListeningSocket) はネットワーク
       層の実装時に追加するため、ここではメンバ初期化のみ行う */
    Server(int port, const std::string &password);

    /* ── 接続管理 (設計書 02 にない移行期 API) ──
     *
     * 設計書 02 の接続受付は acceptClient() (実 socket 前提) だが未実装の
     * ため、Client Map への出し入れだけを分離した。ネットワーク担当は
     * acceptClient() / disconnectClient() の実装からこれらを呼んでよい */

    /* 同一 FD が既に存在すれば false を返し、何もしない。
       挿入は insert(std::make_pair(...)) を使う (設計書 02 §5.3) */
    bool addClient(int fd, const std::string &hostname);
    /* FD の close() は行わない (FD の所有はネットワーク層) */
    void removeClient(int fd);

    /* ── 検索 (設計書 02 §4.6) ──────────────
       失敗時は NULL を返す。例外を送出しない */
    Client       *findClientByFd(int fd);
    const Client *findClientByFd(int fd) const;

    /* ── 送信 (設計書 02 §4.9) ──────────────
       末尾が CRLF でなければ CRLF を付与し、該当 Client の送信バッファへ
       追加する。send() は呼ばない。FD が不在なら何もしない。
       空文字を queue しないのは呼び出し側の責務 (設計書 06 §17) */
    void queueToClient(int fd, const std::string &message);

    /* ── Command Dispatcher (設計書 04 §3) ──
       Parser が生成した Message を登録状態に応じて各 Handler へ振り分ける。
       command は Parser が ASCII 大文字化済みであることを前提とする。
       FD が不在なら何もしない */
    void dispatchCommand(int fd, const Message &message);

private:
    typedef void (Server::*CommandHandler)(int fd, const Message &message);

    /* 設計書 02 §4.10 の Command Handler 群。
       現状は全て空スタブで、handler/ 配下の実装タスクで置き換える */
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

    int                   _port;       /* 設計書 02 §4.2 */
    std::string           _password;   /* 同上 */
    std::string           _serverName; /* 固定値 "ircserv.local" (同上) */
    std::map<int, Client> _clients;    /* FD → Client の所有 Map (同上) */
};
```

`prd/interface/Server.cpp` を新規作成:

```cpp
#include "Server.hpp"

Server::Server(int port, const std::string &password)
    : _port(port),
      _password(password),
      _serverName("ircserv.local")
{
}
```

`prd/interface/ServerRelations.cpp` を新規作成:

```cpp
#include <cstddef>
#include <utility>

#include "Server.hpp"

bool Server::addClient(int fd, const std::string &hostname)
{
    if (_clients.find(fd) != _clients.end())
        return false;
    _clients.insert(std::make_pair(fd, Client(fd, hostname)));
    return true;
}

void Server::removeClient(int fd)
{
    _clients.erase(fd);
}

Client *Server::findClientByFd(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);

    if (it == _clients.end())
        return NULL;
    return &it->second;
}

const Client *Server::findClientByFd(int fd) const
{
    std::map<int, Client>::const_iterator it = _clients.find(fd);

    if (it == _clients.end())
        return NULL;
    return &it->second;
}

void Server::queueToClient(int fd, const std::string &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (message.size() >= 2
        && message.compare(message.size() - 2, 2, "\r\n") == 0)
        client->appendSendBuffer(message);
    else
        client->appendSendBuffer(message + "\r\n");
}
```

- [ ] **Step 4: テストが通ることを確認する**

Run: `make -C tests test`
Expected: 全 suite PASS(Dispatch suite の 12 項目を含む)、exit 0

注意: `dispatchCommand` とハンドラはヘッダ宣言のみで未定義だが、テストから参照していないためリンクは通る。

- [ ] **Step 5: コミット**

```bash
git add prd/interface/Server.hpp prd/interface/Server.cpp \
        prd/interface/ServerRelations.cpp \
        tests/interface/dispatch/test_dispatch.cpp \
        tests/test_main.cpp tests/Makefile
git commit -m "feat(interface): Server 骨格 (接続管理・検索・送信 queue) を実装

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: dispatchCommand とゲーティング

**Files:**
- Create: `prd/interface/ServerDispatch.cpp`
- Modify: `tests/interface/dispatch/test_dispatch.cpp`(末尾へテスト追加)
- Modify: `tests/Makefile`(TARGET_SRCS へ 1 行)
- Modify: `prd/Makefile`(SRCS / HEADERS へ追加)

**Interfaces:**
- Consumes: Task 1 の `Server` 骨格全 API、`Reply::numeric(const std::string&, const Client&, int, const std::string&)`、`Numeric::ERR_UNKNOWNCOMMAND` (421) / `Numeric::ERR_NOTREGISTERED` (451) / `Numeric::ERR_ALREADYREGISTRED` (462)、`Client::acceptPassword()` / `setNickname()` / `setUser()` / `tryCompleteRegistration()` / `isRegistered()`
- Produces: `void Server::dispatchCommand(int fd, const Message &message)` の実装と 14 個の空スタブハンドラ定義

- [ ] **Step 1: 失敗するテストを書く**

`tests/interface/dispatch/test_dispatch.cpp` の先頭 include の直後へヘルパを追加:

```cpp
namespace
{
    /* PASS / NICK / USER を揃えて登録済みにする (設計書 04 §5) */
    void registerClient(Server &server, int fd, const std::string &nickname)
    {
        Client *client = server.findClientByFd(fd);

        client->acceptPassword();
        client->setNickname(nickname);
        client->setUser("u", "Real Name");
        client->tryCompleteRegistration();
    }
}
```

`runDispatchTests()` の末尾(queueToClient ブロックの後)へ追加:

```cpp
    /* ── dispatchCommand: 登録前ゲーティング (設計書 04 §3) ── */

    {
        /* 登録必須 Command は登録前 451 */
        const char *commands[] = { "JOIN", "PRIVMSG", "KICK", "INVITE",
                                   "TOPIC", "MODE", "PART" };

        for (std::size_t i = 0; i < sizeof(commands) / sizeof(commands[0]);
             ++i)
        {
            Server  server(6667, "pass");
            Message message;

            server.addClient(3, "127.0.0.1");
            message.command = commands[i];
            server.dispatchCommand(3, message);

            /* Nickname 未設定のため target は "*" (設計書 06 §4) */
            ASSERT_EQ(std::string("Dispatch: 未登録 ") + commands[i]
                          + " は 451",
                      server.findClientByFd(3)->getSendBuffer(),
                      ":ircserv.local 451 * :You have not registered\r\n");
        }
    }

    {
        /* 登録前に許可される Command はゲートを通過する
           (スタブ到達 = Numeric が積まれない) */
        const char *commands[] = { "PASS", "NICK", "USER", "CAP", "PING",
                                   "PONG", "QUIT" };

        for (std::size_t i = 0; i < sizeof(commands) / sizeof(commands[0]);
             ++i)
        {
            Server  server(6667, "pass");
            Message message;

            server.addClient(3, "127.0.0.1");
            message.command = commands[i];
            server.dispatchCommand(3, message);

            ASSERT_EQ(std::string("Dispatch: 未登録 ") + commands[i]
                          + " はゲート通過",
                      server.findClientByFd(3)->getSendBuffer(), "");
        }
    }

    {
        /* 未登録の未知 Command は無視 (設計書 04 §3 —
           クライアント固有の初期交渉で登録を妨げない) */
        Server  server(6667, "pass");
        Message message;

        server.addClient(3, "127.0.0.1");
        message.command = "FOO";
        server.dispatchCommand(3, message);

        ASSERT_EQ("Dispatch: 未登録の未知 Command は無視",
                  server.findClientByFd(3)->getSendBuffer(), "");
    }

    /* ── dispatchCommand: 登録後ゲーティング ── */

    {
        /* PASS / USER は登録後 462 */
        const char *commands[] = { "PASS", "USER" };

        for (std::size_t i = 0; i < sizeof(commands) / sizeof(commands[0]);
             ++i)
        {
            Server  server(6667, "pass");
            Message message;

            server.addClient(3, "127.0.0.1");
            registerClient(server, 3, "alice");
            message.command = commands[i];
            server.dispatchCommand(3, message);

            ASSERT_EQ(std::string("Dispatch: 登録済み ") + commands[i]
                          + " は 462",
                      server.findClientByFd(3)->getSendBuffer(),
                      ":ircserv.local 462 alice :Unauthorized command "
                      "(already registered)\r\n");
        }
    }

    {
        /* 登録後の未知 Command は 421。受信した Command 名を返す */
        Server  server(6667, "pass");
        Message message;

        server.addClient(3, "127.0.0.1");
        registerClient(server, 3, "alice");
        message.command = "FOO";
        server.dispatchCommand(3, message);

        ASSERT_EQ("Dispatch: 登録済みの未知 Command は 421",
                  server.findClientByFd(3)->getSendBuffer(),
                  ":ircserv.local 421 alice FOO :Unknown command\r\n");
    }

    {
        /* 登録後の通常 Command はゲートを通過する */
        const char *commands[] = { "NICK", "CAP", "PING", "PONG", "QUIT",
                                   "JOIN", "PRIVMSG", "KICK", "INVITE",
                                   "TOPIC", "MODE", "PART" };

        for (std::size_t i = 0; i < sizeof(commands) / sizeof(commands[0]);
             ++i)
        {
            Server  server(6667, "pass");
            Message message;

            server.addClient(3, "127.0.0.1");
            registerClient(server, 3, "alice");
            message.command = commands[i];
            server.dispatchCommand(3, message);

            ASSERT_EQ(std::string("Dispatch: 登録済み ") + commands[i]
                          + " はゲート通過",
                      server.findClientByFd(3)->getSendBuffer(), "");
        }
    }

    {
        /* 不在 FD への dispatch は何もしない (設計書 04 §4 処理順 1) */
        Server  server(6667, "pass");
        Message message;

        message.command = "JOIN";
        server.dispatchCommand(99, message);
        ASSERT_TRUE("Dispatch: 不在 FD は無視",
                    server.findClientByFd(99) == NULL);
    }
```

`sizeof` 計算に必要な `#include <cstddef>` をファイル先頭の include 群
(`"TestRunner.hpp"` の後)へ追加する。

`tests/Makefile` の TARGET_SRCS 末尾へ追加:

```makefile
              ../prd/interface/ServerDispatch.cpp
```

- [ ] **Step 2: テストが失敗することを確認する**

Run: `make -C tests test`
Expected: `make: *** No rule to make target ... ServerDispatch.cpp` のビルドエラー(未作成のため)

- [ ] **Step 3: 最小実装を書く**

`prd/interface/ServerDispatch.cpp` を新規作成:

```cpp
#include <cstddef>

#include "../util/Numerics.hpp"
#include "../util/Reply.hpp"
#include "Server.hpp"

/* ============================================================
 * Command Dispatcher (設計書 04 §3)
 *
 * Dispatcher 表は private の CommandHandler を参照するため、
 * メンバ関数内の static const 配列として定義する。
 * ============================================================ */
void Server::dispatchCommand(int fd, const Message &message)
{
    struct CommandEntry
    {
        const char             *name;
        Server::CommandHandler  handler;
        bool                    requiresRegistration;  /* 登録前 → 451 */
        bool                    rejectsWhenRegistered; /* 登録後 → 462 */
    };
    /* 設計書 04 §3 の Dispatcher 表と 1 対 1 対応 */
    static const CommandEntry table[] = {
        { "PASS",    &Server::handlePass,    false, true  },
        { "NICK",    &Server::handleNick,    false, false },
        { "USER",    &Server::handleUser,    false, true  },
        { "CAP",     &Server::handleCap,     false, false },
        { "PING",    &Server::handlePing,    false, false },
        { "PONG",    &Server::handlePong,    false, false },
        { "QUIT",    &Server::handleQuit,    false, false },
        { "JOIN",    &Server::handleJoin,    true,  false },
        { "PRIVMSG", &Server::handlePrivmsg, true,  false },
        { "KICK",    &Server::handleKick,    true,  false },
        { "INVITE",  &Server::handleInvite,  true,  false },
        { "TOPIC",   &Server::handleTopic,   true,  false },
        { "MODE",    &Server::handleMode,    true,  false },
        { "PART",    &Server::handlePart,    true,  false },
    };
    static const std::size_t tableSize = sizeof(table) / sizeof(table[0]);

    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;

    const CommandEntry *entry = NULL;

    for (std::size_t i = 0; i < tableSize; ++i)
    {
        if (message.command == table[i].name)
        {
            entry = &table[i];
            break;
        }
    }

    if (entry == NULL)
    {
        /* 登録前の未知 Command は無視する (設計書 04 §3 —
           クライアント固有の初期交渉で登録を妨げないため) */
        if (client->isRegistered())
            queueToClient(fd,
                          Reply::numeric(_serverName, *client,
                                         Numeric::ERR_UNKNOWNCOMMAND,
                                         message.command
                                             + " :Unknown command"));
        return;
    }
    if (!client->isRegistered() && entry->requiresRegistration)
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NOTREGISTERED,
                                         ":You have not registered"));
        return;
    }
    if (client->isRegistered() && entry->rejectsWhenRegistered)
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_ALREADYREGISTRED,
                                         ":Unauthorized command "
                                         "(already registered)"));
        return;
    }
    (this->*(entry->handler))(fd, message);
}

/* ============================================================
 * Handler スタブ (設計書 02 §4.10)
 *
 * handler/ 配下の実装タスクで順次置き換える。
 * ============================================================ */

void Server::handlePass(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handleNick(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handleUser(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handleJoin(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handlePrivmsg(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handleKick(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handleInvite(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handleTopic(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handleMode(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handlePing(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handlePong(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handleQuit(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handlePart(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}

void Server::handleCap(int fd, const Message &message)
{
    (void)fd;
    (void)message;
}
```

- [ ] **Step 4: テストが通ることを確認する**

Run: `make -C tests test`
Expected: 全 suite PASS(Dispatch suite に追加 31 項目)、exit 0

Run: `make -C tests test-dispatch`
Expected: Dispatch suite のみ実行され PASS

- [ ] **Step 5: 提出物ビルドへ組み込む**

`prd/Makefile` の SRCS 末尾へ追加:

```makefile
SRCS    = domain/Client.cpp \
          domain/Channel.cpp \
          util/BufferUtil.cpp \
          util/IrcUtil.cpp \
          util/Parser.cpp \
          util/Reply.cpp \
          interface/Server.cpp \
          interface/ServerRelations.cpp \
          interface/ServerDispatch.cpp
```

HEADERS 末尾へ追加:

```makefile
          interface/Server.hpp
```

(`HEADERS = domain/Client.hpp \` で始まる変数の最終行 `util/Reply.hpp` の後)

- [ ] **Step 6: 提出物ビルドが通ることを確認する**

Run: `make -C prd re`
Expected: 全 .o のコンパイルが警告なしで成功(main() が無い移行期のため `all: $(OBJS)` のままリンクは行わない)

- [ ] **Step 7: コミット**

```bash
git add prd/interface/ServerDispatch.cpp prd/Makefile \
        tests/interface/dispatch/test_dispatch.cpp tests/Makefile
git commit -m "feat(interface): Command Dispatcher とゲーティングを実装

設計書 04 §3 の Dispatcher 表に基づき、登録前後の
451 / 462 / 421 / 登録前無視 を dispatchCommand へ実装。
14 ハンドラは空スタブ。

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```
