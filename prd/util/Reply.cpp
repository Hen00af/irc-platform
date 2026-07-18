#include "Reply.hpp"

#include "../domain/Client.hpp"
#include "IrcUtil.hpp"

#include <iomanip>
#include <sstream>

std::string Reply::clientPrefix(const Client &client)
{
    std::string nickname = client.getNickname();
    std::string username = client.getUsername();
    std::string hostname = client.getHostname();

    if (nickname.empty())
        nickname = "*";
    if (username.empty())
        username = "unknown";
    if (hostname.empty())
        hostname = "0.0.0.0";

    return nickname + "!" + username + "@" + hostname;
}

std::string Reply::serverPrefix(const std::string &serverName)
{
    return serverName;
}

std::string Reply::numeric(const std::string &serverName,
                           const Client      &target,
                           int                code,
                           const std::string &parameters)
{
    if (code < 0 || code > 999)
        return std::string();

    if (!IrcUtil::isSafeToken(serverName))
        return std::string();

    std::string targetName = target.getNickname();

    if (targetName.empty())
        targetName = "*";
    if (!IrcUtil::isSafeToken(targetName))
        return std::string();

    std::ostringstream oss;

    oss << ":" << serverName << " " << std::setw(3) << std::setfill('0')
        << code << " " << targetName;

    std::string safeParameters = IrcUtil::sanitizeMessageText(parameters);

    if (!safeParameters.empty())
        oss << " " << safeParameters;
    return oss.str();
}

std::string Reply::command(const std::string &prefix,
                           const std::string &command,
                           const std::string &parameters)
{
    if (!IrcUtil::isSafeToken(command))
        return std::string();

    std::string result;

    if (!prefix.empty())
    {
        if (!IrcUtil::isSafeToken(prefix))
            return std::string();
        result = ":" + prefix + " ";
    }
    result += command;

    std::string safeParameters = IrcUtil::sanitizeMessageText(parameters);

    if (!safeParameters.empty())
        result += " " + safeParameters;
    return result;
}
