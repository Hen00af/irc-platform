#pragma once

#include <string>
#include <map>
#include <vector>
#include <poll.h>

#include "../domain/Client.hpp"
#include "../domain/Channel.hpp"

/* ============================================================
 * Server
 *
 * poll() ベースの IRC サーバー。
 *
 * 責務:
 *   - listen ソケットのセットアップ
 *   - poll ループ (新規接続・データ受信・送信)
 *   - IRC メッセージのパースとコマンドディスパッチ
 *   - クライアント / チャンネルのライフサイクル管理
 * ============================================================ */
class Server
{
public:
    Server(int port, const std::string& password);
    ~Server();

    /* メインループ */
    void run();
    /* シグナルを受けてループを停止する */
    void stop();

private:
    /* ── サーバー状態 ─────────────────────── */
    int                         _port;
    std::string                 _password;
    int                         _serverFd;
    bool                        _running;

    std::vector<struct pollfd>      _fds;
    std::map<int, Client*>          _clients;   /* fd → Client */
    std::map<std::string, Channel*> _channels;  /* name → Channel */

    /* ── ネットワーク層 ───────────────────── */
    void _setupServer();
    void _acceptClient();
    void _recvData(int fd);
    void _sendData(int fd);
    void _disconnectClient(int fd, const std::string& reason = "");

    /* pollfd 管理 */
    void _addPollFd(int fd, short events);
    void _removePollFd(int fd);
    void _setPollOut(int fd, bool enable);

    /* ── IRC メッセージ処理 ───────────────── */
    void _processBuffer(Client* client);
    void _dispatch(Client* client, const std::string& line);

    /* ── コマンドハンドラ ─────────────────── */
    void _cmdPass   (Client* client, const std::vector<std::string>& args);
    void _cmdNick   (Client* client, const std::vector<std::string>& args);
    void _cmdUser   (Client* client, const std::vector<std::string>& args);
    void _cmdJoin   (Client* client, const std::vector<std::string>& args);
    void _cmdPart   (Client* client, const std::vector<std::string>& args);
    void _cmdPrivmsg(Client* client, const std::vector<std::string>& args);
    void _cmdNotice (Client* client, const std::vector<std::string>& args);
    void _cmdKick   (Client* client, const std::vector<std::string>& args);
    void _cmdInvite (Client* client, const std::vector<std::string>& args);
    void _cmdTopic  (Client* client, const std::vector<std::string>& args);
    void _cmdMode   (Client* client, const std::vector<std::string>& args);
    void _cmdQuit   (Client* client, const std::vector<std::string>& args);
    void _cmdPing   (Client* client, const std::vector<std::string>& args);
    void _cmdWho    (Client* client, const std::vector<std::string>& args);

    /* ── ヘルパー ─────────────────────────── */
    void        _sendWelcome(Client* client);
    Channel*    _getOrCreateChannel(const std::string& name);
    Client*     _getClientByNick(const std::string& nick);
    void        _partChannel(Client* client, Channel* channel,
                             const std::string& reason);

    /* IRC 数値リプライを送信 */
    void _reply(Client* client, const std::string& msg);
    void _numericReply(Client* client, int num, const std::string& msg);
    /* エラーリプライ */
    void _errorReply(Client* client, int num, const std::string& msg);

    /* ── 静的ユーティリティ ──────────────── */
    static std::vector<std::string> _parseArgs(const std::string& line);
    static bool _validNick(const std::string& nick);
    static bool _validChannel(const std::string& name);
    static std::string _toUpper(const std::string& s);
    static std::string _itoa(int n);

    /* IRC サーバー名 */
    static const std::string SERVNAME;
};
