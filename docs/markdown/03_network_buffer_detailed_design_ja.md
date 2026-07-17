# ft_irc Mandatory ネットワーク・バッファ詳細設計書

## 1. 文書の目的と固定値

本書は、TCP socket、ノンブロッキングI/O、`poll()`イベントループ、受信バッファ、送信バッファ、切断処理を実装するための詳細設計を定義する。

採用方式:

- TCP/IPv4
- 1つのlisten socket
- 1つの`poll()`呼び出しを繰り返すイベントループ
- 全socketをノンブロッキングに設定
- Clientごとの受信バッファと送信バッファ
- poll走査中の削除を遅延実行
- signal handlerは終了フラグだけを変更

固定値:

```cpp
static const std::size_t RECV_CHUNK_SIZE = 4096;
static const std::size_t MAX_RECEIVE_BUFFER = 65536;
static const std::size_t MAX_SEND_BUFFER = 1048576;
static const int LISTEN_BACKLOG = SOMAXCONN;
static const int POLL_TIMEOUT_MS = -1;
```

行長の定数は`IrcUtil`が持つ。Serverは再定義せず、これを参照する。

```cpp
namespace IrcUtil
{
    const std::size_t IRC_MAX_LINE = 512;
    const std::size_t IRC_MAX_CONTENT = 510;
}
```

`IRC_MAX_LINE`はCRLFを含む。`IRC_MAX_CONTENT`はCommandとParameterに使用できる最大長であり、`IRC_MAX_LINE - 2`である。

## 2. 起動引数の検証

実行形式:

```bash
./ircserv <port> <password>
```

検証順序:

1. `argc == 3`を確認する
2. port文字列が空でないことを確認する
3. port文字列の全文字がASCII数字であることを確認する
4. overflowを避けながら整数へ変換する
5. 1から65535の範囲であることを確認する
6. passwordが空でないことを確認する
7. `Server`を生成して`run()`を呼ぶ

不正な場合:

- Usageを標準エラーへ出力する
- socketを作成せず終了コード1で終了する

```text
Usage: ./ircserv <port> <password>
```

## 3. listen socket初期化

処理順序:

```text
socket(AF_INET, SOCK_STREAM, 0)
  -> setsockopt(SO_REUSEADDR)
  -> fcntl(F_SETFL, O_NONBLOCK)
  -> bind(INADDR_ANY, port)
  -> listen(SOMAXCONN)
  -> pollfdへ登録
```

socket作成:

```cpp
_listenFd = socket(AF_INET, SOCK_STREAM, 0);
```

`_listenFd < 0`なら初期化失敗とする。

Address再利用:

```cpp
int enabled = 1;
setsockopt(_listenFd,
           SOL_SOCKET,
           SO_REUSEADDR,
           &enabled,
           sizeof(enabled));
```

ノンブロッキング設定:

```cpp
fcntl(_listenFd, F_SETFL, O_NONBLOCK);
```

macOS課題制約に従い、`fcntl()`はこの形式以外で使用しない。

bind address:

```cpp
struct sockaddr_in address;
address.sin_family = AF_INET;
address.sin_addr.s_addr = htonl(INADDR_ANY);
address.sin_port = htons(_port);
```

初期pollfd:

```cpp
struct pollfd pfd;
pfd.fd = _listenFd;
pfd.events = POLLIN;
pfd.revents = 0;
_pollFds.push_back(pfd);
```

途中失敗時:

1. `_listenFd`が有効なら`close()`する
2. `_listenFd = -1`へ戻す
3. `std::runtime_error`を送出する

## 4. signalとServer終了

対象signal:

- `SIGINT`
- `SIGTERM`

signal handlerでは非同期signal-safeではない処理を行わない。

```cpp
volatile sig_atomic_t g_stopRequested = 0;

void handleSignal(int)
{
    g_stopRequested = 1;
}
```

イベントループは`poll()`から戻った後に`g_stopRequested`を確認する。

ただし`poll()`が無期限待機中にsignalを受けた場合は`-1`で戻る。停止要求が立っていれば正常終了へ進み、それ以外は致命的pollエラーとする。

終了処理:

1. `_running = false`
2. Clientへの追加送信は行わない
3. 全Client FDを閉じる
4. listen FDを閉じる
5. コンテナを破棄する

## 5. pollfd管理

pollfdの構成:

```text
index 0: listen FD
index 1以降: Client FD
```

Client追加:

```cpp
void Server::addClientPollFd(int fd)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pollFds.push_back(pfd);
}
```

Client削除:

- poll走査中には`_pollFds.erase()`しない
- `_pendingDisconnects`へFDを追加する
- イベント走査終了後にFDを検索して削除する

送信待ち状態によるevents更新:

```text
sendBufferが空:
    events = POLLIN

sendBufferが非空:
    events = POLLIN | POLLOUT
```

`POLLERR`、`POLLHUP`、`POLLNVAL`はeventsへ設定しなくてもreventsで通知される。

## 6. イベントループ

擬似コード:

```cpp
while (_running && !g_stopRequested)
{
    int ready = poll(&_pollFds[0],
                     _pollFds.size(),
                     POLL_TIMEOUT_MS);

    if (ready < 0)
    {
        if (g_stopRequested)
            break;
        throw std::runtime_error("poll failed");
    }

    std::size_t snapshotSize = _pollFds.size();

    for (std::size_t i = 0; i < snapshotSize; ++i)
    {
        int fd = _pollFds[i].fd;
        short revents = _pollFds[i].revents;

        if (revents == 0)
            continue;

        if (fd == _listenFd)
            handleListenEvent(revents);
        else
            handleClientEvent(fd, revents);
    }

    processPendingDisconnects();
}
```

重要事項:

- `poll()`呼び出し前にvectorが空でないことを保証する
- pollfd要素への参照を保持しない
- 新規Client追加でvectorが再配置されても、保存したFD値だけを使用する
- 新規追加Clientは次の`poll()`から処理する
- 切断予約済みFDは追加Handler処理を行わない

図の元データ:

- `../diagrams/poll_iteration_detail_ja.mmd`

## 7. listen FDイベント

処理規則:

| revents | 処理 |
|---|---|
| `POLLIN` | `acceptClient()`を1回呼ぶ |
| `POLLERR` | Server致命的エラー |
| `POLLHUP` | Server致命的エラー |
| `POLLNVAL` | Server致命的エラー |

課題要件に合わせ、1回の`POLLIN`通知で`accept()`を繰り返さない。accept queueに複数接続が残っている場合は次回`poll()`でも`POLLIN`になる。

## 8. Client受付

```cpp
void Server::acceptClient();
```

処理順序:

1. `sockaddr_in`と長さを初期化する
2. `accept()`を1回呼ぶ
3. 失敗なら状態を変更せず戻る
4. Client FDをノンブロッキングにする
5. `inet_ntop()`で接続元IPv4文字列を得る
6. `_clients.insert()`でClientを生成する
7. pollfdを追加する
8. 接続ログを出す

```cpp
int clientFd = accept(_listenFd,
                      reinterpret_cast<struct sockaddr *>(&peer),
                      &peerLength);
```

accept成功後にノンブロッキング設定が失敗した場合:

- Client FDを閉じる
- `_clients`と`_pollFds`へ追加しない
- Serverは継続する

Client生成:

```cpp
_clients.insert(std::make_pair(
    clientFd,
    Client(clientFd, peerAddress)
));
```

## 9. Clientイベントの優先順序

```cpp
void Server::handleClientEvent(int fd, short revents);
```

処理順序:

1. FDがClient Mapに存在するか確認する
2. `POLLNVAL`なら切断予約して終了する
3. `POLLIN`なら受信処理を1回行う
4. 受信処理で切断予約された場合は終了する
5. `POLLOUT`なら送信処理を1回行う
6. 送信処理で切断予約された場合は終了する
7. `POLLERR`または`POLLHUP`なら切断予約する

`POLLIN`と`POLLHUP`が同時の場合、先に残存データを1回受信してから切断する。

## 10. 受信処理

```cpp
void Server::receiveFromClient(int fd);
```

1回のready通知につき`recv()`を1回だけ呼ぶ。

```cpp
char buffer[RECV_CHUNK_SIZE];
ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
```

戻り値:

| 値 | 処理 |
|---|---|
| `> 0` | Client受信バッファへ追記して完成行を処理 |
| `== 0` | EOFとして切断予約 |
| `< 0` | 受信エラーとして切断予約 |

`recv()`後に`errno == EAGAIN`を見て再受信しない。

受信後:

1. `MAX_RECEIVE_BUFFER`超過を確認する
2. 完成した行を先頭からすべて切り出す
3. 各行の長さを確認する
4. Parserへ渡す
5. MessageをDispatcherへ渡す
6. 切断予約されたら残り行の処理を停止する

## 11. IRC行の切り出し

行の切り出しは`BufferUtil`が行う。クラス詳細設計書の10.1を参照する。

```cpp
BufferUtil::LineStatus BufferUtil::findLine(
    const std::string &buffer,
    std::string &out,
    std::size_t &consumed
);
```

`findLine()`はbufferを変更しない。取り除くべきバイト数を`consumed`へ返し、Serverが`Client::eraseReceivePrefix(consumed)`で取り除く。

受信バッファではLFを行終端として検索する。LF直前がCRならCRも終端として除去する。

```text
"PASS secret\r\nNICK alice\r\nPAR"

切り出し:
1. "PASS secret"
2. "NICK alice"

残存:
"PAR"
```

`findLine()`の擬似コード:

```cpp
pos = buffer.find('\n');

if (pos == npos)
{
    // LFがまだ来ていない
    if (buffer.size() >= IrcUtil::IRC_MAX_LINE)
    {
        consumed = buffer.size();
        return LINE_TOO_LONG;
    }
    return LINE_INCOMPLETE;
}

consumed = pos + 1;
raw = buffer.substr(0, pos);

if (!raw.empty() && raw[raw.size() - 1] == '\r')
    raw.erase(raw.size() - 1);

// 長さ判定は終端除去後の本文に対して行う
if (raw.size() > IrcUtil::IRC_MAX_CONTENT)
    return LINE_TOO_LONG;

out = raw;
return LINE_EXTRACTED;
```

Server側の呼び出し:

```cpp
while (true)
{
    switch (BufferUtil::findLine(client.getReceiveBuffer(), line, consumed))
    {
    case BufferUtil::LINE_EXTRACTED:
        client.eraseReceivePrefix(consumed);
        processLine(fd, line);   // 空行はParserが破棄する
        break;
    case BufferUtil::LINE_TOO_LONG:
        queueToClient(fd, "ERROR :Input line too long");
        scheduleDisconnect(fd);
        return;
    case BufferUtil::LINE_INCOMPLETE:
        return;
    }
}
```

空行は`LINE_EXTRACTED`かつ`out`が空文字として返る。空行の破棄は`Parser::parse()`が`false`を返すことで行う。

長さ判定は、終端を除去した後の本文が`IRC_MAX_CONTENT`(510 byte)以下かで行う。LFだけで終端された行も、仮想的なCRLFを含めて512 byte以内と判断するためである。Error・Reply詳細設計書の13章と一致させる。

LFのみを終端とする行に対して`raw.size() > IRC_MAX_LINE`(LF込み)で判定すると、本文511 byte + LFの512 byteを受理してしまい、本文の上限510 byteを超える。

LFがない未完成行は、バッファが512 byte以上になった時点で切断する。この先どこでLFが来ても、本文が511 byte以上になることが確定するためである。

511 byteでは切断しない。正当な最大長行「本文510 byte + CRLF」は、CRまで受信してLF待ちの状態でちょうど511 byteになる。511 byteで切断すると、この行が分割受信されたときに誤って切断してしまう。

図の元データ:

- `../diagrams/receive_buffer_detail_ja.mmd`

## 12. 受信Buffer上限

異常入力への防御:

| 条件 | 処理 |
|---|---|
| 終端除去後の本文が510 byte超 | `ERROR :Input line too long`を可能ならqueueし切断予約 |
| LFなしでBufferが512 byte以上 | 同上 |
| Receive Bufferが65536 byte超 | `ERROR :Receive buffer limit exceeded`をqueueし切断予約 |
| NULを含む | 該当行を不正入力として破棄 |

切断通知をqueueしても、即時送信は保証しない。安全性を優先し、切断処理で未送信のERRORが失われることを許容する。

## 13. Parser呼び出しとDispatcher

```cpp
void Server::processLine(int fd, const std::string &line)
{
    Message message;
    if (!Parser::parse(line, message))
        return;
    dispatchCommand(fd, message);
}
```

1回の`recv()`で複数行を受信した場合、同じevent処理内で完成行をすべて順番に処理してよい。課題が禁止しているのはready通知なしで`recv()`を繰り返すことであり、既にメモリ上にある完成行の処理はI/Oではない。

## 14. 送信Queue

すべてのIRC Replyと通知はClientの送信バッファへ追加する。

```cpp
void Server::queueToClient(int fd,
                           const std::string &message);
```

処理:

1. Client存在確認
2. message末尾のCRLFを正規化
3. 追加後の長さが`MAX_SEND_BUFFER`以下か確認
4. Clientの送信バッファへ追加
5. pollfdの`POLLOUT`を有効化

複数messageの順序はqueueされた順序を保つ。

```text
001 Welcome\r\n
002 Your host...\r\n
003 Created...\r\n
004 My info...\r\n
```

## 15. 部分送信

```cpp
void Server::flushSendBuffer(int fd);
```

1回のready通知につき`send()`を1回だけ呼ぶ。

```cpp
const std::string &out = client.getSendBuffer();
ssize_t sent = send(fd, out.data(), out.size(), 0);
```

戻り値:

| 値 | 処理 |
|---|---|
| `> 0` | 送信済みprefixを削除 |
| `== 0` | 切断予約 |
| `< 0` | 送信エラーとして切断予約 |

送信後にバッファが空なら`POLLOUT`を解除する。残っていれば次回`poll()`まで保持する。

`send()`後に`errno`を見て同じevent内で再送しない。

図の元データ:

- `../diagrams/send_buffer_detail_ja.mmd`

## 16. Send Buffer上限

送信待ちが1MiBを超えるClientは、読み取り速度が極端に遅いか応答不能と判断する。

処理:

1. `queueToClient()`で上限超過を検出する
2. 追加予定messageは破棄する
3. `scheduleDisconnect(fd)`を呼ぶ
4. 他ClientとServerは継続する

チャンネルbroadcast中に1Clientだけ上限を超えた場合、他Clientへのqueueは継続する。

## 17. 切断予約と遅延削除

切断原因:

- `QUIT`
- `recv() == 0`
- `recv() < 0`
- `send() <= 0`
- `POLLERR`
- `POLLHUP`
- `POLLNVAL`
- Buffer上限超過
- 行長超過

```cpp
void Server::scheduleDisconnect(int fd)
{
    _pendingDisconnects.insert(fd);
}
```

同じFDが複数原因で予約されてもSetにより1回だけ処理する。

イベント走査終了後:

```cpp
void Server::processPendingDisconnects()
{
    std::set<int> pending = _pendingDisconnects;
    _pendingDisconnects.clear();

    for (each fd in pending)
        disconnectClient(fd, "Connection closed");
}
```

## 18. disconnectClientの順序

```cpp
void Server::disconnectClient(
    int fd,
    const std::string &reason
);
```

処理順序:

1. Client存在確認
2. Client PrefixとQUIT messageを生成する
3. 参加中Channel名をコピーする
4. 共有Channelの他Member FDを重複なしで収集する
5. 他MemberへQUIT通知をqueueする
6. 各参加ChannelからMember、Operatorを削除する
7. 全ChannelのInvite集合からFDを削除する
8. Clientの参加Channel集合を空にする
9. 空Channelを削除する
10. Nickname索引を削除する
11. pollfd一覧からFDを削除する
12. `close(fd)`する
13. Client MapからClientを削除する

図の元データ:

- `../diagrams/disconnect_order_detail_ja.mmd`

FD再利用対策:

- `close()`より前にpollfdを削除する
- Client Mapから削除した後は古いFDを参照しない
- 新規`accept()`で同じ整数FDが再利用されても、新しいClientとして登録される

## 19. Channel broadcastと送信順序

Channel通知の基本順序:

1. 状態検証
2. 通知文字列を生成
3. 必要な状態変更
4. 対象FDへqueue

JOINでは、追加後のMember全員へJOIN通知を送る。

PART、KICK、QUITでは、削除前に通知先Memberを確定する。

PRIVMSGでは、送信者を除く現在のMemberへqueueする。

## 20. ログ方針

標準出力:

- Server起動
- 接続受付
- Client登録完了
- Client切断
- Channel生成と削除

標準エラー:

- system call失敗
- 不変条件違反
- 予期しない内部状態

PasswordとPRIVMSG本文は通常ログへ出力しない。

## 21. ネットワークAPI一覧

| 関数 | I/O実行 | poll ready必須 |
|---|---|---|
| `setupListeningSocket()` | socket/bind/listen | 起動時のみ |
| `acceptClient()` | accept | listen FDのPOLLIN |
| `receiveFromClient()` | recv | Client FDのPOLLIN |
| `flushSendBuffer()` | send | Client FDのPOLLOUT |
| `queueToClient()` | なし | 不要 |
| `processLine()` | なし | 不要 |
| `disconnectClient()` | close | 切断処理 |

## 22. 実装完了条件

- listen socketと全Client socketがノンブロッキングである
- 1つの`poll()`だけでlisten、read、writeを監視する
- ready通知なしで`accept()`、`recv()`、`send()`を呼ばない
- 1ready通知で同じI/O関数を繰り返さない
- 分割受信を復元できる
- 1回の受信に含まれる複数Commandを順番に処理できる
- 部分送信後の残りを次回POLLOUTまで保持できる
- pollfd走査中にvector要素を削除しない
- 異常切断時もClient、Channel、Nickname索引、FDが整理される
- Receive BufferとSend Bufferの上限が機能する
- Server終了時にFDリークがない

