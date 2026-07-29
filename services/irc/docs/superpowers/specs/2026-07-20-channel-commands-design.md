# チャンネル系 Command (JOIN/PART/PRIVMSG/KICK/INVITE/TOPIC/QUIT) 設計 (2026-07-20)

## 目的

設計書04 §10〜§16 のチャンネル系 Handler と、設計書02 §4.7/§4.9 の
所属関係・broadcast、設計書03 §17〜§18 の切断予約(close なし)を実装する。
これで MODE 以外の全 Mandatory Command が動作する。

前提: 登録フロー(feature/registration-flow)がマージ済み。

## スコープ

- Server: joinChannel / leaveChannel / removeClientFromAllChannels /
  deleteChannelIfEmpty(02 §4.7)、broadcastToChannel(02 §4.9)、
  scheduleDisconnect / processPendingDisconnects / disconnectClient
  (03 §17〜§18、pollfd 削除と close は除く)、
  requireChannelMember / requireChannelOperator(04 §4)
- Handler: handleJoin / handlePart / handlePrivmsg / handleKick /
  handleInvite / handleTopic / handleQuit(スタブ置換。残るスタブは
  handleMode のみ)
- 単体テスト(suite "channel_cmd")

スコープ外: MODE(サブプロジェクト③)、pollfd 管理・close(2)・
「切断予約済み FD の追加 Handler 処理停止」(ネットワーク層 = ④)。

## ファイル構成 (設計書02 §12)

```
prd/interface/Server.hpp              宣言追加
prd/interface/ServerRelations.cpp     所属関係・broadcast・切断 追加
prd/handler/ServerChannelCommands.cpp 新規: JOIN/PART/KICK/INVITE/TOPIC/QUIT
prd/handler/ServerMessageCommands.cpp 新規: PRIVMSG
tests/handler/channel_cmd/test_channel_cmd.cpp 新規
```

## Server クラスへの追加

```cpp
public:
    /* ネットワーク層のイベントループ末尾とテストから呼ぶ (03 §17)。
       設計書02 §4.5 では内部メソッドだが、poll ループ実装前にテスト
       から駆動できるよう public とする (本 spec の決定事項) */
    void processPendingDisconnects();

private:
    /* ── 所属関係 (設計書 02 §4.7) ── */
    bool joinChannel(int clientFd, const std::string &channelKey);
    bool leaveChannel(int clientFd, const std::string &channelKey,
                      const std::string &reason, bool sendPart);
    void removeClientFromAllChannels(int clientFd,
                                     const std::string &quitMessage);
    void deleteChannelIfEmpty(const std::string &channelKey);

    /* ── 送信 (設計書 02 §4.9) ── */
    void broadcastToChannel(const Channel &channel,
                            const std::string &message, int excludeFd);

    /* ── 切断 (設計書 03 §17〜§18) ── */
    void scheduleDisconnect(int fd);
    /* pollfd 削除と close(fd) はネットワーク層実装時に追加する */
    void disconnectClient(int fd, const std::string &reason);

    /* ── 共通検証 (設計書 04 §4) ── */
    bool requireChannelMember(int fd, const Channel &channel);
    bool requireChannelOperator(int fd, const Channel &channel);

    std::set<int>              _pendingDisconnects; /* 02 §4.2 */
    std::map<int, std::string> _disconnectReasons;  /* QUIT 理由 (下記) */
```

- `_disconnectReasons`: 設計書04 §16 は「切断理由を保存して切断予約する」、
  設計書03 §17 の `processPendingDisconnects()` は固定文言
  `"Connection closed"` を渡す。両立させるため FD→理由 の Map を持ち、
  `processPendingDisconnects()` は保存済み理由があればそれを、なければ
  `"Connection closed"` を使う(本 spec の決定事項)
- `broadcastToChannel()`: Channel の Member FD を走査して queue。
  `excludeFd == -1` なら除外しない(02 §4.9)
- `leaveChannel()` の順序(02 §4.7): 通知文字列生成 → sendPart なら
  削除前の全 Member へ PART 通知 → Channel から Member/Operator/Invite
  削除 → Client から Channel 名削除 → 空なら削除
- `disconnectClient()` の順序(03 §18、pollfd/close を除く):
  Client 確認 → QUIT 通知生成 → 共有 Member を重複なし収集して queue →
  各 Channel から Member/Operator 削除・全 Channel の Invite から削除 →
  Client の参加集合クリア → 空 Channel 削除 → Nickname 索引削除 →
  Client Map から削除。`_disconnectReasons` の該当エントリも削除する
- `requireChannelMember`: 非 Member なら 442
  `<channel> :You're not on that channel` を queue して false。
  `requireChannelOperator`: 非 Operator なら 482
  `<channel> :You're not channel operator` を queue して false。
  channel 名は `Channel::getName()`(表示名)を使う

## Handler 仕様 (Numeric 本文は設計書06 §8 と完全一致)

### JOIN (設計書04 §10)

`JOIN <channel>{,<channel>} [<key>{,<key>}]` / `JOIN 0`

- params 無し → 461。`params[0] == "0"` なら全 Channel から退出
  (各 Channel へ PART 通知、reason は Nickname)
- channel 一覧 / key 一覧を `IrcUtil::splitCommaList()` で分割し、
  位置対応。key 不足分は空文字
- Channel ごと(1 つの失敗で他を中止しない):
  1. `IrcUtil::isValidChannelName()` 不正 → 403 `<channel> :No such channel`
  2. 参加済み → 何もせず次へ
  3. 未存在なら生成(`insert(std::make_pair(key, Channel(name, key)))`)
  4. 既存 Channel は +i → +k → +l の順に検証:
     Invite なし → 473 `<channel> :Cannot join channel (+i)` /
     Key 不一致 → 475 `<channel> :Cannot join channel (+k)` /
     満員 → 471 `<channel> :Cannot join channel (+l)`
  5. `joinChannel()` で双方向追加。新規 Channel なら Operator 追加
  6. Invite を消費(`removeInvite`)
  7. 全 Member へ `:<prefix> JOIN :<channel>`(表示名)
  8. 参加者へ Topic Reply: 無し → 331 `<channel> :No topic is set`、
     有り → 332 `<channel> :<topic>`
  9. 参加者へ Names: 353 `= <channel> :@op user ...`(FD 昇順、
     Operator に `@`)と 366 `<channel> :End of NAMES list`
- 新規生成に失敗した Channel は Map に残さない(mode 検証は既存
  Channel のみ対象なので、生成→即参加で失敗経路はない)

### PART (設計書04 §15)

`PART <channel>{,<channel>} [:<reason>]`

- params 無し → 461。末尾 param が reason(channel 一覧は params[0])
- Channel ごとに独立処理: 未存在 → 403 / 非 Member → 442
- reason 無しは Nickname。`leaveChannel(fd, key, reason, true)` →
  削除前の全 Member へ `:<prefix> PART <channel> :<reason>`

### PRIVMSG (設計書04 §11)

- target param 無し → 411 `:No recipient given (PRIVMSG)`
- text param 無しまたは空 → 412 `:No text to send`
- target を comma 分割し独立処理
- `#` 始まり: Channel。未存在 → 403 / 送信者非 Member → 404
  `<channel> :Cannot send to channel` / 送信者以外の全 Member へ
  `:<prefix> PRIVMSG <channel> :<text>`(表示名)
- それ以外: Nickname。未存在 → 401 `<nick> :No such nick/channel` /
  対象へ `:<prefix> PRIVMSG <targetNick> :<text>`。自分宛でも 1 回 queue

### KICK (設計書04 §12)

`KICK <channel> <nickname> [:<reason>]` — 1 Channel 1 Client のみ

1. params 2 個未満 → 461
2. Channel 未存在 → 403
3. 実行者非 Member → 442 / 非 Operator → 482
4. 対象 Nickname 未存在 → 401
5. 対象が非 Member → 441 `<nick> <channel> :They aren't on that channel`
6. reason 無しは実行者 Nickname
7. 削除前の全 Member へ `:<operatorPrefix> KICK <channel> <targetNick> :<reason>`
8. `leaveChannel(対象fd, key, reason, false)` 相当で削除(通知は 7 で
   済んでいるため sendPart=false)。空 Channel 削除。Operator 自動移譲なし

### INVITE (設計書04 §13)

`INVITE <nickname> <channel>`

1. params 2 個未満 → 461
2. 対象 Client 未存在 → 401
3. Channel 未存在 → 403
4. 実行者非 Member → 442 / 非 Operator → 482(Mode に関係なく
   Operator 限定。課題文の指定)
5. 対象が既に Member → 443 `<nick> <channel> :is already on channel`
6. `addInvite(対象fd)`(戻り値 false でも成功扱い。再 Invite は通知再送)
7. 実行者へ 341 `<inviterNick> <channel> <targetNick>`
8. 対象へ `:<inviterPrefix> INVITE <targetNick> :<channel>`

### TOPIC (設計書04 §14)

`TOPIC <channel>` (照会) / `TOPIC <channel> :<topic>` (変更)

1. params 無し → 461
2. Channel 未存在 → 403
3. 非 Member → 442
4. topic param 無し → 照会: 無し 331 / 有り 332
5. 変更: `+t` かつ非 Operator → 482。空文字 topic は削除。
   全 Member(自分含む)へ `:<prefix> TOPIC <channel> :<topic>`

### QUIT (設計書04 §16)

`QUIT [:<message>]`

- message 無しは `"Client Quit"`
- `_disconnectReasons[fd] = message` を保存し `scheduleDisconnect(fd)`
- 通知・関係削除は `processPendingDisconnects()` →
  `disconnectClient()` で行う: 共有 Channel の各 Client へ 1 回だけ
  `:<prefix> QUIT :<message>`。送信者自身へは送らない
- 「残り Command 列を処理しない」はネットワーク層の受信ループが
  担当(03 §6。本サブプロジェクトでは対象外)

## テスト計画 (suite "channel_cmd")

dispatchLine ヘルパ(auth テストと同形)で駆動。主なケース:

| 分類 | ケース |
|---|---|
| JOIN | 461 / 403(不正名)/ 新規作成で JOIN+331+353(`@nick`)+366 / 2 人目参加で両者へ JOIN 通知・Names に 2 人(FD 昇順)/ 参加済み再 JOIN は無応答 / 複数 Channel comma / 1 つ失敗しても他は参加 / +i 未招待 473・招待後成功と Invite 消費(再 JOIN 時 473)/ +k 不一致 475・一致成功 / +l 満員 471 / JOIN 0 で全退出(PART 通知) |
| PART | 461 / 403 / 442 / reason 付き通知が全 Member へ / reason 無しは Nickname / 空 Channel 削除(再 JOIN で新規扱い=自分が Operator) |
| PRIVMSG | 411 / 412(text なし・空)/ 401 / 403 / 404(非 Member)/ Channel で送信者以外へ / Direct で対象のみ・自分宛 1 回 |
| KICK | 461 / 403 / 442 / 482(非 Op)/ 401 / 441 / 通知が削除前全員へ・対象の joinedChannels から消える / reason 省略は実行者 Nick |
| INVITE | 461 / 401 / 403 / 442 / 482 / 443 / 341+INVITE 通知 / 再 INVITE でも再送 |
| TOPIC | 461 / 403 / 442 / 照会 331→設定→332 / +t 有効で非 Op は 482・Op は変更可・+t 無効なら非 Op でも変更可 / 変更通知が全 Member へ / 空 topic で削除(331 に戻る) |
| QUIT | 予約時点では Client 残存 / processPendingDisconnects 後: 共有 Member へ QUIT 通知 1 回・自分へは無し・Client/索引/Channel 関係が消える・空 Channel 削除 / message 省略は `Client Quit` / 複数 Channel 共有でも通知 1 回 |

注: +i/+k/+l の状態設定は MODE 未実装のため、テストは `findChannel()` 経由の
`Channel` 公開 API(`setInviteOnly` 等)で直接設定する。

## 設計書との差分 (意図的)

1. `processPendingDisconnects()` を public にする(02 §4.5 では内部。
   poll ループ実装前にテストから駆動するため)
2. `_disconnectReasons` Map を追加(04 §16 と 03 §17 の整合のため)
3. `disconnectClient()` から pollfd 削除と close(2) を除く(ネットワーク
   層実装時に追加。FD の所有はネットワーク層のまま)
