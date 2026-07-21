#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#include "../util/BufferUtil.hpp"
#include "../util/Parser.hpp"
#include "Server.hpp"

/* ============================================================
 * ネットワーク層 (設計書 03)
 *
 * TCP/IPv4、1 つの listen socket、1 つの poll() を繰り返すイベント
 * ループ、全 socket ノンブロッキング、poll 走査中の削除は遅延実行、
 * signal handler は終了フラグだけを変更する (設計書 03 §1)。
 * ============================================================ */
namespace
{
    const std::size_t RECV_CHUNK_SIZE    = 4096;
    const std::size_t MAX_RECEIVE_BUFFER = 65536;
    const int         LISTEN_BACKLOG     = SOMAXCONN;
    const int         POLL_TIMEOUT_MS    = -1;

    /* signal handler は非同期 signal-safe な操作 (フラグ代入) だけを
       行う (設計書 03 §4) */
    volatile sig_atomic_t g_stopRequested = 0;

    void handleStopSignal(int)
    {
        g_stopRequested = 1;
    }
}

/* ── listen socket 初期化 (設計書 03 §3) ──────────────────── */

void Server::setupListeningSocket()
{
    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0)
        throw std::runtime_error("socket() failed");

    int enabled = 1;

    setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &enabled,
              sizeof(enabled));

    /* macOS 課題制約に従い、fcntl() はこの形式以外で使用しない
       (設計書 03 §3) */
    if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
    {
        close(_listenFd);
        _listenFd = -1;
        throw std::runtime_error("fcntl() failed");
    }

    struct sockaddr_in address;

    std::memset(&address, 0, sizeof(address));
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port        = htons(static_cast<unsigned short>(_port));

    if (bind(_listenFd, reinterpret_cast<struct sockaddr *>(&address),
            sizeof(address)) < 0)
    {
        close(_listenFd);
        _listenFd = -1;
        throw std::runtime_error("bind() failed");
    }

    if (listen(_listenFd, LISTEN_BACKLOG) < 0)
    {
        close(_listenFd);
        _listenFd = -1;
        throw std::runtime_error("listen() failed");
    }

    struct pollfd pfd;

    pfd.fd      = _listenFd;
    pfd.events  = POLLIN;
    pfd.revents = 0;
    _pollFds.push_back(pfd);

    std::cout << "ircserv: listening on port " << _port << std::endl;
}

/* ── 起動・停止 (設計書 02 §4.4) ──────────────────────────── */

void Server::run()
{
    /* _listenFd 未設定 (単体テストではメンバ初期化のみで構築される) な
       ときだけ socket を用意する。意図的な設計書との差分は Server.hpp
       のクラスコメントに記載 */
    if (_listenFd < 0)
        setupListeningSocket();

    std::signal(SIGINT, handleStopSignal);
    std::signal(SIGTERM, handleStopSignal);

    eventLoop();
}

void Server::stop()
{
    _running = false;
}

/* ── pollfd 管理 (設計書 03 §5) ───────────────────────────── */

void Server::addClientPollFd(int fd)
{
    struct pollfd pfd;

    pfd.fd      = fd;
    pfd.events  = POLLIN;
    pfd.revents = 0;
    _pollFds.push_back(pfd);
}

void Server::removeClientPollFd(int fd)
{
    for (std::vector<struct pollfd>::iterator it = _pollFds.begin();
         it != _pollFds.end(); ++it)
    {
        if (it->fd == fd)
        {
            _pollFds.erase(it);
            return;
        }
    }
}

void Server::updatePollEvents(int fd)
{
    const Client *client = findClientByFd(fd);

    if (client == NULL)
        return;

    short events = POLLIN;

    if (client->hasPendingOutput())
        events |= POLLOUT;

    for (std::size_t i = 0; i < _pollFds.size(); ++i)
    {
        if (_pollFds[i].fd == fd)
        {
            _pollFds[i].events = events;
            return;
        }
    }
}

/* ── イベントループ (設計書 03 §6) ────────────────────────── */

void Server::eventLoop()
{
    while (_running && !g_stopRequested)
    {
        int ready = poll(&_pollFds[0], _pollFds.size(), POLL_TIMEOUT_MS);

        if (ready < 0)
        {
            if (g_stopRequested)
                break;
            throw std::runtime_error("poll() failed");
        }

        /* pollfd 要素への参照は保持しない。新規 Client 追加で vector が
           再配置されても、保存した FD 値だけを使用する (03 §6) */
        std::size_t snapshotSize = _pollFds.size();

        for (std::size_t i = 0; i < snapshotSize; ++i)
        {
            int   fd      = _pollFds[i].fd;
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
}

/* ── listen FD イベント (設計書 03 §7〜§8) ────────────────── */

void Server::handleListenEvent(short revents)
{
    if (revents & static_cast<short>(POLLIN))
        acceptClient();
    if (revents & static_cast<short>(POLLERR | POLLHUP | POLLNVAL))
        throw std::runtime_error("listen socket error");
}

void Server::acceptClient()
{
    struct sockaddr_in peer;
    socklen_t          peerLength = sizeof(peer);

    std::memset(&peer, 0, sizeof(peer));

    int clientFd = accept(_listenFd,
                          reinterpret_cast<struct sockaddr *>(&peer),
                          &peerLength);

    /* 失敗なら状態を変更せず戻る (設計書 03 §8) */
    if (clientFd < 0)
        return;

    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
    {
        close(clientFd);
        return;
    }

    char        ipBuffer[INET_ADDRSTRLEN];
    const char *ipResult =
        inet_ntop(AF_INET, &peer.sin_addr, ipBuffer, sizeof(ipBuffer));
    std::string peerAddress = (ipResult != NULL) ? ipBuffer : "0.0.0.0";

    /* Client 生成は既存の addClient() を再利用する (insert(std::
       make_pair(...)) を使う、設計書 02 §5.3) */
    if (!addClient(clientFd, peerAddress))
    {
        close(clientFd);
        return;
    }
    addClientPollFd(clientFd);

    std::cout << "ircserv: accepted fd=" << clientFd << " host="
              << peerAddress << std::endl;
}

/* ── Client イベント (設計書 03 §9) ───────────────────────── */

void Server::handleClientEvent(int fd, short revents)
{
    if (findClientByFd(fd) == NULL)
        return;

    if (revents & static_cast<short>(POLLNVAL))
    {
        scheduleDisconnect(fd);
        return;
    }

    if (revents & static_cast<short>(POLLIN))
    {
        receiveFromClient(fd);
        if (_pendingDisconnects.find(fd) != _pendingDisconnects.end())
            return;
    }

    if (revents & static_cast<short>(POLLOUT))
    {
        flushSendBuffer(fd);
        if (_pendingDisconnects.find(fd) != _pendingDisconnects.end())
            return;
    }

    if (revents & static_cast<short>(POLLERR | POLLHUP))
        scheduleDisconnect(fd);
}

/* ── 受信処理 (設計書 03 §10〜§13) ────────────────────────── */

void Server::receiveFromClient(int fd)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;

    char    buffer[RECV_CHUNK_SIZE];
    ssize_t received = recv(fd, buffer, sizeof(buffer), 0);

    if (received == 0 || received < 0)
    {
        scheduleDisconnect(fd);
        return;
    }

    client->appendReceiveBuffer(buffer, static_cast<std::size_t>(received));

    if (client->getReceiveBuffer().size() > MAX_RECEIVE_BUFFER)
    {
        queueToClient(fd, "ERROR :Receive buffer limit exceeded");
        scheduleDisconnect(fd);
        return;
    }

    std::string line;
    std::size_t consumed;
    bool        done = false;

    while (!done)
    {
        switch (BufferUtil::findLine(client->getReceiveBuffer(), line,
                                     consumed))
        {
        case BufferUtil::LINE_EXTRACTED:
            client->eraseReceivePrefix(consumed);
            processLine(fd, line);
            /* 切断予約されたら残り行の処理を停止する (03 §10) */
            if (_pendingDisconnects.find(fd) != _pendingDisconnects.end())
                done = true;
            break;
        case BufferUtil::LINE_TOO_LONG:
            client->eraseReceivePrefix(consumed);
            queueToClient(fd, "ERROR :Input line too long");
            scheduleDisconnect(fd);
            done = true;
            break;
        case BufferUtil::LINE_INCOMPLETE:
            done = true;
            break;
        }
    }
}

void Server::processLine(int fd, const std::string &line)
{
    Message message;

    if (!Parser::parse(line, message))
        return;
    dispatchCommand(fd, message);
}

/* ── 送信 (設計書 03 §15) ─────────────────────────────────── */

void Server::flushSendBuffer(int fd)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;

    const std::string &out = client->getSendBuffer();

    if (!out.empty())
    {
        ssize_t sent = send(fd, out.data(), out.size(), 0);

        if (sent > 0)
            client->eraseSendPrefix(static_cast<std::size_t>(sent));
        else
        {
            scheduleDisconnect(fd);
            return;
        }
    }
    /* 送信後にバッファが空なら POLLOUT を解除する。残っていれば次回
       poll() まで保持する (03 §15) */
    updatePollEvents(fd);
}
