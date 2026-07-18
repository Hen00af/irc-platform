#include "Client.hpp"

Client::Client()
    : _fd(-1),
      _passwordAccepted(false),
      _userReceived(false),
      _registered(false)
{
}

Client::Client(int fd, const std::string &hostname)
    : _fd(fd),
      _hostname(hostname),
      _passwordAccepted(false),
      _userReceived(false),
      _registered(false)
{
}

/* ── 識別情報 ─────────────────────────────── */

int Client::getFd() const
{
    return _fd;
}

const std::string &Client::getNickname() const
{
    return _nickname;
}

const std::string &Client::getUsername() const
{
    return _username;
}

const std::string &Client::getRealname() const
{
    return _realname;
}

const std::string &Client::getHostname() const
{
    return _hostname;
}

void Client::setNickname(const std::string &nickname)
{
    _nickname = nickname;
}

void Client::setUser(const std::string &username, const std::string &realname)
{
    _username     = username;
    _realname     = realname;
    _userReceived = true;
}

/* ── 登録状態 ─────────────────────────────── */

bool Client::isPasswordAccepted() const
{
    return _passwordAccepted;
}

bool Client::hasNickname() const
{
    return !_nickname.empty();
}

bool Client::hasUser() const
{
    return _userReceived;
}

bool Client::isRegistered() const
{
    return _registered;
}

void Client::acceptPassword()
{
    _passwordAccepted = true;
}

bool Client::tryCompleteRegistration()
{
    if (_registered)
        return false;

    if (_passwordAccepted && hasNickname() && _userReceived)
    {
        _registered = true;
        return true;
    }
    return false;
}

/* ── 受信バッファ ─────────────────────────── */

const std::string &Client::getReceiveBuffer() const
{
    return _receiveBuffer;
}

void Client::appendReceiveBuffer(const char *data, std::size_t length)
{
    if (data == 0 || length == 0)
        return;
    _receiveBuffer.append(data, length);
}

void Client::eraseReceivePrefix(std::size_t length)
{
    if (length >= _receiveBuffer.size())
        _receiveBuffer.clear();
    else
        _receiveBuffer.erase(0, length);
}

/* ── 送信バッファ ─────────────────────────── */

const std::string &Client::getSendBuffer() const
{
    return _sendBuffer;
}

bool Client::hasPendingOutput() const
{
    return !_sendBuffer.empty();
}

void Client::appendSendBuffer(const std::string &data)
{
    _sendBuffer += data;
}

void Client::eraseSendPrefix(std::size_t length)
{
    if (length >= _sendBuffer.size())
        _sendBuffer.clear();
    else
        _sendBuffer.erase(0, length);
}

void Client::clearBuffers()
{
    _receiveBuffer.clear();
    _sendBuffer.clear();
}

/* ── Channel 関係 ─────────────────────────── */

const std::set<std::string> &Client::getJoinedChannels() const
{
    return _joinedChannels;
}

bool Client::hasJoinedChannel(const std::string &channelKey) const
{
    return _joinedChannels.find(channelKey) != _joinedChannels.end();
}

bool Client::addJoinedChannel(const std::string &channelKey)
{
    return _joinedChannels.insert(channelKey).second;
}

bool Client::removeJoinedChannel(const std::string &channelKey)
{
    return _joinedChannels.erase(channelKey) > 0;
}
