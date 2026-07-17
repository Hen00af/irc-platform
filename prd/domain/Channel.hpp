#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <vector>

/* ============================================================
 * Channel
 *
 * 1 チャンネル分の表示名・正規化 Key・Topic・Mandatory Mode 状態と、
 * Member / Operator / Invite の Client FD 集合を保持する。
 *
 * 対応モード:
 *   i - invite-only (招待制)
 *   t - topic 変更を Operator へ制限
 *   k - channel key (パスワード)
 *   o - operator 権限
 *   l - user limit (最大人数)
 *
 * 所有権と関係:
 *   - Client* を保持せず、FD (int) だけを保持する
 *   - Channel から Client オブジェクトは取得できない。Client の取得は
 *     Server が FD を使って行う
 *   - FD を所有しないため、デストラクタで close() しない
 *   - Client との関係変更は最終的に Server が統括する
 *
 * 本クラスは状態を保持して返すだけで、Numeric Reply の生成も Command の
 * 可否判定も行わない (設計書 05 §18)。入力値の検証は呼び出し側の責務。
 * ============================================================ */
class Channel
{
public:
    /* 原則使用しない。Server は
       insert(std::make_pair(key, Channel(name, key))) を使うこと。
       これはコピー Constructor を呼ぶので Default は不要である。
       Default が必要になるのは std::map::operator[] だけであり、
       _channels[key] は _name / _keyName が空の Channel を作って
       不変条件「_channels の Key と Channel::_keyName が一致する」を破る */
    Channel();
    /* keyName には IrcUtil::normalizeChannelName(name) を渡す。
       Channel を IrcUtil へ依存させないため、正規化は呼び出し側で行う */
    Channel(const std::string &name, const std::string &keyName);

    /* ── 基本 ─────────────────────────────── */
    const std::string &getName() const;    /* 表示名 */
    const std::string &getKeyName() const; /* 正規化済みの検索 Key */
    const std::string &getTopic() const;
    void               setTopic(const std::string &topic);
    bool               hasTopic() const;

    /* ── Member ───────────────────────────── */
    const std::set<int> &getMembers() const;
    std::size_t          getMemberCount() const;
    bool                 isEmpty() const;
    bool                 hasMember(int fd) const;
    /* 戻り値は集合が実際に変更されたかを表す */
    bool                 addMember(int fd);
    /* 不変条件「Operator は必ず Member」を保つため、Operator 集合からも
       同じ FD を取り除く。Invite は参加許可情報のため残す。削除は
       Server が明示的に行う */
    bool                 removeMember(int fd);

    /* ── Operator ─────────────────────────── */
    bool isOperator(int fd) const;
    /* 対象が Member でなければ false を返し、追加しない。
       Member であっても既に Operator なら、集合は変化しないので false */
    bool addOperator(int fd);
    /* 戻り値は集合が実際に変更されたかを表す */
    bool removeOperator(int fd);

    /* ── Invite ───────────────────────────── */
    bool isInvited(int fd) const;
    /* 戻り値は集合が実際に変更されたかを表す。既に Invite 済みなら
       false を返すが、これは失敗ではない。設計書 04 §13 は
       「同じ Client を再度 Invite した場合も成功扱いとし、通知を再送する」
       としているため、INVITE Handler はこの戻り値を失敗として扱わないこと */
    bool addInvite(int fd);
    /* 戻り値は集合が実際に変更されたかを表す */
    bool removeInvite(int fd);

    /* ── Mode i ───────────────────────────── */
    bool isInviteOnly() const;
    void setInviteOnly(bool enabled);

    /* ── Mode t ───────────────────────────── */
    bool isTopicRestricted() const;
    void setTopicRestricted(bool enabled);

    /* ── Mode k ───────────────────────────── */
    bool               hasKey() const;
    const std::string &getChannelKey() const;
    /* Key の形式検証 (1〜23 文字など) は MODE Handler の責務 */
    void               setChannelKey(const std::string &key);
    void               clearChannelKey();
    /* Key 未設定なら常に true。設定済みなら完全一致で比較する */
    bool               matchesKey(const std::string &key) const;

    /* ── Mode l ───────────────────────────── */
    bool        hasUserLimit() const;
    std::size_t getUserLimit() const;
    /* Limit の妥当性検証 (0 不可、上限値など) は MODE Handler の責務 */
    void        setUserLimit(std::size_t limit);
    void        clearUserLimit();
    bool        isFull() const;

    /* ── Mode 文字列 ──────────────────────── */

    /* 有効な Mode を itkl 順に連結し、先頭へ '+' を付けて返す。
       例: "+itkl"。Mode が 1 つも無ければ "+"。
       o は Member ごとの権限であり Channel 全体の状態ではないため
       含めない (設計書 02 §6.8, 05 §4) */
    std::string              buildModeString() const;
    /* buildModeString() の itkl 順に対応する Parameter。key, limit の順 */
    std::vector<std::string> buildModeParameters() const;

private:
    std::string   _name;
    std::string   _keyName;
    std::string   _topic;
    std::set<int> _members;
    std::set<int> _operators;
    std::set<int> _invited;
    bool          _inviteOnly;
    bool          _topicRestricted;
    bool          _keyEnabled;
    std::string   _channelKey;
    bool          _limitEnabled;
    std::size_t   _userLimit;
};
