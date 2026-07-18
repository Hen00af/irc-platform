#include "TestRunner.hpp"

#include "prd/domain/Channel.hpp"
#include "prd/util/IrcUtil.hpp"

#include <map>

void runChannelTests()
{
    TestRunner::beginSuite("Channel");

    /* ── Constructor と初期状態 ───────────── */

    {
        Channel channel("#General", "#general");

        /* 表示名は元の綴りを保ち、検索 Key だけ正規化済み */
        ASSERT_EQ("Channel: getName は表示名", channel.getName(), "#General");
        ASSERT_EQ("Channel: getKeyName は正規化名",
                  channel.getKeyName(), "#general");
        ASSERT_EQ("Channel: 新規 Channel の Topic は空",
                  channel.getTopic(), "");
        ASSERT_FALSE("Channel: 新規 Channel は Topic なし", channel.hasTopic());
        ASSERT_TRUE("Channel: 新規 Channel は空", channel.isEmpty());
        ASSERT_EQ("Channel: 新規 Channel の Member 数は 0",
                  channel.getMemberCount(), static_cast<std::size_t>(0));
        ASSERT_FALSE("Channel: 新規 Channel は invite-only ではない",
                     channel.isInviteOnly());
        /* 設計書 02 §6.2: 新規 Channel では t を有効にする */
        ASSERT_TRUE("Channel: 新規 Channel は topic 制限が有効",
                    channel.isTopicRestricted());
        ASSERT_FALSE("Channel: 新規 Channel に Key はない", channel.hasKey());
        ASSERT_EQ("Channel: 新規 Channel の Key は空",
                  channel.getChannelKey(), "");
        ASSERT_FALSE("Channel: 新規 Channel に Limit はない",
                     channel.hasUserLimit());
        ASSERT_EQ("Channel: 新規 Channel の Limit は 0",
                  channel.getUserLimit(), static_cast<std::size_t>(0));
        ASSERT_FALSE("Channel: 新規 Channel は満員でない", channel.isFull());
        ASSERT_EQ("Channel: 新規 Channel の Mode 文字列",
                  channel.buildModeString(), "+t");
    }

    {
        /* Default Constructor は std::map 挿入用 */
        Channel channel;

        ASSERT_EQ("Channel: Default Constructor の名前は空",
                  channel.getName(), "");
        ASSERT_TRUE("Channel: Default Constructor は空", channel.isEmpty());
        ASSERT_TRUE("Channel: Default Constructor も topic 制限が有効",
                    channel.isTopicRestricted());
    }

    {
        /* 正規化は呼び出し側 (Server) の責務。Channel は受け取った値を保持する */
        std::string name = "#Foo[Bar]";
        Channel     channel(name, IrcUtil::normalizeChannelName(name));

        ASSERT_EQ("Channel: 表示名は正規化しない", channel.getName(),
                  "#Foo[Bar]");
        ASSERT_EQ("Channel: Key は case mapping 済み",
                  channel.getKeyName(), "#foo{bar}");
    }

    /* ── Topic ────────────────────────────── */

    {
        Channel channel("#general", "#general");

        channel.setTopic("Hello World");
        ASSERT_EQ("Channel: setTopic", channel.getTopic(), "Hello World");
        ASSERT_TRUE("Channel: setTopic で hasTopic", channel.hasTopic());

        channel.setTopic("Changed");
        ASSERT_EQ("Channel: Topic の上書き", channel.getTopic(), "Changed");

        /* 空文字 Topic は Topic 削除を意味する (設計書 04 §14) */
        channel.setTopic("");
        ASSERT_EQ("Channel: 空文字 Topic", channel.getTopic(), "");
        ASSERT_FALSE("Channel: 空文字 Topic は hasTopic が false",
                     channel.hasTopic());

        /* Topic に空白や colon を含められる */
        channel.setTopic("a: b  c");
        ASSERT_EQ("Channel: 空白と colon を含む Topic",
                  channel.getTopic(), "a: b  c");
    }

    /* ── Member ───────────────────────────── */

    {
        Channel channel("#general", "#general");

        ASSERT_FALSE("Channel: 未参加 FD の hasMember",
                     channel.hasMember(3));
        ASSERT_TRUE("Channel: addMember は追加時 true", channel.addMember(3));
        ASSERT_TRUE("Channel: addMember 後の hasMember",
                    channel.hasMember(3));
        ASSERT_FALSE("Channel: Member がいれば空でない", channel.isEmpty());
        ASSERT_EQ("Channel: Member 数は 1", channel.getMemberCount(),
                  static_cast<std::size_t>(1));

        /* 同じ FD は重複追加できない */
        ASSERT_FALSE("Channel: 重複 addMember は false", channel.addMember(3));
        ASSERT_EQ("Channel: 重複追加しても Member 数は増えない",
                  channel.getMemberCount(), static_cast<std::size_t>(1));

        ASSERT_TRUE("Channel: 2 人目の addMember", channel.addMember(4));
        ASSERT_TRUE("Channel: 3 人目の addMember", channel.addMember(5));
        ASSERT_EQ("Channel: Member 数は 3", channel.getMemberCount(),
                  static_cast<std::size_t>(3));

        /* getMembers は FD 集合を返す。Client オブジェクトは返さない */
        const std::set<int> &members = channel.getMembers();
        ASSERT_EQ("Channel: getMembers の要素数", members.size(),
                  static_cast<std::size_t>(3));
        ASSERT_TRUE("Channel: getMembers に FD 3 が含まれる",
                    members.find(3) != members.end());
        ASSERT_TRUE("Channel: getMembers に FD 5 が含まれる",
                    members.find(5) != members.end());

        ASSERT_TRUE("Channel: removeMember は削除時 true",
                    channel.removeMember(4));
        ASSERT_FALSE("Channel: 削除後の hasMember", channel.hasMember(4));
        ASSERT_EQ("Channel: 削除後の Member 数", channel.getMemberCount(),
                  static_cast<std::size_t>(2));

        /* 未参加 FD の削除で状態が壊れない */
        ASSERT_FALSE("Channel: 未参加 FD の removeMember は false",
                     channel.removeMember(4));
        ASSERT_FALSE("Channel: 存在しない FD の removeMember は false",
                     channel.removeMember(999));
        ASSERT_EQ("Channel: 無効な削除で Member 数は変わらない",
                  channel.getMemberCount(), static_cast<std::size_t>(2));

        /* 全員退出すると空になる (Server が Channel 削除を判断する) */
        channel.removeMember(3);
        channel.removeMember(5);
        ASSERT_TRUE("Channel: 全員退出後は空", channel.isEmpty());
        ASSERT_EQ("Channel: 全員退出後の Member 数",
                  channel.getMemberCount(), static_cast<std::size_t>(0));
    }

    /* ── Operator ─────────────────────────── */

    {
        Channel channel("#general", "#general");

        /* Member でない FD は Operator にできない (不変条件) */
        ASSERT_FALSE("Channel: Member でない FD の addOperator は false",
                     channel.addOperator(3));
        ASSERT_FALSE("Channel: 追加されていないので isOperator も false",
                     channel.isOperator(3));

        channel.addMember(3);
        ASSERT_FALSE("Channel: 参加直後は Operator ではない",
                     channel.isOperator(3));
        ASSERT_TRUE("Channel: Member なら addOperator できる",
                    channel.addOperator(3));
        ASSERT_TRUE("Channel: addOperator 後の isOperator",
                    channel.isOperator(3));

        ASSERT_FALSE("Channel: 重複 addOperator は false",
                     channel.addOperator(3));

        ASSERT_TRUE("Channel: removeOperator は削除時 true",
                    channel.removeOperator(3));
        ASSERT_FALSE("Channel: removeOperator 後の isOperator",
                     channel.isOperator(3));
        ASSERT_FALSE("Channel: 既に Operator でない FD の removeOperator",
                     channel.removeOperator(3));
        /* -o しても Member のまま */
        ASSERT_TRUE("Channel: removeOperator しても Member のまま",
                    channel.hasMember(3));
    }

    /* DD-012「最後の Operator が退出・-o されても自動昇格しない」は
       Channel ではなく Server / JOIN 層の方針であり、Channel には昇格の
       コード経路自体が存在しない。ここで isOperator(他人) == false を
       確認しても「std::set が勝手に増えないこと」を確かめるだけの恒真
       テストになるため、Server の作業単位で検証する */

    {
        /* removeMember は Operator 集合からも取り除く */
        Channel channel("#general", "#general");

        channel.addMember(3);
        channel.addOperator(3);
        ASSERT_TRUE("Channel: Operator である", channel.isOperator(3));

        channel.removeMember(3);
        ASSERT_FALSE("Channel: removeMember で Member から消える",
                     channel.hasMember(3));
        ASSERT_FALSE("Channel: removeMember で Operator からも消える",
                     channel.isOperator(3));

        /* 同じ FD が再参加しても Operator 権限は復活しない */
        channel.addMember(3);
        ASSERT_FALSE("Channel: 再参加しても Operator ではない",
                     channel.isOperator(3));
    }

    /* ── Invite ───────────────────────────── */

    {
        Channel channel("#general", "#general");

        ASSERT_FALSE("Channel: 未 Invite の isInvited", channel.isInvited(3));
        ASSERT_TRUE("Channel: addInvite は追加時 true", channel.addInvite(3));
        ASSERT_TRUE("Channel: addInvite 後の isInvited",
                    channel.isInvited(3));
        ASSERT_FALSE("Channel: 重複 addInvite は false",
                     channel.addInvite(3));

        /* Invite は Member でなくても追加できる (参加前の許可情報) */
        ASSERT_FALSE("Channel: Invite されても Member ではない",
                     channel.hasMember(3));

        ASSERT_TRUE("Channel: removeInvite は削除時 true",
                    channel.removeInvite(3));
        ASSERT_FALSE("Channel: removeInvite 後の isInvited",
                     channel.isInvited(3));
        ASSERT_FALSE("Channel: 未 Invite の removeInvite は false",
                     channel.removeInvite(3));
    }

    {
        /* Invite は参加許可情報のため removeMember では消さない。
           削除は Server が明示的に行う (設計書 02 §6.5) */
        Channel channel("#general", "#general");

        channel.addInvite(3);
        channel.addMember(3);
        channel.removeMember(3);
        ASSERT_TRUE("Channel: removeMember は Invite を消さない",
                    channel.isInvited(3));
    }

    /* ── Mode i ───────────────────────────── */

    {
        Channel channel("#general", "#general");

        ASSERT_FALSE("Channel: i の初期値", channel.isInviteOnly());
        channel.setInviteOnly(true);
        ASSERT_TRUE("Channel: +i", channel.isInviteOnly());
        channel.setInviteOnly(false);
        ASSERT_FALSE("Channel: -i", channel.isInviteOnly());

        /* -i にしても Invite 集合は保持する (設計書 05 §9) */
        channel.addInvite(3);
        channel.setInviteOnly(true);
        channel.setInviteOnly(false);
        ASSERT_TRUE("Channel: -i で Invite 集合は消えない",
                    channel.isInvited(3));
    }

    /* ── Mode t ───────────────────────────── */

    {
        Channel channel("#general", "#general");

        ASSERT_TRUE("Channel: t の初期値は有効", channel.isTopicRestricted());
        channel.setTopicRestricted(false);
        ASSERT_FALSE("Channel: -t", channel.isTopicRestricted());
        channel.setTopicRestricted(true);
        ASSERT_TRUE("Channel: +t", channel.isTopicRestricted());
    }

    /* ── Mode k ───────────────────────────── */

    {
        Channel channel("#general", "#general");

        ASSERT_FALSE("Channel: k の初期値", channel.hasKey());
        /* Key 未設定なら、どんな Key を渡しても参加できる */
        ASSERT_TRUE("Channel: Key 未設定なら matchesKey は常に true",
                    channel.matchesKey("anything"));
        ASSERT_TRUE("Channel: Key 未設定なら空 Key も true",
                    channel.matchesKey(""));

        channel.setChannelKey("secret");
        ASSERT_TRUE("Channel: +k で hasKey", channel.hasKey());
        ASSERT_EQ("Channel: getChannelKey", channel.getChannelKey(), "secret");
        ASSERT_TRUE("Channel: 一致する Key", channel.matchesKey("secret"));
        ASSERT_FALSE("Channel: 一致しない Key", channel.matchesKey("wrong"));
        ASSERT_FALSE("Channel: 空 Key は不一致", channel.matchesKey(""));
        /* 完全一致で比較する (大文字小文字を区別) */
        ASSERT_FALSE("Channel: Key は大文字小文字を区別",
                     channel.matchesKey("SECRET"));
        ASSERT_FALSE("Channel: 前方一致では通らない",
                     channel.matchesKey("secretX"));

        /* Operator は既存 Key を置換できる (設計書 05 §11) */
        channel.setChannelKey("newkey");
        ASSERT_EQ("Channel: Key の置換", channel.getChannelKey(), "newkey");
        ASSERT_TRUE("Channel: 置換後の新しい Key で一致",
                    channel.matchesKey("newkey"));
        ASSERT_FALSE("Channel: 置換後は古い Key で不一致",
                     channel.matchesKey("secret"));

        channel.clearChannelKey();
        ASSERT_FALSE("Channel: -k で hasKey が false", channel.hasKey());
        ASSERT_EQ("Channel: -k で Key が空", channel.getChannelKey(), "");
        ASSERT_TRUE("Channel: -k 後は matchesKey が常に true",
                    channel.matchesKey("secret"));

        /* -k は繰り返しても安全 */
        channel.clearChannelKey();
        ASSERT_FALSE("Channel: -k の繰り返しも安全", channel.hasKey());

        /* 同一 Key の再設定は冪等 */
        channel.setChannelKey("k1");
        channel.setChannelKey("k1");
        ASSERT_EQ("Channel: 同一 Key の再設定は冪等", channel.getChannelKey(),
                  "k1");
    }

    {
        /* 退化状態: setChannelKey("") は hasKey() == true かつ Key が空。
           324 の Parameter が空文字になり不正な行を生む。設計書 05 §11 が
           「1 文字以上 23 文字以下」の検証を MODE Handler の責務としており、
           Channel 側は値を保持するだけ。Handler 未実装の現時点では
           これを防ぐ実装がツリー上に存在しないことを、ここで固定しておく */
        Channel channel("#general", "#general");

        channel.setChannelKey("");
        ASSERT_TRUE("Channel: 空 Key でも hasKey は true (Handler が防ぐ責務)",
                    channel.hasKey());
        ASSERT_EQ("Channel: 空 Key の getChannelKey", channel.getChannelKey(),
                  "");
        ASSERT_TRUE("Channel: 空 Key は空文字と一致", channel.matchesKey(""));
        ASSERT_FALSE("Channel: 空 Key は非空文字と不一致",
                     channel.matchesKey("x"));
        ASSERT_EQ("Channel: 空 Key でも Parameter は 1 つ出る",
                  channel.buildModeParameters().size(),
                  static_cast<std::size_t>(1));
        ASSERT_EQ("Channel: 空 Key の Parameter は空文字 (不正な 324 になる)",
                  channel.buildModeParameters()[0], "");
    }

    /* ── Mode l ───────────────────────────── */

    {
        Channel channel("#general", "#general");

        ASSERT_FALSE("Channel: l の初期値", channel.hasUserLimit());
        ASSERT_FALSE("Channel: Limit なしなら満員にならない", channel.isFull());

        channel.setUserLimit(2);
        ASSERT_TRUE("Channel: +l で hasUserLimit", channel.hasUserLimit());
        ASSERT_EQ("Channel: getUserLimit", channel.getUserLimit(),
                  static_cast<std::size_t>(2));
        ASSERT_FALSE("Channel: Member 0 / Limit 2 は満員でない",
                     channel.isFull());

        channel.addMember(3);
        ASSERT_FALSE("Channel: Member 1 / Limit 2 は満員でない",
                     channel.isFull());
        channel.addMember(4);
        ASSERT_TRUE("Channel: Member 2 / Limit 2 は満員", channel.isFull());

        /* 現在の Member 数より小さい Limit も設定できる。
           既存 Member は削除せず、新規 JOIN だけ拒否する (設計書 05 §13) */
        channel.setUserLimit(1);
        ASSERT_EQ("Channel: Member 数より小さい Limit も設定できる",
                  channel.getUserLimit(), static_cast<std::size_t>(1));
        ASSERT_EQ("Channel: 小さい Limit でも既存 Member は残る",
                  channel.getMemberCount(), static_cast<std::size_t>(2));
        ASSERT_TRUE("Channel: Member 2 / Limit 1 は満員", channel.isFull());

        channel.removeMember(4);
        channel.removeMember(3);
        ASSERT_FALSE("Channel: Member 0 / Limit 1 は満員でない",
                     channel.isFull());

        channel.clearUserLimit();
        ASSERT_FALSE("Channel: -l で hasUserLimit が false",
                     channel.hasUserLimit());
        ASSERT_EQ("Channel: -l で Limit が 0", channel.getUserLimit(),
                  static_cast<std::size_t>(0));
        ASSERT_FALSE("Channel: -l 後は満員にならない", channel.isFull());

        /* -l は繰り返しても安全 */
        channel.clearUserLimit();
        ASSERT_FALSE("Channel: -l の繰り返しも安全", channel.hasUserLimit());
    }

    {
        /* Limit 1 の境界 */
        Channel channel("#general", "#general");

        channel.setUserLimit(1);
        ASSERT_FALSE("Channel: Limit 1 / Member 0 は満員でない",
                     channel.isFull());
        channel.addMember(3);
        ASSERT_TRUE("Channel: Limit 1 / Member 1 は満員", channel.isFull());
    }

    {
        /* 退化状態: setUserLimit(0) は誰も参加できない Channel を作る
           (size() >= 0 は unsigned では恒真)。設計書 05 §13 が「0 を
           許可しない」検証を MODE Handler の責務としており、Channel 側は
           値を保持するだけ。Handler 未実装の現時点ではこれを防ぐ実装が
           ツリー上に存在しないことを、ここで固定しておく */
        Channel channel("#general", "#general");

        channel.setUserLimit(0);
        ASSERT_TRUE("Channel: Limit 0 でも hasUserLimit は true",
                    channel.hasUserLimit());
        ASSERT_EQ("Channel: Limit 0 の getUserLimit", channel.getUserLimit(),
                  static_cast<std::size_t>(0));
        ASSERT_TRUE("Channel: Limit 0 は Member 0 でも満員 (誰も参加できない)",
                    channel.isFull());

        /* setUserLimit(0) と clearUserLimit() は _userLimit が同じ 0 で、
           _limitEnabled だけが違う。両者を取り違える実装を検出する */
        channel.clearUserLimit();
        ASSERT_FALSE("Channel: clearUserLimit は setUserLimit(0) と別状態",
                     channel.hasUserLimit());
        ASSERT_FALSE("Channel: -l 後は Member 0 で満員でない",
                     channel.isFull());
    }

    {
        /* Mode setter の冪等性。Channel の setter は void を返すため、
           Handler は「実変更があったか」を read-then-set で判断する必要が
           ある (設計書 05 §8「実変更がない Mode を通知しない」) */
        Channel channel("#general", "#general");

        channel.setInviteOnly(true);
        channel.setInviteOnly(true);
        ASSERT_TRUE("Channel: +i の二重適用は冪等", channel.isInviteOnly());
        channel.setTopicRestricted(false);
        channel.setTopicRestricted(false);
        ASSERT_FALSE("Channel: -t の二重適用は冪等",
                     channel.isTopicRestricted());
    }

    /* ── Mode 文字列 ──────────────────────── */

    {
        Channel channel("#general", "#general");

        /* 新規 Channel は +t が既定 */
        ASSERT_EQ("Channel: buildModeString 既定", channel.buildModeString(),
                  "+t");

        channel.setTopicRestricted(false);
        ASSERT_EQ("Channel: Mode がなければ + だけ",
                  channel.buildModeString(), "+");
        ASSERT_EQ("Channel: Mode がなければ Parameter も空",
                  channel.buildModeParameters().size(),
                  static_cast<std::size_t>(0));

        /* 表示順は itkl 固定 */
        channel.setInviteOnly(true);
        ASSERT_EQ("Channel: +i のみ", channel.buildModeString(), "+i");
        channel.setTopicRestricted(true);
        ASSERT_EQ("Channel: +it", channel.buildModeString(), "+it");
        channel.setChannelKey("secret");
        ASSERT_EQ("Channel: +itk", channel.buildModeString(), "+itk");
        channel.setUserLimit(10);
        ASSERT_EQ("Channel: +itkl", channel.buildModeString(), "+itkl");

        /* Parameter は itkl 順で key, limit */
        std::vector<std::string> parameters = channel.buildModeParameters();
        ASSERT_EQ("Channel: Parameter 数", parameters.size(),
                  static_cast<std::size_t>(2));
        ASSERT_EQ("Channel: Parameter 1 番目は Key", parameters[0], "secret");
        ASSERT_EQ("Channel: Parameter 2 番目は Limit", parameters[1], "10");

        /* o は Channel 全体の状態ではないため Mode 文字列に含めない */
        channel.addMember(3);
        channel.addOperator(3);
        ASSERT_EQ("Channel: Operator がいても o は含めない",
                  channel.buildModeString(), "+itkl");
    }

    {
        /* Key だけ / Limit だけの Parameter */
        Channel channel("#general", "#general");

        channel.setTopicRestricted(false);
        channel.setChannelKey("k");
        ASSERT_EQ("Channel: +k のみ", channel.buildModeString(), "+k");
        ASSERT_EQ("Channel: +k のみの Parameter 数",
                  channel.buildModeParameters().size(),
                  static_cast<std::size_t>(1));
        ASSERT_EQ("Channel: +k のみの Parameter",
                  channel.buildModeParameters()[0], "k");

        channel.clearChannelKey();
        channel.setUserLimit(42);
        ASSERT_EQ("Channel: +l のみ", channel.buildModeString(), "+l");
        ASSERT_EQ("Channel: +l のみの Parameter 数",
                  channel.buildModeParameters().size(),
                  static_cast<std::size_t>(1));
        ASSERT_EQ("Channel: +l のみの Parameter は数値文字列",
                  channel.buildModeParameters()[0], "42");
    }

    {
        /* 順序は itkl 固定であり、設定した順ではない。他のテストは
           すべて key → limit の順で設定しているため、挿入順で返す実装でも
           通ってしまう。ここでは limit を先に設定して順序を検証する */
        Channel channel("#general", "#general");

        channel.setUserLimit(10);
        channel.setChannelKey("secret");
        ASSERT_EQ("Channel: 逆順に設定しても Mode 文字列は itkl 順",
                  channel.buildModeString(), "+tkl");

        std::vector<std::string> parameters = channel.buildModeParameters();
        ASSERT_EQ("Channel: 逆順設定でも Parameter 数", parameters.size(),
                  static_cast<std::size_t>(2));
        if (parameters.size() == 2)
        {
            ASSERT_EQ("Channel: 逆順設定でも Key が先", parameters[0],
                      "secret");
            ASSERT_EQ("Channel: 逆順設定でも Limit が後", parameters[1], "10");
        }
    }

    {
        /* i / t / k / l の全 16 組合せ。期待値は設計書 05 §4
           「有効な文字だけを itkl 順で連結する」から導く */
        for (int mask = 0; mask < 16; ++mask)
        {
            Channel channel("#g", "#g");

            /* 既定は +t なので、いったん落としてから組み立てる */
            channel.setTopicRestricted(false);
            if (mask & 1)
                channel.setInviteOnly(true);
            if (mask & 2)
                channel.setTopicRestricted(true);
            if (mask & 4)
                channel.setChannelKey("K");
            if (mask & 8)
                channel.setUserLimit(7);

            std::string expected = "+";
            if (mask & 1)
                expected += 'i';
            if (mask & 2)
                expected += 't';
            if (mask & 4)
                expected += 'k';
            if (mask & 8)
                expected += 'l';

            ASSERT_EQ("Channel: buildModeString 全 16 組合せ",
                      channel.buildModeString(), expected);

            std::size_t expectedParams = 0;
            if (mask & 4)
                ++expectedParams;
            if (mask & 8)
                ++expectedParams;
            ASSERT_EQ("Channel: buildModeParameters 全 16 組合せの数",
                      channel.buildModeParameters().size(), expectedParams);
        }
    }

    /* ── 値としてコピーできる (std::map<string, Channel> 用) ── */

    {
        Channel original("#general", "#general");

        original.addMember(3);
        original.addOperator(3);
        original.addInvite(4);
        original.setTopic("topic");
        original.setChannelKey("secret");
        original.setUserLimit(5);

        Channel copy(original);

        ASSERT_EQ("Channel: コピーの名前", copy.getName(), "#general");
        ASSERT_EQ("Channel: コピーの Topic", copy.getTopic(), "topic");
        ASSERT_TRUE("Channel: コピーの Member", copy.hasMember(3));
        ASSERT_TRUE("Channel: コピーの Operator", copy.isOperator(3));
        ASSERT_TRUE("Channel: コピーの Invite", copy.isInvited(4));
        ASSERT_TRUE("Channel: コピーの Key", copy.matchesKey("secret"));
        ASSERT_EQ("Channel: コピーの Limit", copy.getUserLimit(),
                  static_cast<std::size_t>(5));

        /* コピーは独立している */
        copy.addMember(9);
        ASSERT_FALSE("Channel: コピー変更は元へ影響しない",
                     original.hasMember(9));
    }

    {
        /* 実際に std::map<std::string, Channel> へ格納し、コピー代入
           (operator[] が使う経路) で全フィールドが運ばれることを確認する。
           bool フラグを落とすコピーは、部分的な確認では検出できない */
        std::map<std::string, Channel> channels;
        Channel                        source("#General", "#general");

        source.addMember(3);
        source.addOperator(3);
        source.addInvite(4);
        source.setTopic("hello");
        source.setInviteOnly(true);
        source.setChannelKey("secret");
        source.setUserLimit(5);

        channels["#general"] = source; /* Default 構築 + コピー代入 */

        ASSERT_EQ("Channel: map 経由の表示名", channels["#general"].getName(),
                  "#General");
        ASSERT_EQ("Channel: map 経由の Key 名",
                  channels["#general"].getKeyName(), "#general");
        ASSERT_EQ("Channel: map 経由の Topic", channels["#general"].getTopic(),
                  "hello");
        ASSERT_TRUE("Channel: map 経由の Member",
                    channels["#general"].hasMember(3));
        ASSERT_EQ("Channel: map 経由の Member 数",
                  channels["#general"].getMemberCount(),
                  static_cast<std::size_t>(1));
        ASSERT_TRUE("Channel: map 経由の Operator",
                    channels["#general"].isOperator(3));
        ASSERT_TRUE("Channel: map 経由の Invite",
                    channels["#general"].isInvited(4));
        ASSERT_TRUE("Channel: map 経由の invite-only",
                    channels["#general"].isInviteOnly());
        ASSERT_TRUE("Channel: map 経由の topic 制限",
                    channels["#general"].isTopicRestricted());
        ASSERT_TRUE("Channel: map 経由の hasKey", channels["#general"].hasKey());
        ASSERT_EQ("Channel: map 経由の Key",
                  channels["#general"].getChannelKey(), "secret");
        ASSERT_TRUE("Channel: map 経由の hasUserLimit",
                    channels["#general"].hasUserLimit());
        ASSERT_EQ("Channel: map 経由の Limit",
                  channels["#general"].getUserLimit(),
                  static_cast<std::size_t>(5));
        ASSERT_EQ("Channel: map 経由の Mode 文字列",
                  channels["#general"].buildModeString(), "+itkl");

        /* map 内の要素を変更しても元へ影響しない */
        channels["#general"].addMember(9);
        ASSERT_FALSE("Channel: map 内での変更は元へ影響しない",
                     source.hasMember(9));
    }
}
