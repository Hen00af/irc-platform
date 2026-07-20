# チャンネル系 Command Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** JOIN/PART/PRIVMSG/KICK/INVITE/TOPIC/QUIT と所属関係・broadcast・切断予約を実装する。

**Architecture:** ServerRelations.cpp に関係管理と切断を、handler/ServerChannelCommands.cpp と handler/ServerMessageCommands.cpp にハンドラを実装。実装者は各タスクで **spec と設計書該当節を必ず読み**、Numeric 本文・処理順序をそこから取ること。

**Tech Stack:** C++98、自前 TestRunner、make。

**Spec (必読):** `docs/superpowers/specs/2026-07-20-channel-commands-design.md`
**設計書 (必読):** `docs/markdown/04_command_detailed_design_ja.md` §10〜§16, §19〜§20、`docs/markdown/02_class_detailed_design_ja.md` §4.7/§4.9、`docs/markdown/03_network_buffer_detailed_design_ja.md` §17〜§18、`docs/markdown/06_error_reply_detailed_design_ja.md` §8

## Global Constraints

- C++98 のみ。`-Wall -Wextra -Werror -std=c++98` で警告ゼロ
- 送信行は必ず `Reply::numeric()` / `Reply::command()` 経由。生の文字列連結でプロトコル行を作らない
- Numeric 定数は namespace `Numeric`。本文は設計書06 §8 と完全一致
- Map への Client/Channel 挿入は `insert(std::make_pair(...))`(`operator[]` 禁止。`_nickToFd`/`_disconnectReasons` など値が int/string の Map は可)
- Channel 生成時は `Channel(表示名, IrcUtil::normalizeChannelName(表示名))`、Map の Key は正規化名
- 通知の Channel 名は表示名(`Channel::getName()`)を使う
- 状態変更前に失敗した場合、部分的な変更を残さない(設計書04 §4)
- 複数 Channel / 複数 target の 1 要素の失敗で他要素を中止しない
- コメントは既存スタイル(設計書 §番号引用の日本語ブロックコメント)
- テストは `tests/handler/channel_cmd/test_channel_cmd.cpp`(suite "channel_cmd"、`runChannelCmdTests()`)に追記していく。ヘルパは auth テストと同形の `dispatchLine` / `takeOutput` に加え、`registerUser(server, fd, nick, host)`(addClient + PASS/NICK/USER + 出力読み捨て)を置く
- 各タスクとも: テストを先に書く → `make -C tests test` で RED 確認 → 実装 → GREEN 確認 → コミット(末尾 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`)

## 共通の期待文字列例 (alice=fd3/127.0.0.1, bob=fd4/10.0.0.1, username "u")

```
:alice!u@127.0.0.1 JOIN :#general
:ircserv.local 331 alice #general :No topic is set
:ircserv.local 353 alice = #general :@alice
:ircserv.local 366 alice #general :End of NAMES list
:ircserv.local 353 bob = #general :@alice bob     (bob 参加時、FD 昇順)
:alice!u@127.0.0.1 PART #general :alice           (reason 省略時は Nickname)
:alice!u@127.0.0.1 PRIVMSG #general :hello
:alice!u@127.0.0.1 PRIVMSG bob :hi
:alice!u@127.0.0.1 KICK #general bob :spam
:ircserv.local 341 alice #general bob
:alice!u@127.0.0.1 INVITE bob :#general
:alice!u@127.0.0.1 TOPIC #general :new topic
:alice!u@127.0.0.1 QUIT :bye
```

(実際の queue 内容は各行 + `\r\n`。エラー Numeric は auth テストと同形式
`:ircserv.local <code> <nick> <params>`)

---

### Task 1: 関係管理基盤 + JOIN + PART

**Files:**
- Modify: `prd/interface/Server.hpp`(spec の「Server クラスへの追加」のうち joinChannel / leaveChannel / deleteChannelIfEmpty / broadcastToChannel / requireChannelMember / requireChannelOperator を宣言)
- Modify: `prd/interface/ServerRelations.cpp`(上記の実装)
- Create: `prd/handler/ServerChannelCommands.cpp`(handleJoin / handlePart)
- Modify: `prd/interface/ServerDispatch.cpp`(handleJoin / handlePart スタブ削除)
- Create: `tests/handler/channel_cmd/test_channel_cmd.cpp`
- Modify: `tests/test_main.cpp` / `tests/Makefile`(suite "channel_cmd"、`test-channel-cmd` ターゲット、TARGET_SRCS へ ServerChannelCommands.cpp、vpath へ handler/channel_cmd)
- Modify: `prd/Makefile`(SRCS へ handler/ServerChannelCommands.cpp)

**Interfaces (Produces):**
```cpp
bool Server::joinChannel(int clientFd, const std::string &channelKey);
bool Server::leaveChannel(int clientFd, const std::string &channelKey,
                          const std::string &reason, bool sendPart);
void Server::deleteChannelIfEmpty(const std::string &channelKey);
void Server::broadcastToChannel(const Channel &channel,
                                const std::string &message, int excludeFd);
bool Server::requireChannelMember(int fd, const Channel &channel);
bool Server::requireChannelOperator(int fd, const Channel &channel);
void Server::handleJoin(int fd, const Message &message);
void Server::handlePart(int fd, const Message &message);
```

- [ ] **Step 1: テストを書く** — spec のテスト計画 JOIN / PART 行の全ケース。JOIN 成功時は上の期待文字列どおり 4 行バーストを完全一致で検証。+i/+k/+l は `server.findChannel("#c")->setInviteOnly(true)` 等で直接設定(+i は `addInvite` が必要だが private のため、Operator の INVITE 実装前は `findChannel()->addInvite(fd)` を Channel 公開 API で直接呼ぶ)
- [ ] **Step 2: RED 確認** — `make -C tests test` がビルドエラー
- [ ] **Step 3: 実装** — spec「Server クラスへの追加」「JOIN」「PART」節と設計書04 §10/§15・02 §4.7 の処理順序どおり
- [ ] **Step 4: GREEN 確認** — `make -C tests test` 全 PASS、`make -C prd re` 警告ゼロ
- [ ] **Step 5: コミット** — `feat(handler): JOIN/PART と Channel 所属関係を実装`

### Task 2: PRIVMSG

**Files:**
- Create: `prd/handler/ServerMessageCommands.cpp`(handlePrivmsg)
- Modify: `prd/interface/ServerDispatch.cpp`(handlePrivmsg スタブ削除)
- Modify: `tests/handler/channel_cmd/test_channel_cmd.cpp`(PRIVMSG ケース追記)
- Modify: `tests/Makefile` / `prd/Makefile`(ServerMessageCommands.cpp 追加)

**Interfaces (Produces):** `void Server::handlePrivmsg(int fd, const Message &message);`

- [ ] **Step 1: テストを書く** — spec テスト計画 PRIVMSG 行の全ケース(411/412 空 text 含む/401/403/404/Channel 配送は送信者除外/Direct は対象のみ/自分宛 1 回/comma 区切り複数 target で一部失敗しても他へ配送)
- [ ] **Step 2: RED 確認**
- [ ] **Step 3: 実装** — spec「PRIVMSG」節と設計書04 §11 どおり
- [ ] **Step 4: GREEN 確認** — 全 PASS + prd ビルド
- [ ] **Step 5: コミット** — `feat(handler): PRIVMSG を実装`

### Task 3: KICK + INVITE + TOPIC

**Files:**
- Modify: `prd/handler/ServerChannelCommands.cpp`(handleKick / handleInvite / handleTopic 追記)
- Modify: `prd/interface/ServerDispatch.cpp`(3 スタブ削除)
- Modify: `tests/handler/channel_cmd/test_channel_cmd.cpp`(追記)

**Interfaces (Produces):** `handleKick` / `handleInvite` / `handleTopic`

- [ ] **Step 1: テストを書く** — spec テスト計画の KICK / INVITE / TOPIC 行の全ケース。検証順序(403→442→482→401→441 など)がエラー表どおりであることを、条件を重ねたケースで確認(例: 非 Member かつ対象不在 → 442 が先)
- [ ] **Step 2: RED 確認**
- [ ] **Step 3: 実装** — spec 各節と設計書04 §12/§13/§14 の処理順序どおり。KICK の Channel からの削除は leaveChannel(sendPart=false) を再利用
- [ ] **Step 4: GREEN 確認** — 全 PASS + prd ビルド
- [ ] **Step 5: コミット** — `feat(handler): KICK/INVITE/TOPIC を実装`

### Task 4: QUIT + 切断予約

**Files:**
- Modify: `prd/interface/Server.hpp`(processPendingDisconnects(public) / scheduleDisconnect / disconnectClient / removeClientFromAllChannels / `_pendingDisconnects` / `_disconnectReasons`)
- Modify: `prd/interface/ServerRelations.cpp`(実装)
- Modify: `prd/handler/ServerChannelCommands.cpp`(handleQuit 追記)
- Modify: `prd/interface/ServerDispatch.cpp`(handleQuit スタブ削除。スタブ節コメントを「残るスタブは handleMode のみ」へ更新)
- Modify: `tests/handler/channel_cmd/test_channel_cmd.cpp`(QUIT ケース追記)

**Interfaces (Produces):**
```cpp
void Server::processPendingDisconnects();  /* public */
void Server::scheduleDisconnect(int fd);
void Server::disconnectClient(int fd, const std::string &reason);
void Server::removeClientFromAllChannels(int clientFd,
                                         const std::string &quitMessage);
void Server::handleQuit(int fd, const Message &message);
```

- [ ] **Step 1: テストを書く** — spec テスト計画 QUIT 行の全ケース(予約時点で Client 残存 → process 後に通知/削除/索引解放/空 Channel 削除/複数共有でも通知 1 回/message 省略 `Client Quit`)
- [ ] **Step 2: RED 確認**
- [ ] **Step 3: 実装** — spec「QUIT」節と設計書03 §17〜§18 の順序どおり(pollfd 削除・close は行わない。ヘッダコメントにネットワーク層で追加する旨を明記)
- [ ] **Step 4: GREEN 確認** — 全 PASS + prd ビルド
- [ ] **Step 5: コミット** — `feat(handler): QUIT と切断予約・遅延削除を実装`
