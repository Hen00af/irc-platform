#include "Channel.hpp"
#include "Client.hpp"
#include <sstream>

Channel::Channel(const std::string& name)
    : name(name), userLimit(0), inviteOnly(false), topicOp(false)
{
}

bool Channel::hasMember(int fd) const
{
    return members.count(fd) > 0;
}

bool Channel::isOperator(int fd) const
{
    return operators.count(fd) > 0;
}

bool Channel::isInvited(int fd) const
{
    return invited.count(fd) > 0;
}

void Channel::addMember(Client* client)
{
    members[client->fd] = client;
}

void Channel::removeMember(int fd)
{
    members.erase(fd);
    operators.erase(fd);
    invited.erase(fd);
}

void Channel::addOperator(int fd)
{
    operators.insert(fd);
}

void Channel::removeOperator(int fd)
{
    operators.erase(fd);
}

/* MODE 文字列: +[i][t][k][l] [key] [limit] */
std::string Channel::getModeStr() const
{
    std::string modes = "+";
    std::string params;

    if (inviteOnly)
        modes += 'i';
    if (topicOp)
        modes += 't';
    if (!key.empty())
    {
        modes += 'k';
        params += " " + key;
    }
    if (userLimit > 0)
    {
        modes += 'l';
        std::ostringstream oss;
        oss << userLimit;
        params += " " + oss.str();
    }
    return modes + params;
}

/* NAMES 用: "@nick" (oper) または "nick" の空白区切りリスト */
std::string Channel::getMemberList() const
{
    std::string list;
    for (std::map<int, Client*>::const_iterator it = members.begin();
         it != members.end(); ++it)
    {
        if (!list.empty())
            list += " ";
        if (operators.count(it->first))
            list += "@";
        list += it->second->nickname;
    }
    return list;
}

/* チャンネル全員 (exceptFd を除く) の送信バッファに追記 */
void Channel::broadcast(const std::string& msg, int exceptFd)
{
    for (std::map<int, Client*>::iterator it = members.begin();
         it != members.end(); ++it)
    {
        if (it->first != exceptFd)
            it->second->appendSend(msg);
    }
}
