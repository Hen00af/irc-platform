#include "Channel.hpp"

#include <sstream>

/* 新規 Channel では t を有効にする (設計書 02 §6.2)。
   最初の参加者の Operator 追加は Server の JOIN 処理が行う */
Channel::Channel()
    : _inviteOnly(false),
      _topicRestricted(true),
      _keyEnabled(false),
      _limitEnabled(false),
      _userLimit(0)
{
}

Channel::Channel(const std::string &name, const std::string &keyName)
    : _name(name),
      _keyName(keyName),
      _inviteOnly(false),
      _topicRestricted(true),
      _keyEnabled(false),
      _limitEnabled(false),
      _userLimit(0)
{
}

/* ── 基本 ─────────────────────────────────── */

const std::string &Channel::getName() const
{
    return _name;
}

const std::string &Channel::getKeyName() const
{
    return _keyName;
}

const std::string &Channel::getTopic() const
{
    return _topic;
}

void Channel::setTopic(const std::string &topic)
{
    _topic = topic;
}

bool Channel::hasTopic() const
{
    return !_topic.empty();
}

/* ── Member ───────────────────────────────── */

const std::set<int> &Channel::getMembers() const
{
    return _members;
}

std::size_t Channel::getMemberCount() const
{
    return _members.size();
}

bool Channel::isEmpty() const
{
    return _members.empty();
}

bool Channel::hasMember(int fd) const
{
    return _members.find(fd) != _members.end();
}

bool Channel::addMember(int fd)
{
    return _members.insert(fd).second;
}

bool Channel::removeMember(int fd)
{
    bool removed = _members.erase(fd) > 0;

    /* 不変条件「Operator FD は必ず Member FD でもある」を保つ */
    _operators.erase(fd);
    return removed;
}

/* ── Operator ─────────────────────────────── */

bool Channel::isOperator(int fd) const
{
    return _operators.find(fd) != _operators.end();
}

bool Channel::addOperator(int fd)
{
    if (!hasMember(fd))
        return false;
    return _operators.insert(fd).second;
}

bool Channel::removeOperator(int fd)
{
    return _operators.erase(fd) > 0;
}

/* ── Invite ───────────────────────────────── */

bool Channel::isInvited(int fd) const
{
    return _invited.find(fd) != _invited.end();
}

bool Channel::addInvite(int fd)
{
    return _invited.insert(fd).second;
}

bool Channel::removeInvite(int fd)
{
    return _invited.erase(fd) > 0;
}

/* ── Mode i ───────────────────────────────── */

bool Channel::isInviteOnly() const
{
    return _inviteOnly;
}

void Channel::setInviteOnly(bool enabled)
{
    _inviteOnly = enabled;
}

/* ── Mode t ───────────────────────────────── */

bool Channel::isTopicRestricted() const
{
    return _topicRestricted;
}

void Channel::setTopicRestricted(bool enabled)
{
    _topicRestricted = enabled;
}

/* ── Mode k ───────────────────────────────── */

bool Channel::hasKey() const
{
    return _keyEnabled;
}

const std::string &Channel::getChannelKey() const
{
    return _channelKey;
}

void Channel::setChannelKey(const std::string &key)
{
    _channelKey = key;
    _keyEnabled = true;
}

void Channel::clearChannelKey()
{
    _channelKey.clear();
    _keyEnabled = false;
}

bool Channel::matchesKey(const std::string &key) const
{
    if (!_keyEnabled)
        return true;
    return _channelKey == key;
}

/* ── Mode l ───────────────────────────────── */

bool Channel::hasUserLimit() const
{
    return _limitEnabled;
}

std::size_t Channel::getUserLimit() const
{
    return _userLimit;
}

void Channel::setUserLimit(std::size_t limit)
{
    _userLimit    = limit;
    _limitEnabled = true;
}

void Channel::clearUserLimit()
{
    _userLimit    = 0;
    _limitEnabled = false;
}

bool Channel::isFull() const
{
    if (!_limitEnabled)
        return false;
    return _members.size() >= _userLimit;
}

/* ── Mode 文字列 ──────────────────────────── */

std::string Channel::buildModeString() const
{
    std::string modes = "+";

    if (_inviteOnly)
        modes += 'i';
    if (_topicRestricted)
        modes += 't';
    if (_keyEnabled)
        modes += 'k';
    if (_limitEnabled)
        modes += 'l';
    return modes;
}

std::vector<std::string> Channel::buildModeParameters() const
{
    std::vector<std::string> parameters;

    if (_keyEnabled)
        parameters.push_back(_channelKey);

    if (_limitEnabled)
    {
        std::ostringstream oss;

        oss << _userLimit;
        parameters.push_back(oss.str());
    }
    return parameters;
}
