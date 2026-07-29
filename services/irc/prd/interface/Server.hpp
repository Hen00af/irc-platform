#pragma once

#include <map>
#include <poll.h>
#include <set>
#include <string>
#include <vector>

#include "../domain/Channel.hpp"
#include "../domain/Client.hpp"
#include "../domain/Message.hpp"

/* ============================================================
 * Server
 *
 * Client の所有・Command のディスパッチ・ネットワーク I/O を担う
 * (設計書 02 §4, 03)。
 *
 * Constructor はメンバ初期化のみを行い、socket は作成しない (移行期に
 * 単体テストが実 socket なしで Server を構築できるようにするための
 * 意図的な設計書との差分。設計書 02 §4.3 は Constructor が
 * setupListeningSocket() を呼ぶとするが、本実装では run() が
 * _listenFd < 0 のときに限り呼ぶ)。main.cpp から Server を構築して
 * run() を呼ぶことで、設計書どおり listen 開始からイベントループまでが
 * 一連の呼び出しになる。
 * ============================================================ */
class Server
{
public:
    /* 設計書 02 §4.3。socket 作成 (setupListeningSocket) は run() が
       行う (上記の意図的な差分)。ここではメンバ初期化のみ行う */
    Server(int port, const std::string &password);

    /* 設計書 02 §4.3。開いている全 Client FD と listen FD を close()
       する。IRC 通知は送信しない */
    ~Server();

    /* ── 起動・停止 (設計書 02 §4.4) ────────
       run(): _listenFd が未設定なら setupListeningSocket() を呼んでから
       イベントループへ入る。失敗時は std::runtime_error を送出する
       stop(): _running を false にする。次回ループ先頭で終了する */
    void run();
    void stop();

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

    /* ── 切断予約の処理 (設計書 03 §17) ──────
       ネットワーク層のイベントループ末尾から呼ぶ想定。設計書 02 §4.5
       では private だが、poll ループ実装前にテストから駆動できるよう
       public にする (spec の決定事項)。_pendingDisconnects をコピーして
       Member を空にしてから各 FD を disconnectClient() へ渡す。理由は
       _disconnectReasons に保存済みならそれを、無ければ
       "Connection closed" を使う */
    void processPendingDisconnects();

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
    /* Channel の Member FD を走査して queue する。excludeFd == -1 なら
       誰も除外しない (設計書 02 §4.9) */
    void broadcastToChannel(const Channel &channel,
                            const std::string &message, int excludeFd);

    /* ── 所属関係 (設計書 02 §4.7) ──────────
       joinChannel(): Channel へ Member FD を追加し、Client へ正規化
       Channel 名を追加する。存在確認は Handler が Channel 生成も含めて
       済ませている前提。Client か Channel が不在なら false
       leaveChannel(): 通知文字列を先に組み立て、sendPart なら削除前の
       全 Member (退出 Client 自身を含む) へ PART 通知を broadcast した
       あとに Member/Operator/Invite/所属を削除する。Channel が空になれば
       削除する。Client 未参加なら false で何もしない */
    bool joinChannel(int clientFd, const std::string &channelKey);
    bool leaveChannel(int clientFd, const std::string &channelKey,
                      const std::string &reason, bool sendPart);
    /* Channel が空になった時点で Map から削除する (設計書 02 §4.7) */
    void deleteChannelIfEmpty(const std::string &channelKey);
    /* disconnectClient() から呼ばれる、Channel 側の関係整理だけを行う
       部分 (設計書 02 §4.7, 03 §18 手順 6〜9)。参加中 Channel 名を
       コピーしてから走査し (removeJoinedChannel() で集合を変更するため
       Iterator 無効化を避ける)、各 Channel から Member/Operator を削除
       して空なら削除する。Client は Channel に参加していなくても Invite
       され得るため、Invite 集合の削除は参加中に限らず全 Channel を対象
       にする。QUIT 通知は disconnectClient() が事前に broadcast 済み
       のため、quitMessage はここでは使わない */
    void removeClientFromAllChannels(int clientFd,
                                     const std::string &quitMessage);

    /* ── 切断 (設計書 03 §17〜§18) ──────────
       scheduleDisconnect(): 同じ FD が複数原因で予約されても Set により
       1 回だけ処理される (設計書 03 §17)。
       disconnectClient(): Client 確認 → QUIT 通知生成・共有 Member への
       queue → removeClientFromAllChannels() で Channel 関係を整理 →
       Nickname 索引削除 → pollfd 削除・close(fd) →
       _disconnectReasons・Client Map から削除、の順序で行う
       (設計書 03 §18) */
    void scheduleDisconnect(int fd);
    void disconnectClient(int fd, const std::string &reason);

    /* ── ネットワーク層 (設計書 03) ──────────
       setupListeningSocket(): socket/setsockopt/fcntl/bind/listen の順
       (§3)。失敗時は開いた FD を閉じて std::runtime_error を送出する。
       eventLoop(): 1 つの poll() を繰り返す (§6)。走査中に _pollFds を
       変更しない。新規 Client は次回 poll() から処理する。
       acceptClient(): listen FD の POLLIN で 1 回だけ accept() する
       (§7〜§8)。ノンブロッキング化に失敗したら Client を破棄して継続
       する。
       handleListenEvent()/handleClientEvent(): revents に応じた処理の
       振り分け (§7, §9)。切断予約済みなら追加の I/O を行わない。
       receiveFromClient(): 1 回の ready 通知につき recv() を 1 回だけ
       呼ぶ (§10)。MAX_RECEIVE_BUFFER 超過は ERROR を queue して切断
       予約する (§12)。BufferUtil::findLine() で完成行を切り出し、
       processLine() へ渡す (§11, §13)。
       flushSendBuffer(): 1 回の ready 通知につき send() を 1 回だけ
       呼ぶ (§15)。送信後に送信バッファが空なら POLLOUT を解除する。
       processLine(): Parser::parse() → dispatchCommand() (§13)。
       updatePollEvents(): 該当 fd の pollfd.events を
       Client::hasPendingOutput() に応じて POLLIN か POLLIN|POLLOUT へ
       更新する (§5, §14)。addClientPollFd()/removeClientPollFd(): 追加
       は末尾へ push、削除は該当 fd を探して erase する (§5, §18) */
    void setupListeningSocket();
    void eventLoop();
    void acceptClient();
    void handleListenEvent(short revents);
    void handleClientEvent(int fd, short revents);
    void receiveFromClient(int fd);
    void flushSendBuffer(int fd);
    void processLine(int fd, const std::string &line);
    void updatePollEvents(int fd);
    void addClientPollFd(int fd);
    void removeClientPollFd(int fd);

    /* ── 共通検証 (設計書 04 §4) ────────────
       非 Member / 非 Operator なら該当 Numeric を queue して false */
    bool requireChannelMember(int fd, const Channel &channel);
    bool requireChannelOperator(int fd, const Channel &channel);

    /* 403 ERR_NOSUCHCHANNEL を queue する共通 Helper (設計書 06 §8)。
       JOIN/PART/KICK/INVITE/TOPIC/PRIVMSG で重複していた本文を集約する。
       channelName は各呼び出し元が受け取った表示名をそのまま使う */
    void sendNoSuchChannel(int fd, const Client &client,
                           const std::string &channelName);

    /* ── 登録 (設計書 04 §5) ────────────────
       PASS/NICK/USER の各 Handler の最後で呼ぶ。この呼び出しで初めて
       登録完了したときだけ Welcome Sequence を送る */
    void tryRegisterClient(int fd);
    /* 001 002 003 004 422 を順に queue する (設計書 06 §6) */
    void sendWelcomeSequence(Client &client);

    /* ── 共通検証 (設計書 04 §4) ────────────
       Parameter 不足なら 461 を queue して false を返す */
    bool requireParams(int fd, const Message &message, std::size_t count);

    /* 設計書 02 §4.10 の Command Handler 群。全て handler/ 配下に
       実装済み (スタブは残っていない) */
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
    std::set<int>              _pendingDisconnects; /* 切断予約 FD (02 §4.2) */
    std::map<int, std::string> _disconnectReasons;  /* FD → QUIT 理由。
        04 §16 の「理由を保存して切断予約する」と 03 §17 の固定文言
        "Connection closed" を両立させるための Map (本 spec の決定事項) */

    int  _listenFd; /* 接続受付専用 FD。未初期化は -1 (02 §4.2, 03 §3) */
    bool _running;  /* イベントループ継続フラグ (02 §4.2) */
    std::vector<struct pollfd> _pollFds; /* index 0 = listen FD、以降は
        Client FD (03 §5) */
};
