#pragma once

#include <cstddef>
#include <set>
#include <string>

/* ============================================================
 * Client
 *
 * 1 接続分の FD、IRC 登録情報、送受信バッファを保持する。
 *
 * 登録フロー: PASS / NICK / USER の受信順序は固定しない。3 つが
 * 揃った時点で tryCompleteRegistration() が 1 度だけ true を返す。
 *
 * 所有権と関係:
 *   - FD を所有しないため、デストラクタで close() しない
 *   - Channel への参照は正規化済み Channel 名の集合だけで表す
 *     (Channel* を保持しない)
 *   - Channel との関係変更は最終的に Server が統括する。本クラスは
 *     自身の集合を書き換える小さな API だけを提供する
 *
 * 本クラスは recv() / send() も行解析も行わない (設計書 02 §5.6)。
 * バッファへの追記と先頭削除だけを提供する。CRLF の検索と完成行の
 * 抽出は BufferUtil::findLine() が担当する。
 *
 * ネットワーク担当の利用例:
 *
 *   // 受信。LINE_TOO_LONG を握り潰すと超過行がバッファ先頭に残り、
 *   // 本来の ERROR :Input line too long ではなく、64KiB 到達後の
 *   // ERROR :Receive buffer limit exceeded へ落ちるため必ず分岐する
 *   ssize_t n = recv(fd, buf, sizeof(buf), 0);
 *   client.appendReceiveBuffer(buf, n);
 *
 *   bool done = false;
 *   while (!done)
 *   {
 *       std::string line;
 *       std::size_t consumed = 0;
 *
 *       switch (BufferUtil::findLine(client.getReceiveBuffer(), line,
 *                                    consumed))
 *       {
 *       case BufferUtil::LINE_EXTRACTED:
 *           client.eraseReceivePrefix(consumed);
 *           {
 *               Message message;
 *               if (Parser::parse(line, message))
 *                   dispatchCommand(fd, message);
 *           }
 *           break;
 *       case BufferUtil::LINE_TOO_LONG:
 *           queueToClient(fd, "ERROR :Input line too long");
 *           scheduleDisconnect(fd);
 *           done = true;
 *           break;
 *       case BufferUtil::LINE_INCOMPLETE:
 *           done = true;
 *           break;
 *       }
 *   }
 *
 *   // 送信
 *   client.appendSendBuffer(reply);
 *   const std::string &out = client.getSendBuffer();
 *   ssize_t sent = send(fd, out.data(), out.size(), 0);
 *   if (sent > 0)
 *       client.eraseSendPrefix(static_cast<std::size_t>(sent));
 *   // client.hasPendingOutput() が false なら POLLOUT を外す
 * ============================================================ */
class Client
{
public:
    /* 設計書 02 §5.3 により原則使用しない。
       Server は insert(std::make_pair(fd, Client(fd, host))) を使うこと。
       これはコピー Constructor を呼ぶので Default は不要である。
       Default が必要になるのは std::map::operator[] だけであり、
       _clients[fd] は _fd == -1 の Client を作って不変条件
       「_clients の Key と Client::_fd が一致する」を破る */
    Client();
    Client(int fd, const std::string &hostname);

    /* ── 識別情報 ─────────────────────────── */
    int                getFd() const;
    const std::string &getNickname() const;
    const std::string &getUsername() const;
    const std::string &getRealname() const;
    const std::string &getHostname() const;

    void setNickname(const std::string &nickname);
    /* _username / _realname / USER 受信済みフラグを同時に更新する */
    void setUser(const std::string &username, const std::string &realname);

    /* ── 登録状態 ─────────────────────────── */
    bool isPasswordAccepted() const;
    bool hasNickname() const;
    bool hasUser() const;
    bool isRegistered() const;

    void acceptPassword();

    /* PASS / NICK / USER が揃っていれば登録済みにする。
       戻り値 true はこの呼び出しで初めて登録完了になったことを表す。
       呼び出し側は true のときだけ Welcome Reply を送る */
    bool tryCompleteRegistration();

    /* ── 受信バッファ ─────────────────────── */

    /* 完成行の判定と取り出しには BufferUtil::hasCompleteLine() /
       BufferUtil::findLine() へこの参照を渡す */
    const std::string &getReceiveBuffer() const;

    /* NUL を含むデータも扱えるよう、長さを明示して受け取る。
       data が NULL または length が 0 なら何もしない */
    void appendReceiveBuffer(const char *data, std::size_t length);
    /* 処理済みの先頭 length byte を取り除く。
       length がバッファ長以上なら全体を空にする */
    void eraseReceivePrefix(std::size_t length);

    /* ── 送信バッファ ─────────────────────── */

    /* 未送信データの参照。send() へそのまま渡す */
    const std::string &getSendBuffer() const;
    /* 送信待ちがあるか。POLLOUT の監視要否の判定に使う */
    bool               hasPendingOutput() const;

    /* 送信 IRC メッセージを末尾へ追加する。queue された順序を保つ */
    void appendSendBuffer(const std::string &data);
    /* send() が返した送信済みバイト数を先頭から取り除く。
       部分送信の残りはバッファに残る */
    void eraseSendPrefix(std::size_t length);
    void clearBuffers();

    /* ── Channel 関係 ─────────────────────── */

    /* channelKey には IrcUtil::normalizeChannelName() 済みの名前を渡す */
    const std::set<std::string> &getJoinedChannels() const;
    bool hasJoinedChannel(const std::string &channelKey) const;
    /* 戻り値は集合が実際に変更されたかを表す */
    bool addJoinedChannel(const std::string &channelKey);
    bool removeJoinedChannel(const std::string &channelKey);

private:
    int                   _fd;
    std::string           _nickname;
    std::string           _username;
    std::string           _realname;
    std::string           _hostname;
    std::string           _receiveBuffer;
    std::string           _sendBuffer;
    bool                  _passwordAccepted;
    bool                  _userReceived;
    bool                  _registered;
    std::set<std::string> _joinedChannels;
};
