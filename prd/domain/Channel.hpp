#pragma once

#include <string>
#include <map>
#include <set>

class Client;

/* ============================================================
 * Channel
 *
 * IRC チャンネルの状態を管理する。
 *
 * 対応モード:
 *   i  - invite-only (招待制)
 *   t  - topic (オペレータのみ topic 変更可)
 *   k  - key (チャンネルパスワード)
 *   o  - operator (オペレータ権限)
 *   l  - limit (最大人数)
 * ============================================================ */
class Channel
{
public:
    std::string name;
    std::string topic;
    std::string key;        /* モード k のパスワード */
    int         userLimit;  /* モード l の上限 (0 = 制限なし) */
    bool        inviteOnly; /* モード i */
    bool        topicOp;    /* モード t */

    /* fd → Client* のメンバー一覧 */
    std::map<int, Client*> members;
    /* オペレータの fd 集合 */
    std::set<int>          operators;
    /* 招待済みの fd 集合 */
    std::set<int>          invited;

    explicit Channel(const std::string& name);

    bool hasMember(int fd) const;
    bool isOperator(int fd) const;
    bool isInvited(int fd) const;

    void addMember(Client* client);
    void removeMember(int fd);
    void addOperator(int fd);
    void removeOperator(int fd);

    /* MODE 文字列 (+itkl など) を返す */
    std::string getModeStr() const;
    /* NAMES 用のメンバー一覧を返す (@nick nick ...) */
    std::string getMemberList() const;

    /* チャンネル全員 (exceptFd を除く) に送信バッファへ追記する */
    void broadcast(const std::string& msg, int exceptFd = -1);
};
