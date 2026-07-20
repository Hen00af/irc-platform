#include <cassert>
#include <iostream>

#include "Client.hpp"
#include "Channel.hpp"

/* ============================================================
 * Client のテスト
 * ============================================================ */

static void test_client_registration()
{
    Client c(4, "localhost");

    assert(!c.isRegistered());
    c.passOk = true;
    assert(!c.isRegistered());
    c.nickOk = true;
    assert(!c.isRegistered());
    c.userOk = true;
    assert(c.isRegistered());
}

static void test_client_prefix()
{
    Client c(4, "localhost");

    c.nickname = "alice";
    c.username = "auser";
    assert(c.prefix() == "alice!auser@localhost");
}

static void test_client_append_send()
{
    Client c(4, "localhost");

    c.appendSend("PING\r\n");
    c.appendSend("PONG\r\n");
    assert(c.sendBuf == "PING\r\nPONG\r\n");
}

/* ============================================================
 * Channel のテスト
 * ============================================================ */

static void test_channel_defaults()
{
    Channel ch("#test");

    assert(ch.name == "#test");
    assert(ch.userLimit == 0);
    assert(!ch.inviteOnly);
    assert(!ch.topicOp);
    assert(ch.getModeStr() == "+");
}

static void test_channel_membership()
{
    Channel ch("#test");
    Client  alice(4, "host");
    Client  bob(5, "host");

    ch.addMember(&alice);
    ch.addMember(&bob);
    assert(ch.hasMember(4));
    assert(ch.hasMember(5));

    ch.addOperator(4);
    assert(ch.isOperator(4));
    assert(!ch.isOperator(5));

    ch.removeMember(4);
    assert(!ch.hasMember(4));
    /* removeMember はオペレータ/招待状態も掃除する */
    assert(!ch.isOperator(4));
}

static void test_channel_invite()
{
    Channel ch("#test");

    assert(!ch.isInvited(7));
    ch.invited.insert(7);
    assert(ch.isInvited(7));
    ch.removeMember(7);
    assert(!ch.isInvited(7));
}

static void test_channel_mode_str()
{
    Channel ch("#test");

    ch.inviteOnly = true;
    ch.topicOp = true;
    assert(ch.getModeStr() == "+it");

    ch.key = "secret";
    ch.userLimit = 10;
    assert(ch.getModeStr() == "+itkl secret 10");
}

static void test_channel_member_list()
{
    Channel ch("#test");
    Client  alice(4, "host");
    Client  bob(5, "host");

    alice.nickname = "alice";
    bob.nickname = "bob";
    ch.addMember(&alice);
    ch.addMember(&bob);
    ch.addOperator(4);
    assert(ch.getMemberList() == "@alice bob");
}

static void test_channel_broadcast()
{
    Channel ch("#test");
    Client  alice(4, "host");
    Client  bob(5, "host");

    ch.addMember(&alice);
    ch.addMember(&bob);
    ch.broadcast("hello\r\n", 4);
    assert(alice.sendBuf.empty());
    assert(bob.sendBuf == "hello\r\n");

    ch.broadcast("all\r\n");
    assert(alice.sendBuf == "all\r\n");
    assert(bob.sendBuf == "hello\r\nall\r\n");
}

int main()
{
    test_client_registration();
    test_client_prefix();
    test_client_append_send();

    test_channel_defaults();
    test_channel_membership();
    test_channel_invite();
    test_channel_mode_str();
    test_channel_member_list();
    test_channel_broadcast();

    std::cout << "All unit tests passed." << std::endl;
    return 0;
}
