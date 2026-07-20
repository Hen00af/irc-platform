#pragma once

#include <map>
#include <string>

#include "../domain/Channel.hpp"
#include "../domain/Client.hpp"
#include "../domain/Message.hpp"

/* ============================================================
 * Server
 *
 * Client の所有と Command のディスパッチを担う (設計書 02 §4)。
 *
 * 移行期の骨格である。ネットワーク層 (listen socket・poll ループ・
 * ServerNetwork.cpp) はまだ持たない。設計書 02 §4.2 のうち未使用の
 * メンバ (_listenFd, _running, _pollFds, _pendingDisconnects) は、
 * それらを使う層の実装時に追加する。設計書 02 §4.3 の Destructor
 * (全 Client FD と _listenFd の close) も同時に追加すること。現状は
 * FD を所有しないため Default の Destructor で正しい。
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

    /* Nickname 検索 (設計書 02 §4.6)。ircCaseFold した Key で索引を引く。
       失敗時は NULL */
    Client       *findClientByNickname(const std::string &nickname);
    const Client *findClientByNickname(const std::string &nickname) const;

    /* Channel 検索 (設計書 02 §4.6)。normalizeChannelName した Key で
       Map を引く。失敗時は NULL */
    Channel       *findChannel(const std::string &name);
    const Channel *findChannel(const std::string &name) const;

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
    std::map<std::string, Channel> _channels;  /* 正規化名 → Channel (02 §4.2) */
    std::map<std::string, int>     _nickToFd;  /* 正規化 Nick → FD (02 §4.2) */
    std::string                    _serverStartTime; /* 003 用 (06 §6) */
};
