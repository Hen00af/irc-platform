#include <cstddef>
#include <string>
#include <vector>

#include "../interface/Server.hpp"
#include "../util/IrcUtil.hpp"
#include "../util/Numerics.hpp"
#include "../util/Reply.hpp"

/* ============================================================
 * MODE Command Handler (設計書 05、spec 2026-07-20-mode-design.md)
 *
 * 最後のスタブだった handleMode を実装する。Channel Mode の照会
 * (設計書 05 §4) と変更 (§6〜§16) を扱う。変更は Mode 文字列を
 * 左から右へ解析し、成功した変更だけを 1 つの MODE 通知へ集約する
 * (§8, §16)。
 *
 * i/t/k/o/l のすべてと未知 Mode の 472 を実装する (spec Task 1 + 2)。
 * ============================================================ */

namespace
{
    /* +k の Key 検証 (設計書 05 §11)。IrcUtil を増やさず MODE 固有
       ロジックとして本ファイルに閉じる (spec「k の検証」節)。
       1〜23 文字、かつ空白 (0x20)・NUL・CR・LF・TAB (0x09) を含まない */
    bool isValidChannelKey(const std::string &key)
    {
        if (key.empty() || key.size() > 23)
            return false;

        for (std::size_t i = 0; i < key.size(); ++i)
        {
            char c = key[i];

            if (c == ' ' || c == '\0' || c == '\r' || c == '\n' || c == '\t')
                return false;
        }
        return true;
    }

    /* +l の上限 (設計書 05 §13)。parsePositiveSize() 自体は課さない
       MODE 固有の制約のため、本ファイル内 static 定数として持つ */
    const std::size_t MODE_MAX_USER_LIMIT = 100000;

    /* 成功した Mode 変更を changedModes へ追加する。直前に出力した符号
       (lastSign) と異なる符号のときだけ '+'/'-' を先に足す
       (設計書 05 §8) */
    void appendChangedMode(bool adding, char mode, char &lastSign,
                           std::string &changedModes)
    {
        char sign = adding ? '+' : '-';

        if (lastSign != sign)
        {
            changedModes += sign;
            lastSign = sign;
        }
        changedModes += mode;
    }
}

void Server::handleMode(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (!requireParams(fd, message, 1))
        return;

    const std::string &target = message.params[0];

    /* target が '#' で始まらない場合は User Mode 形式であり Mandatory
       対象外 (設計書 05 §2)。421 を返す */
    if (target.empty() || target[0] != '#')
    {
        queueToClient(fd, Reply::numeric(
            _serverName, *client, Numeric::ERR_UNKNOWNCOMMAND,
            target + " :Unknown command"));
        return;
    }

    Channel *channel = findChannel(target);

    if (channel == NULL)
    {
        sendNoSuchChannel(fd, *client, target);
        return;
    }

    /* ── Mode 照会 (設計書 05 §4) ──────────────────
       Channel Member でなくても照会可能。Key も Operator 以外へ返す */
    if (message.params.size() == 1)
    {
        std::string               modes  = channel->buildModeString();
        std::vector<std::string>  params = channel->buildModeParameters();
        std::string                reply = channel->getName() + " " + modes;

        for (std::size_t i = 0; i < params.size(); ++i)
            reply += " " + params[i];

        queueToClient(fd, Reply::numeric(
            _serverName, *client, Numeric::RPL_CHANNELMODEIS, reply));
        return;
    }

    /* ── Mode 変更 — 権限検証 (設計書 05 §15) ──────
       この順序で検証し、失敗時は Mode 文字列を一切解析せず、
       状態も変更しない */
    if (!requireChannelMember(fd, *channel))
        return;
    if (!requireChannelOperator(fd, *channel))
        return;

    /* ── Mode 文字列の解析 (設計書 05 §6〜§8) ──────
       argIndex は Mode Parameter (params[2] 以降) の参照位置。i/t は
       引数を消費しないが、k/o/l は各 case 内でここを進める */
    const std::string        &modeString = message.params[1];
    bool                       adding     = true;
    bool                       signSeen   = false;
    std::size_t                argIndex   = 2;
    std::string                changedModes;
    std::vector<std::string>   changedParams;
    char                       lastSign   = 0;

    for (std::size_t i = 0; i < modeString.size(); ++i)
    {
        char c = modeString[i];

        if (c == '+')
        {
            adding   = true;
            signSeen = true;
            continue;
        }
        if (c == '-')
        {
            adding   = false;
            signSeen = true;
            continue;
        }
        /* 符号がまだ現れていない文字は 472。引数は消費せず継続する
           (設計書 05 §6) */
        if (!signSeen)
        {
            queueToClient(fd, Reply::numeric(
                _serverName, *client, Numeric::ERR_UNKNOWNMODE,
                std::string(1, c) + " :is unknown mode char to me for "
                    + channel->getName()));
            continue;
        }

        switch (c)
        {
            case 'i':
                /* 設計書 05 §9。引数消費なし */
                if (channel->isInviteOnly() != adding)
                {
                    channel->setInviteOnly(adding);
                    appendChangedMode(adding, 'i', lastSign, changedModes);
                }
                break;
            case 't':
                /* 設計書 05 §10。引数消費なし */
                if (channel->isTopicRestricted() != adding)
                {
                    channel->setTopicRestricted(adding);
                    appendChangedMode(adding, 't', lastSign, changedModes);
                }
                break;
            case 'k':
                /* 設計書 05 §11。+k は引数必須、-k は引数不要 */
                if (adding)
                {
                    if (argIndex >= message.params.size())
                    {
                        queueToClient(fd, Reply::numeric(
                            _serverName, *client, Numeric::ERR_NEEDMOREPARAMS,
                            message.command + " :Not enough parameters"));
                        break;
                    }

                    const std::string &key = message.params[argIndex];

                    /* 不正な key は 461 とし、467 は使わない
                       (spec の意図的な差分)。引数は消費しない */
                    if (!isValidChannelKey(key))
                    {
                        queueToClient(fd, Reply::numeric(
                            _serverName, *client, Numeric::ERR_NEEDMOREPARAMS,
                            message.command + " :Not enough parameters"));
                        break;
                    }

                    /* 既存 key の置換も常に実変更扱い (467 不使用) */
                    channel->setChannelKey(key);
                    appendChangedMode(adding, 'k', lastSign, changedModes);
                    changedParams.push_back(key);
                    ++argIndex;
                }
                else if (channel->hasKey())
                {
                    channel->clearChannelKey();
                    appendChangedMode(adding, 'k', lastSign, changedModes);
                }
                break;
            case 'o':
            {
                /* 設計書 05 §12。+o/-o とも引数必須 */
                if (argIndex >= message.params.size())
                {
                    queueToClient(fd, Reply::numeric(
                        _serverName, *client, Numeric::ERR_NEEDMOREPARAMS,
                        message.command + " :Not enough parameters"));
                    break;
                }

                const std::string &nickArg = message.params[argIndex];

                /* 引数が存在すれば、401/441 の場合でも必ず消費する */
                ++argIndex;

                Client *target = findClientByNickname(nickArg);

                if (target == NULL)
                {
                    queueToClient(fd, Reply::numeric(
                        _serverName, *client, Numeric::ERR_NOSUCHNICK,
                        nickArg + " :No such nick/channel"));
                    break;
                }
                if (!channel->hasMember(target->getFd()))
                {
                    queueToClient(fd, Reply::numeric(
                        _serverName, *client, Numeric::ERR_USERNOTINCHANNEL,
                        nickArg + " " + channel->getName()
                            + " :They aren't on that channel"));
                    break;
                }

                /* 既に期待状態なら実変更なし。自動移譲は行わない */
                bool changed = adding ? channel->addOperator(target->getFd())
                                      : channel->removeOperator(
                                            target->getFd());

                if (changed)
                {
                    appendChangedMode(adding, 'o', lastSign, changedModes);
                    changedParams.push_back(target->getNickname());
                }
                break;
            }
            case 'l':
                /* 設計書 05 §13。+l は引数必須、-l は引数不要 */
                if (adding)
                {
                    if (argIndex >= message.params.size())
                    {
                        queueToClient(fd, Reply::numeric(
                            _serverName, *client, Numeric::ERR_NEEDMOREPARAMS,
                            message.command + " :Not enough parameters"));
                        break;
                    }

                    const std::string &arg = message.params[argIndex];

                    /* 不正 limit は Numeric を送らず黙って無視するが、
                       引数は消費する (spec の意図的な差分) */
                    ++argIndex;

                    std::size_t limit = 0;

                    if (!IrcUtil::parsePositiveSize(arg, limit)
                        || limit > MODE_MAX_USER_LIMIT)
                        break;

                    channel->setUserLimit(limit);
                    appendChangedMode(adding, 'l', lastSign, changedModes);
                    changedParams.push_back(arg);
                }
                else if (channel->hasUserLimit())
                {
                    channel->clearUserLimit();
                    appendChangedMode(adding, 'l', lastSign, changedModes);
                }
                break;
            default:
                /* Mandatory 外の未知 Mode (設計書 05 §14)。引数は消費
                   しない */
                queueToClient(fd, Reply::numeric(
                    _serverName, *client, Numeric::ERR_UNKNOWNMODE,
                    std::string(1, c) + " :is unknown mode char to me for "
                        + channel->getName()));
                break;
        }
    }

    /* ── 成功変更の集約通知 (設計書 05 §8, §16) ──────
       実変更 0 件なら MODE 通知を送らない。1 件以上なら変更後の全
       Member (実行者含む) へ、実行者 Client Prefix で通知する */
    if (changedModes.empty())
        return;

    std::string notifyParams = changedModes;

    for (std::size_t i = 0; i < changedParams.size(); ++i)
        notifyParams += " " + changedParams[i];

    broadcastToChannel(*channel, Reply::command(
        Reply::clientPrefix(*client), "MODE",
        channel->getName() + " " + notifyParams), -1);
}
