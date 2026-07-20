#include <cstddef>
#include <string>
#include <vector>

#include "../interface/Server.hpp"
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
 * i/t/k/o/l のうち、本ファイルでは i/t を実装する (spec Task 1)。
 * k/o/l と未知 Mode は Task 2 で追加するため、この段では該当文字へ
 * 到達したら「何もせず continue」で仮置きする。ただし符号なし
 * (signSeen == false) 時の 472 は解析ループの骨格として Task 1 で
 * 実装する。
 * ============================================================ */

namespace
{
    /* 成功した Mode 変更を changedModes へ追加する。直前に出力した符号
       (lastSign) と異なる符号のときだけ '+'/'-' を先に足す
       (設計書 05 §8)。Task 2 が k/o/l を追加する際も同じ規則で
       呼び出せるよう、ファイル内 static Helper として独立させる */
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
       引数を消費しないため本ファイルでは未使用のまま置くが、Task 2 が
       k/o/l の引数消費でそのまま使う集約状態としてここに用意する */
    const std::string        &modeString = message.params[1];
    bool                       adding     = true;
    bool                       signSeen   = false;
    std::size_t                argIndex   = 2;
    std::string                changedModes;
    std::vector<std::string>   changedParams;
    char                       lastSign   = 0;

    (void)argIndex;

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
            default:
                /* k/o/l と未知 Mode は Task 2 で実装する。この段では
                   何もせず次の文字へ進む (spec Task 1 の仮置き) */
                continue;
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
