#include "Client.hpp"

Client::Client(int fd, const std::string& host)
    : fd(fd), hostname(host), passOk(false), nickOk(false), userOk(false)
{
}

bool Client::isRegistered() const
{
    return passOk && nickOk && userOk;
}

std::string Client::prefix() const
{
    return nickname + "!" + username + "@" + hostname;
}

void Client::appendSend(const std::string& msg)
{
    sendBuf += msg;
}
