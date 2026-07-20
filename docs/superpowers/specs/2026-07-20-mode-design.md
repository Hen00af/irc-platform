# MODE Command 設計 (2026-07-20)

## 目的

設計書05 の Channel MODE Command と Mandatory Mode i/t/k/o/l を実装する。
これで最後のスタブ handleMode が埋まり、MODE 以外は既に動作しているため、
ネットワーク層(サブプロジェクト④)を残すのみとなる。

前提: チャンネル系(feature/channel-commands)がマージ済み。JOIN/TOPIC の
+i/+k/+l/+t 参照はすでに Channel の該当 API を呼んでおり、本サブプロジェクトは
その状態を MODE で変更できるようにする。

## スコープ

- handleMode(スタブ置換。これで全 14 ハンドラが実装済みになる)
- Mode 照会(324)と Mode 変更(i/t/k/o/l、複数・符号切替・一部失敗・
  未知 Mode・実変更なしの集約)
- 単体テスト(suite "mode")

スコープ外: User Mode、Mandatory 外 Mode、ネットワーク層。

## ファイル構成 (設計書02 §12)

```
prd/handler/ServerMode.cpp        新規: handleMode 一式
prd/interface/ServerDispatch.cpp  handleMode スタブ削除(スタブ0個に)
tests/handler/mode/test_mode.cpp  新規: suite "mode"
tests/test_main.cpp / tests/Makefile / prd/Makefile  組み込み
```

MODE は他ハンドラと違い引数消費・符号解析という固有ロジックを持つため、
専用ファイル `ServerMode.cpp` に置く(設計書02 §12 の handler/ServerMode.cpp)。

## Handler 仕様 (設計書05)

Numeric 本文は設計書06 §8 と完全一致。送信は `Reply::numeric()` /
`Reply::command()` のみ。

### 分岐 (設計書05 §3)

1. `params.empty()` → 461 `MODE :Not enough parameters`
2. target(`params[0]`)が `#` で始まらない → 421
   `<target> :Unknown command`(設計書05 §2。User Mode は対象外)
3. `findChannel(params[0])` が NULL → 403 `<channel> :No such channel`
4. `params.size() == 1` → Mode 照会
5. `params.size() >= 2` → Mode 変更

### Mode 照会 (設計書05 §4)

```
:ircserv.local 324 <nick> <channel> +<modes> [parameters]
```

- `Channel::buildModeString()`(itkl 順、有効な文字だけ、無ければ `+`)と
  `Channel::buildModeParameters()`(key, limit の順)を使う
- 324 の parameters 部分は「`+<modes>` の後に半角空白区切りで
  buildModeParameters を連結」。パラメータが無ければ modes だけ
- Member でなくても照会可(設計書05 §2)。Key も Operator 以外へ返す
  (設計書05 §4)

### Mode 変更 — 権限検証 (設計書05 §15。この順序で、失敗時は一切解析しない)

1. Channel 存在(照会分岐で確認済み)
2. `requireChannelMember(fd, *channel)` → 非 Member なら 442 を送って終了
3. `requireChannelOperator(fd, *channel)` → 非 Operator なら 482 を送って終了

### Mode 文字列の解析 (設計書05 §6〜§8)

- modeString = `params[1]`。Mode Parameter は `params[2]` 以降を順に消費
  (argIndex は 2 から開始)
- 状態: `adding`(初期 true)、`signSeen`(初期 false)、`argIndex`
- 集約用: `changedModes`(符号込みの成功 Mode 列)、`changedParams`
  (成功した Mode の Parameter)、`lastSign`(直近に出力した符号)
- 走査:
  - `'+'` → adding=true, signSeen=true, continue
  - `'-'` → adding=false, signSeen=true, continue
  - signSeen が false → その文字へ 472
    `<char> :is unknown mode char to me for <channel>`、変更せず continue
    (引数は消費しない)
  - i/t/k/o/l → 各 Mode 処理(下記)
  - それ以外 → 472(引数消費しない)
- 集約規則(設計書05 §8): 実変更があった Mode だけ changedModes へ追加。
  直前の出力符号と異なる符号のときだけ `+`/`-` を出力してから文字を足す。
  Parameter を伴う Mode は changedParams へ push。実変更が無い Mode
  (既に同じ状態)は Error も通知も無し
- 全走査後、changedModes が空(実変更 0 件)なら MODE 通知を送らない。
  1 件以上なら全 Member(実行者含む)へ
  `:<clientPrefix> MODE <channel> <changedModes> <changedParams...>`

### 各 Mode 処理

Parameter 消費規則(設計書05 §7):

| Mode | +時 | -時 |
|---|---|---|
| i | 引数不要 | 引数不要 |
| t | 引数不要 | 引数不要 |
| k | 引数必須 | 引数不要 |
| o | 引数必須 | 引数必須 |
| l | 引数必須 | 引数不要 |

引数必須で不足 → 461 `MODE :Not enough parameters`、その Mode は変更せず、
引数は消費せず、後続 Mode の解析は継続(設計書05 §7)。

- **i**: `channel->isInviteOnly() != adding` なら `setInviteOnly(adding)` で
  実変更。changedModes へ `i`。引数消費なし(設計書05 §9)
- **t**: `channel->isTopicRestricted() != adding` なら
  `setTopicRestricted(adding)`。changedModes へ `t`。引数消費なし(§10)
- **k**:
  - `+k`: 引数必須。IrcUtil で 1〜23 文字かつ空白/NUL/CR/LF/TAB を含まない
    検証(下記「k の検証」)。不正なら **461 とはしない** —
    設計書05 §11 は形式のみ規定し Numeric 未指定のため、本 spec では
    「不正な key は 461 `MODE :Not enough parameters` を送り、その Mode は
    変更しない」と決定する。有効なら `setChannelKey(key)`(常に実変更扱い、
    既存 key の置換も可、467 不使用)、changedModes へ `k`、changedParams
    へ key、引数消費
  - `-k`: 引数不要(付いていても消費しない)。`hasKey()` が true のとき
    `clearChannelKey()` で実変更、changedModes へ `k`。key 未設定なら
    実変更なし(§11)
- **o**: `+o`/`-o` とも引数必須(§12)。
  1. 引数不足 → 461、継続
  2. `findClientByNickname(arg)` が NULL → 401 `<nick> :No such nick/channel`、
     引数は消費する、その Mode は変更しない
  3. 対象が非 Member(`channel->hasMember(target->getFd())` が false)→
     441 `<nick> <channel> :They aren't on that channel`、引数消費、変更なし
  4. `+o`: 既に Operator なら実変更なし。else `addMember` 済み前提で
     `addOperator(fd)`、実変更
  5. `-o`: Operator でなければ実変更なし。else `removeOperator(fd)`、実変更
  6. 実変更時 changedModes へ `o`、changedParams へ対象 Nickname
  (最後の Operator を -o しても可、自動移譲なし。§12)
- **l**:
  - `+l`: 引数必須。`IrcUtil::parsePositiveSize(arg, limit)` 成功かつ
    `limit <= MODE_MAX_USER_LIMIT`(100000)なら `setUserLimit(limit)`、
    実変更、changedModes へ `l`、changedParams へ arg(元の文字列)、
    引数消費。失敗(非数字/0/overflow/上限超過)なら **変更せず次の Mode
    へ進む**。設計書05 §13 は Numeric 未指定のため本 spec では
    「不正 limit は 461 を送らず黙って無視し引数のみ消費」と決定する
  - `-l`: 引数不要。`hasUserLimit()` が true なら `clearUserLimit()`、
    実変更、changedModes へ `l`。未設定なら実変更なし(§13)

`MODE_MAX_USER_LIMIT = 100000` は ServerMode.cpp 内の
`static const std::size_t` とする(設計書05 §13)。

### k の検証

`IrcUtil` に key 検証関数が無いため、ServerMode.cpp 内の static ヘルパ
`isValidChannelKey(const std::string &key)` で判定する:
1〜23 文字、かつ空白(0x20)・NUL・CR・LF・TAB(0x09)を含まない。
(設計書05 §11。IrcUtil を増やさず MODE 固有ロジックとして閉じる)

## テスト計画 (suite "mode")

dispatchLine / takeOutput / registerUser ヘルパ(channel_cmd テストと同形)。
op = 作成者(自動 Operator)、fd3 alice。

| 分類 | ケース |
|---|---|
| 分岐 | params なし 461 / target が `#` 以外 421 / Channel なし 403 |
| 照会 | 新規 `#c` は `+t`(初期値)→ 324 `+t` / +i+k+l 設定後 324 `+itkl <key> <limit>`(itkl 順、o は含まない) |
| 権限 | 非 Member が変更 442 / Member 非 Op が変更 482(どちらも状態不変)|
| i | `+i` 通知 `MODE #c +i`・照会反映 / JOIN に反映(未招待 473→招待後成功)/ `-i` で解除 / 既に +i に +i は通知なし |
| t | `+t`(初期から)実変更なし通知なし / `-t` で解除・TOPIC が非 Op でも可に / 再 `+t` |
| k | `+k secret` 通知・324 反映・JOIN で key 一致要求(不一致 475)/ 置換可 / `-k` 引数なしで解除 / 不正 key(24文字/空白)は 461・状態不変 / key 未設定で `-k` は通知なし |
| o | `+o bob` で bob が Operator(bob が KICK 可になる)/ `-o bob` で解除 / 対象不在 401 / 対象非 Member 441 / 引数不足 461 / 既に Op に +o は通知なし / 最後の Op を -o 可 |
| l | `+l 3` 設定・324 反映・満員 JOIN 471 / `+l 0` は無視(通知なし・状態不変)/ `+l abc` 無視 / `+l 100001` 無視 / `-l` で解除 / 現 Member 数未満の limit も設定可 |
| 複数 | `+it` で両方通知 `MODE #c +it` / `+kol secret bob 10` で 3 引数消費順・通知 `+kol secret bob 10` / `+it-k`(先に +k してから)符号切替通知 `+it-k` / `+io nosuch` は +i 成功 401・通知 `+i` / `+im` は +i 成功・m へ 472・通知 `+i` / 符号なし `it` は各文字 472・通知なし |
| 集約 | 実変更 0 件(既存状態と同一)は MODE 通知を送らない |

## 設計書との差分 (意図的)

1. 不正な `+k` key → 461 `MODE :Not enough parameters`(設計書05 §11 は
   Numeric 未指定)
2. 不正な `+l` limit(非数字/0/範囲外)→ Numeric を送らず黙って無視し
   引数のみ消費(設計書05 §13 は Numeric 未指定)
3. key 検証を IrcUtil でなく ServerMode.cpp 内 static ヘルパで行う
   (IrcUtil を増やさないため)
