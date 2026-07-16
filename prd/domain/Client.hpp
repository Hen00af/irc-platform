#pragma once

#include <string>

/* ============================================================
 * Client
 *
 * IRC 接続クライアントの状態を管理する。
 * 登録フロー: PASS → NICK → USER の順で受信し、
 * 3つが揃ったら isRegistered() == true になる。
 * ============================================================ */
class Client
{
public:
    int         fd;
    std::string nickname;
    std::string username;
    std::string realname;
    std::string hostname;

    /* 受信バッファ: \r\n が来るまでデータを蓄積する */
    std::string recvBuf;
    /* 送信バッファ: send() が EAGAIN の場合に残りを保持する */
    std::string sendBuf;

    bool passOk; /* PASS コマンドで正しいパスワードが送られた */
    bool nickOk; /* NICK コマンドで有効なニックネームが設定された */
    bool userOk; /* USER コマンドが受信された */

    Client(int fd, const std::string& host);

    /* PASS + NICK + USER が揃っているか */
    bool isRegistered() const;

    /* IRC メッセージの prefix 部分 nick!user@host */
    std::string prefix() const;

    /* 送信バッファにメッセージを追記する */
    void appendSend(const std::string& msg);
};
