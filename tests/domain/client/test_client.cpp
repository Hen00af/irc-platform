#include "TestRunner.hpp"

#include "prd/domain/Client.hpp"
#include "prd/domain/Message.hpp"
#include "prd/util/BufferUtil.hpp"
#include "prd/util/IrcUtil.hpp"
#include "prd/util/Parser.hpp"

#include <cstring>
#include <map>

void runClientTests()
{
    TestRunner::beginSuite("Client");

    /* ── Constructor ──────────────────────── */

    {
        Client client(3, "127.0.0.1");

        ASSERT_EQ("Client: getFd", client.getFd(), 3);
        ASSERT_EQ("Client: getHostname", client.getHostname(), "127.0.0.1");
        ASSERT_EQ("Client: 接続直後の Nickname は空",
                  client.getNickname(), "");
        ASSERT_EQ("Client: 接続直後の Username は空",
                  client.getUsername(), "");
        ASSERT_EQ("Client: 接続直後の Realname は空",
                  client.getRealname(), "");
        ASSERT_EQ("Client: 接続直後の受信バッファは空",
                  client.getReceiveBuffer(), "");
        ASSERT_EQ("Client: 接続直後の送信バッファは空",
                  client.getSendBuffer(), "");
        ASSERT_FALSE("Client: 接続直後は PASS 未承認",
                     client.isPasswordAccepted());
        ASSERT_FALSE("Client: 接続直後は Nickname 未設定",
                     client.hasNickname());
        ASSERT_FALSE("Client: 接続直後は USER 未受信", client.hasUser());
        ASSERT_FALSE("Client: 接続直後は未登録", client.isRegistered());
        ASSERT_FALSE("Client: 接続直後は送信待ちなし",
                     client.hasPendingOutput());
        ASSERT_EQ("Client: 接続直後の参加 Channel は 0",
                  client.getJoinedChannels().size(),
                  static_cast<std::size_t>(0));
    }

    {
        /* Default Constructor は原則使用しない。Server は
           insert(std::make_pair(...)) を使う。生成された場合は FD が -1 */
        Client client;

        ASSERT_EQ("Client: Default Constructor の FD は -1",
                  client.getFd(), -1);
        ASSERT_FALSE("Client: Default Constructor は未登録",
                     client.isRegistered());
    }

    /* ── 識別情報 ─────────────────────────── */

    {
        Client client(4, "10.0.0.1");

        client.setNickname("alice");
        ASSERT_EQ("Client: setNickname", client.getNickname(), "alice");
        ASSERT_TRUE("Client: setNickname で hasNickname", client.hasNickname());

        /* Nickname は変更できる。索引の更新は Server の責務 */
        client.setNickname("bob");
        ASSERT_EQ("Client: setNickname で上書き", client.getNickname(), "bob");

        /* 表示名はそのまま保持する。正規化は検索 Key 側の責務 */
        client.setNickname("Alice[1]");
        ASSERT_EQ("Client: Nickname の表示名は正規化しない",
                  client.getNickname(), "Alice[1]");

        ASSERT_FALSE("Client: setNickname だけでは USER 未受信",
                     client.hasUser());

        client.setUser("aliceuser", "Alice Example");
        ASSERT_EQ("Client: setUser の Username",
                  client.getUsername(), "aliceuser");
        ASSERT_EQ("Client: setUser の Realname",
                  client.getRealname(), "Alice Example");
        ASSERT_TRUE("Client: setUser で hasUser", client.hasUser());

        /* Realname は空でも許可する */
        Client other(5, "10.0.0.2");
        other.setUser("u", "");
        ASSERT_TRUE("Client: Realname が空でも hasUser", other.hasUser());
        ASSERT_EQ("Client: Realname が空", other.getRealname(), "");
    }

    /* ── 登録状態 ─────────────────────────── */

    {
        Client client(6, "127.0.0.1");

        ASSERT_FALSE("Client: 何もなければ登録完了しない",
                     client.tryCompleteRegistration());

        client.acceptPassword();
        ASSERT_TRUE("Client: acceptPassword", client.isPasswordAccepted());
        ASSERT_FALSE("Client: PASS だけでは登録完了しない",
                     client.tryCompleteRegistration());

        client.setNickname("alice");
        ASSERT_FALSE("Client: PASS + NICK では登録完了しない",
                     client.tryCompleteRegistration());
        ASSERT_FALSE("Client: 登録完了前は isRegistered が false",
                     client.isRegistered());

        client.setUser("alice", "Alice");
        ASSERT_TRUE("Client: PASS + NICK + USER で登録完了",
                    client.tryCompleteRegistration());
        ASSERT_TRUE("Client: 登録完了後は isRegistered", client.isRegistered());

        /* Welcome Reply を 2 回送らないため、2 回目以降は false */
        ASSERT_FALSE("Client: 2 回目の tryCompleteRegistration は false",
                     client.tryCompleteRegistration());
        ASSERT_TRUE("Client: 2 回目以降も isRegistered は true",
                    client.isRegistered());
    }

    {
        /* PASS / NICK / USER の順序は固定しない */
        Client client(7, "127.0.0.1");

        client.setUser("alice", "Alice");
        client.setNickname("alice");
        ASSERT_FALSE("Client: PASS がなければ登録完了しない",
                     client.tryCompleteRegistration());
        client.acceptPassword();
        ASSERT_TRUE("Client: USER → NICK → PASS の順でも登録完了",
                    client.tryCompleteRegistration());
    }

    {
        /* 登録完了は PASS 成功後だけ (設計書 04 §5) */
        Client client(8, "127.0.0.1");

        client.setNickname("alice");
        client.setUser("alice", "Alice");
        ASSERT_FALSE("Client: NICK + USER だけでは登録完了しない",
                     client.tryCompleteRegistration());
        ASSERT_FALSE("Client: NICK + USER だけでは未登録",
                     client.isRegistered());
    }

    /* ── 受信バッファ ─────────────────────── */

    {
        Client client(9, "127.0.0.1");

        client.appendReceiveBuffer("PASS x\r\n", 8);
        ASSERT_EQ("Client: appendReceiveBuffer",
                  client.getReceiveBuffer(), "PASS x\r\n");

        /* 追記は末尾へ連結される (分割受信の復元) */
        client.appendReceiveBuffer("NICK a\r\n", 8);
        ASSERT_EQ("Client: appendReceiveBuffer は末尾へ追記",
                  client.getReceiveBuffer(), "PASS x\r\nNICK a\r\n");

        client.eraseReceivePrefix(8);
        ASSERT_EQ("Client: eraseReceivePrefix で先頭を削除",
                  client.getReceiveBuffer(), "NICK a\r\n");

        /* バッファ長以上を指定しても安全に空になる */
        client.eraseReceivePrefix(1000);
        ASSERT_EQ("Client: eraseReceivePrefix の過大な length は全削除",
                  client.getReceiveBuffer(), "");
        client.eraseReceivePrefix(1);
        ASSERT_EQ("Client: 空バッファへの eraseReceivePrefix も安全",
                  client.getReceiveBuffer(), "");
    }

    {
        Client client(10, "127.0.0.1");

        /* length を明示するため NUL を含むデータも扱える */
        client.appendReceiveBuffer("A\0B", 3);
        ASSERT_EQ("Client: NUL を含む受信データの長さ",
                  client.getReceiveBuffer().size(),
                  static_cast<std::size_t>(3));
        ASSERT_EQ("Client: NUL を含む受信データの内容",
                  client.getReceiveBuffer(), std::string("A\0B", 3));

        /* length より長い文字列を渡しても length で切られる */
        client.eraseReceivePrefix(3);
        client.appendReceiveBuffer("ABCDEF", 3);
        ASSERT_EQ("Client: length で切り取る",
                  client.getReceiveBuffer(), "ABC");

        /* 異常入力への防御 */
        client.eraseReceivePrefix(3);
        client.appendReceiveBuffer("ABC", 0);
        ASSERT_EQ("Client: length 0 は何もしない",
                  client.getReceiveBuffer(), "");
        client.appendReceiveBuffer(static_cast<const char *>(0), 5);
        ASSERT_EQ("Client: NULL データは何もしない",
                  client.getReceiveBuffer(), "");

        client.appendReceiveBuffer("ABC", 3);
        client.eraseReceivePrefix(0);
        ASSERT_EQ("Client: eraseReceivePrefix(0) は何もしない",
                  client.getReceiveBuffer(), "ABC");
    }

    /* ── 送信バッファ ─────────────────────── */

    {
        Client client(11, "127.0.0.1");

        ASSERT_FALSE("Client: 初期状態は送信待ちなし",
                     client.hasPendingOutput());

        client.appendSendBuffer(":server 001 alice :Welcome\r\n");
        ASSERT_TRUE("Client: appendSendBuffer で送信待ちあり",
                    client.hasPendingOutput());
        ASSERT_EQ("Client: getSendBuffer",
                  client.getSendBuffer(), ":server 001 alice :Welcome\r\n");

        /* queue された順序を保つ */
        client.appendSendBuffer(":server 002 alice :Your host\r\n");
        ASSERT_EQ("Client: appendSendBuffer は順序を保つ",
                  client.getSendBuffer(),
                  ":server 001 alice :Welcome\r\n"
                  ":server 002 alice :Your host\r\n");

        /* 部分送信: 送信できたバイト数だけ消費する。
           12 byte = ":server 001 " */
        client.eraseSendPrefix(12);
        ASSERT_EQ("Client: eraseSendPrefix で送信済み分を削除",
                  client.getSendBuffer(),
                  "alice :Welcome\r\n:server 002 alice :Your host\r\n");
        ASSERT_TRUE("Client: 残りがあれば送信待ちのまま",
                    client.hasPendingOutput());

        /* 全部送信し切ると送信待ちが解消する (POLLOUT を外す条件) */
        client.eraseSendPrefix(client.getSendBuffer().size());
        ASSERT_EQ("Client: 全送信後の送信バッファは空",
                  client.getSendBuffer(), "");
        ASSERT_FALSE("Client: 全送信後は送信待ちなし",
                     client.hasPendingOutput());
    }

    {
        Client client(12, "127.0.0.1");

        client.appendSendBuffer("ABC");
        client.eraseSendPrefix(1000);
        ASSERT_EQ("Client: eraseSendPrefix の過大な length は全削除",
                  client.getSendBuffer(), "");
        ASSERT_FALSE("Client: 全削除後は送信待ちなし",
                     client.hasPendingOutput());
        client.eraseSendPrefix(1);
        ASSERT_EQ("Client: 空バッファへの eraseSendPrefix も安全",
                  client.getSendBuffer(), "");

        client.appendSendBuffer("");
        ASSERT_FALSE("Client: 空文字の追記では送信待ちにならない",
                     client.hasPendingOutput());
    }

    {
        Client client(13, "127.0.0.1");

        client.appendReceiveBuffer("ABC", 3);
        client.appendSendBuffer("DEF");
        client.clearBuffers();
        ASSERT_EQ("Client: clearBuffers で受信バッファが空",
                  client.getReceiveBuffer(), "");
        ASSERT_EQ("Client: clearBuffers で送信バッファが空",
                  client.getSendBuffer(), "");
        ASSERT_FALSE("Client: clearBuffers 後は送信待ちなし",
                     client.hasPendingOutput());
    }

    /* ── Channel 関係 ─────────────────────── */

    {
        Client client(14, "127.0.0.1");

        ASSERT_FALSE("Client: 未参加の Channel",
                     client.hasJoinedChannel("#general"));
        ASSERT_TRUE("Client: addJoinedChannel は追加時 true",
                    client.addJoinedChannel("#general"));
        ASSERT_TRUE("Client: hasJoinedChannel",
                    client.hasJoinedChannel("#general"));
        ASSERT_EQ("Client: 参加 Channel 数",
                  client.getJoinedChannels().size(),
                  static_cast<std::size_t>(1));

        /* 集合が変更されなければ false */
        ASSERT_FALSE("Client: 重複 addJoinedChannel は false",
                     client.addJoinedChannel("#general"));
        ASSERT_EQ("Client: 重複追加しても Channel 数は増えない",
                  client.getJoinedChannels().size(),
                  static_cast<std::size_t>(1));

        ASSERT_TRUE("Client: 2 つ目の Channel へ参加",
                    client.addJoinedChannel("#dev"));
        ASSERT_EQ("Client: 参加 Channel 数は 2",
                  client.getJoinedChannels().size(),
                  static_cast<std::size_t>(2));

        ASSERT_TRUE("Client: removeJoinedChannel は削除時 true",
                    client.removeJoinedChannel("#general"));
        ASSERT_FALSE("Client: 削除後は hasJoinedChannel が false",
                     client.hasJoinedChannel("#general"));
        ASSERT_FALSE("Client: 未参加 Channel の removeJoinedChannel は false",
                     client.removeJoinedChannel("#general"));
        ASSERT_EQ("Client: 削除後の Channel 数",
                  client.getJoinedChannels().size(),
                  static_cast<std::size_t>(1));

        /* Client は渡された Key をそのまま保持する。
           正規化は Server が normalizeChannelName() で行う */
        ASSERT_FALSE("Client: 正規化前の名前では見つからない",
                     client.hasJoinedChannel("#DEV"));
        ASSERT_TRUE("Client: 正規化済みの名前で見つかる",
                    client.hasJoinedChannel(
                        IrcUtil::normalizeChannelName("#DEV")));
    }

    /* ── 値としてコピーできる (std::map<int, Client> 用) ── */

    {
        Client original(15, "127.0.0.1");

        original.setNickname("alice");
        original.acceptPassword();
        original.addJoinedChannel("#general");
        original.appendSendBuffer("queued");

        Client copy(original);

        ASSERT_EQ("Client: コピーの FD", copy.getFd(), 15);
        ASSERT_EQ("Client: コピーの Nickname", copy.getNickname(), "alice");
        ASSERT_TRUE("Client: コピーの PASS 状態", copy.isPasswordAccepted());
        ASSERT_TRUE("Client: コピーの参加 Channel",
                    copy.hasJoinedChannel("#general"));
        ASSERT_EQ("Client: コピーの送信バッファ", copy.getSendBuffer(),
                  "queued");

        /* コピーは独立している */
        copy.setNickname("bob");
        ASSERT_EQ("Client: コピー変更は元へ影響しない",
                  original.getNickname(), "alice");
    }

    /* ── ネットワーク担当の利用フロー ─────── */

    {
        /* recv() → appendReceiveBuffer → findLine → eraseReceivePrefix
           → Parser::parse という一連の流れを再現する */
        Client client(16, "127.0.0.1");

        /* NICK 行がパケット境界で分割されている */
        const char *packet1 = "PASS secret\r\nNICK ali";
        const char *packet2 = "ce\r\nUSER u 0 * :Real Name\r\n";

        client.appendReceiveBuffer(packet1, std::strlen(packet1));
        client.appendReceiveBuffer(packet2, std::strlen(packet2));

        /* Command だけを集めると、分割された "ali" + "ce" が復元されず
           "NICK ali" になっても Command は "NICK" のままで通ってしまう。
           Parameter まで含めて復元結果を検証する。
           範囲外アクセスを避けるため結果は連結して比較する */
        std::string parsed;
        std::string line;
        std::size_t consumed;
        int         guard = 0;

        /* findLine が consumed == 0 で LINE_EXTRACTED を返す退行が起きても
           無限ループにせず、テストとして失敗させる */
        while (BufferUtil::findLine(client.getReceiveBuffer(), line, consumed)
                   == BufferUtil::LINE_EXTRACTED
               && ++guard < 100)
        {
            client.eraseReceivePrefix(consumed);

            Message message;
            if (Parser::parse(line, message))
            {
                if (!parsed.empty())
                    parsed += ",";
                parsed += message.command;
                for (std::size_t i = 0; i < message.params.size(); ++i)
                    parsed += " " + message.params[i];
            }
        }

        ASSERT_TRUE("Client: 受信ループが有限回で終了する", guard < 100);
        ASSERT_EQ("Client: 分割受信を復元して 3 コマンドを完全に得る", parsed,
                  "PASS secret,NICK alice,USER u 0 * Real Name");
        ASSERT_EQ("Client: 受信フロー完了後のバッファは空",
                  client.getReceiveBuffer(), "");
    }

    {
        /* appendSendBuffer → getSendBuffer → send() → eraseSendPrefix
           という送信フローを、毎回 5 byte しか送れない状況で再現する */
        Client client(17, "127.0.0.1");

        client.appendSendBuffer("0123456789");

        std::string sent;
        int         loops = 0;

        while (client.hasPendingOutput() && loops < 10)
        {
            const std::string &out = client.getSendBuffer();
            std::size_t        n   = out.size() < 5 ? out.size() : 5;

            sent += out.substr(0, n);
            client.eraseSendPrefix(n);
            ++loops;
        }

        ASSERT_EQ("Client: 部分送信を繰り返して全送信できる",
                  sent, "0123456789");
        ASSERT_FALSE("Client: 全送信後は送信待ちなし",
                     client.hasPendingOutput());
    }

    /* ── 登録順序の全 6 通り ──────────────── */

    {
        /* PASS / NICK / USER の受信順序は固定しない (設計書 04 §5)。
           3 つ揃った時点で、かつその時だけ登録完了する */
        const char *orders[6][3] = {{"P", "N", "U"}, {"P", "U", "N"},
                                    {"N", "P", "U"}, {"N", "U", "P"},
                                    {"U", "P", "N"}, {"U", "N", "P"}};

        for (int i = 0; i < 6; ++i)
        {
            Client client(20 + i, "127.0.0.1");

            for (int step = 0; step < 3; ++step)
            {
                ASSERT_FALSE("Client: 3 要素が揃う前は登録完了しない",
                             client.tryCompleteRegistration());

                switch (orders[i][step][0])
                {
                case 'P':
                    client.acceptPassword();
                    break;
                case 'N':
                    client.setNickname("alice");
                    break;
                case 'U':
                    client.setUser("u", "Real");
                    break;
                }
            }
            ASSERT_TRUE("Client: 任意の順序で 3 要素が揃えば登録完了",
                        client.tryCompleteRegistration());
            ASSERT_FALSE("Client: 2 回目は false (Welcome 二重送信を防ぐ)",
                         client.tryCompleteRegistration());
        }
    }

    /* ── 再実行と冪等性 ───────────────────── */

    {
        Client client(30, "127.0.0.1");

        client.acceptPassword();
        client.acceptPassword();
        ASSERT_TRUE("Client: acceptPassword は冪等",
                    client.isPasswordAccepted());

        client.setUser("u1", "Real1");
        client.setUser("u2", "Real2");
        ASSERT_EQ("Client: 2 回目の setUser で Username を上書き",
                  client.getUsername(), "u2");
        ASSERT_EQ("Client: 2 回目の setUser で Realname を上書き",
                  client.getRealname(), "Real2");
        ASSERT_TRUE("Client: 2 回目の setUser でも hasUser", client.hasUser());
    }

    {
        /* clearBuffers はバッファだけを消す。登録状態や Nickname、
           参加 Channel を消す実装になっていないことを確認する */
        Client client(31, "127.0.0.1");

        client.setNickname("alice");
        client.acceptPassword();
        client.setUser("u", "Real");
        client.tryCompleteRegistration();
        client.addJoinedChannel("#a");
        client.appendReceiveBuffer("X", 1);
        client.appendSendBuffer("Y");

        client.clearBuffers();
        ASSERT_TRUE("Client: clearBuffers は登録状態を消さない",
                    client.isRegistered());
        ASSERT_EQ("Client: clearBuffers は Nickname を消さない",
                  client.getNickname(), "alice");
        ASSERT_TRUE("Client: clearBuffers は参加 Channel を消さない",
                    client.hasJoinedChannel("#a"));
        ASSERT_TRUE("Client: clearBuffers は PASS 状態を消さない",
                    client.isPasswordAccepted());
    }

    {
        /* 送信バッファも NUL を扱える (受信側と対称) */
        Client client(32, "127.0.0.1");

        client.appendSendBuffer(std::string("A\0B", 3));
        ASSERT_EQ("Client: NUL を含む送信データの長さ",
                  client.getSendBuffer().size(), static_cast<std::size_t>(3));
        ASSERT_EQ("Client: NUL を含む送信データの内容", client.getSendBuffer(),
                  std::string("A\0B", 3));

        /* 空文字の追記は既存データを壊さない */
        client.appendSendBuffer("");
        ASSERT_EQ("Client: 空文字追記は既存データを壊さない",
                  client.getSendBuffer().size(), static_cast<std::size_t>(3));
        ASSERT_TRUE("Client: 空文字追記後も送信待ちのまま",
                    client.hasPendingOutput());
    }

    /* ── std::map<int, Client> への格納 ───── */

    {
        /* 実際に std::map へ格納し、コピー代入 (operator[] が使う経路) で
           全フィールドが運ばれることを確認する。bool フラグや Hostname を
           落とすコピーは、部分的な確認では検出できない */
        std::map<int, Client> clients;
        Client                source(42, "127.0.0.1");

        source.setNickname("alice");
        source.acceptPassword();
        source.setUser("u", "Real");
        source.tryCompleteRegistration();
        source.addJoinedChannel("#a");
        source.appendReceiveBuffer("PARTIAL", 7);
        source.appendSendBuffer("QUEUED");

        clients[42] = source; /* Default 構築 + コピー代入 */

        ASSERT_EQ("Client: map 経由の FD", clients[42].getFd(), 42);
        ASSERT_EQ("Client: map 経由の Hostname", clients[42].getHostname(),
                  "127.0.0.1");
        ASSERT_EQ("Client: map 経由の Nickname", clients[42].getNickname(),
                  "alice");
        ASSERT_EQ("Client: map 経由の Username", clients[42].getUsername(),
                  "u");
        ASSERT_EQ("Client: map 経由の Realname", clients[42].getRealname(),
                  "Real");
        ASSERT_TRUE("Client: map 経由の PASS 状態",
                    clients[42].isPasswordAccepted());
        ASSERT_TRUE("Client: map 経由の hasUser", clients[42].hasUser());
        ASSERT_TRUE("Client: map 経由の登録状態", clients[42].isRegistered());
        ASSERT_TRUE("Client: map 経由の参加 Channel",
                    clients[42].hasJoinedChannel("#a"));
        ASSERT_EQ("Client: map 経由の受信バッファ",
                  clients[42].getReceiveBuffer(), "PARTIAL");
        ASSERT_EQ("Client: map 経由の送信バッファ", clients[42].getSendBuffer(),
                  "QUEUED");

        /* map 内の要素を変更しても元へ影響しない */
        clients[42].appendReceiveBuffer("X", 1);
        ASSERT_EQ("Client: map 内での変更は元へ影響しない",
                  source.getReceiveBuffer(), "PARTIAL");
    }
}
