# Command Dispatcher 設計 (2026-07-20)

## 目的

設計書04 §3 の Command Dispatcher を、既存の Domain / Util 層
(Client, Channel, Message, Parser, Reply, Numerics, IrcUtil) を使って実装する。
各 Command Handler の本体と MODE は本スコープ外とし、空スタブを置く。
ネットワーク層 (poll ループ、socket) も本スコープ外で、後続の
`ServerNetwork.cpp` 実装時に追加される。

## スコープ

- Server クラスの最小骨格 (ディスパッチの動作に必要な範囲のみ)
- `dispatchCommand()` と設計書04 §3 の Dispatcher 表
- 登録前後のゲーティング (451 / 462 / 421 / 登録前無視)
- 単体テスト (ソケット不使用)

スコープ外: 各ハンドラの実装、MODE、ネットワーク I/O、main.cpp。

## ファイル構成 (設計書02 §12 に準拠)

```
prd/interface/Server.hpp           Server クラス宣言
prd/interface/Server.cpp           Constructor
prd/interface/ServerRelations.cpp  addClient / removeClient /
                                   findClientByFd / queueToClient
prd/interface/ServerDispatch.cpp   dispatchCommand + 表 + スタブハンドラ
tests/interface/dispatch/test_dispatch.cpp
```

メソッドの配置は設計書02 §12 のファイル分担 (Server.cpp / ServerRelations.cpp
/ ServerDispatch.cpp) に従う。

`prd/Makefile` の SRCS へ `interface/ServerDispatch.cpp` を追加する。
main() が無い移行期のため `all: $(OBJS)` は維持する。

## Server クラス骨格

```cpp
class Server
{
public:
    /* 設計書02 §4.3。socket 作成 (setupListeningSocket) はネットワーク層
       実装時に追加するため、ここではメンバ初期化のみ行う */
    Server(int port, const std::string &password);

    /* 設計書02 §4.6。失敗時 NULL、例外を送出しない */
    Client       *findClientByFd(int fd);
    const Client *findClientByFd(int fd) const;

    /* 設計書02 §4.9。末尾が CRLF でなければ CRLF を付与し、該当 Client の
       送信バッファへ追加する。send() は呼ばない。
       空文字を queue しないのは呼び出し側の責務 (設計書06 §17) */
    void queueToClient(int fd, const std::string &message);

    /* 設計書04 §3 のエントリポイント */
    void dispatchCommand(int fd, const Message &message);

    /* ── 移行期 API (設計書02 に存在しない) ──
       設計書の接続受付は acceptClient() (実 socket 前提) だが未実装のため、
       Client の登録・削除だけを分離した。ネットワーク担当は acceptClient()
       実装時にこれを下請けとして使用してよい。
       addClient: 同一 FD が既に存在すれば false。
                  insert(std::make_pair(...)) を使う (設計書02 §5.3)。
       removeClient: close() は行わない (FD の所有はネットワーク層) */
    bool addClient(int fd, const std::string &hostname);
    void removeClient(int fd);

private:
    typedef void (Server::*CommandHandler)(int fd, const Message &message);

    /* 設計書02 §4.10 の 14 ハンドラ。本スコープでは全て空スタブ */
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

    int                   _port;        /* 設計書02 §4.2 */
    std::string           _password;    /* 同上 */
    std::string           _serverName;  /* 固定値 "ircserv.local" (同上) */
    std::map<int, Client> _clients;     /* FD → Client 所有 Map (同上) */
};
```

設計書02 §4.2 の残りのメンバ (`_listenFd`, `_running`, `_pollFds`,
`_channels`, `_nickToFd`, `_pendingDisconnects`) は本スコープでは未使用のため
宣言しない (clang の `-Wunused-private-field` が `-Werror` で fail するため)。
ネットワーク層・ハンドラ実装時にそれぞれ追加する。

Dispatcher 表は `Server::dispatchCommand()` 内の static const 配列として
定義する (後述)。メンバ関数内なので private の `CommandHandler` typedef と
private ハンドラのポインタをそのまま参照でき、friend も public 公開も
不要である。

## Dispatcher

設計書04 §3 の表を `ServerDispatch.cpp` 内の静的配列で表現する:

```cpp
struct CommandEntry
{
    const char             *name;
    Server::CommandHandler  handler;
    bool                    requiresRegistration;  /* 登録前 → 451 */
    bool                    rejectsWhenRegistered; /* 登録後 → 462 */
};
```

| Command | requiresRegistration | rejectsWhenRegistered |
|---|---|---|
| PASS | false | true |
| NICK | false | false |
| USER | false | true |
| CAP / PING / PONG / QUIT | false | false |
| JOIN / PRIVMSG / KICK / INVITE / TOPIC / MODE / PART | true | false |

private メンバ関数ポインタは class 外の静的初期化子から取れないため、表は
`Server::dispatchCommand()` 内の static const 配列として定義する。

`dispatchCommand(fd, message)` の処理順:

1. `findClientByFd(fd)` — NULL なら何もせず return (設計書04 §4 処理順 1)
2. `message.command` で表を線形検索 (Parser が大文字化済み、単純比較)
3. 表に無い場合
   - 未登録: 無視 (設計書04 §3 — 初期交渉を妨げない)
   - 登録済み: `421 ERR_UNKNOWNCOMMAND` `<command> :Unknown command`
4. 表に有る場合
   - 未登録 かつ requiresRegistration:
     `451 ERR_NOTREGISTERED` `:You have not registered`
   - 登録済み かつ rejectsWhenRegistered:
     `462 ERR_ALREADYREGISTRED` `:Unauthorized command (already registered)`
   - それ以外: `(this->*handler)(fd, message)`

Numeric 本文は設計書06 §16 の表と一致させる。組み立ては必ず
`Reply::numeric(_serverName, *client, Numerics::…, params)` を経由し、
生の文字列連結で行を作らない (設計書06 §10, Parser.hpp の注意書き)。
421 の `<command>` は受信した command (大文字化済み) をそのまま使う。
未登録 Client への Numeric の target は Reply が `*` へ落とす (設計書06 §4)。

スタブハンドラは `(void)fd; (void)message;` のみの空実装
(`-Wall -Wextra -Werror` 対策)。

## エラー処理

- 存在しない FD への dispatch / queueToClient は何もしない (クラッシュしない)
- Reply が空文字を返すのは serverName 不正時のみで、`"ircserv.local"` は
  `IrcUtil::isSafeToken()` を満たすため実行時には発生しない。
  queueToClient 側での空文字チェックは設計書に無いため行わない

## テスト計画

`tests/interface/dispatch/test_dispatch.cpp` を既存の TestRunner 形式で追加し、
tests/Makefile へ組み込む。Client を登録済みにする手順は既存 API
(`acceptPassword()` → `setNickname()` → `setUser()` →
`tryCompleteRegistration()`) を使う。

| ケース | 期待 |
|---|---|
| 未登録 + JOIN 等 (7 種) | `:ircserv.local 451 * :You have not registered` + CRLF |
| 未登録 + 未知 Command | 送信バッファ空のまま |
| 未登録 + NICK / CAP / PING / PONG / QUIT | ゲート通過 (Numeric が積まれない) |
| 登録済み + PASS / USER | 462 `:Unauthorized command (already registered)` |
| 登録済み + 未知 Command | 421 `FOO :Unknown command` |
| 登録済み + JOIN 等 | ゲート通過 (スタブなのでバッファ空) |
| 存在しない FD | 何も起きない |
| queueToClient | CRLF 付与 / 既に CRLF 付きなら二重付与しない |
| addClient / removeClient / findClientByFd | 追加・重複 FD 拒否・削除・NULL |

## 設計書との差分 (意図的)

1. `addClient` / `removeClient` — 設計書02 に無い移行期 API (上述)
2. Constructor が `setupListeningSocket()` を呼ばない — ネットワーク層実装時に追加
3. Server メンバ変数の一部未宣言 — 使用時に追加 (上述)
