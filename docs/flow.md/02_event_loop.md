# Webserv Event Loop

イベント監視から返されたイベントを種類ごとに振り分ける流れ。

```mermaid
---
config:
  layout: fixed
---
flowchart TB
    A["イベントを待機"]
    B["発生したイベントを順番に処理"]
    C{"イベントの種類"}

    D["accept"]
    E["Client Connectionを作成"]
    F["Client socketを<br/>イベント監視へ登録"]

    G["recvでデータを受信"]
    H{"受信結果"}

    I["受信Bufferへ追加"]
    J["Request Parserへ渡す"]

    K["send可能な分だけ送信"]
    L{"Responseを<br/>すべて送信したか？"}

    M{"Connectionを<br/>維持するか？"}
    N["Request処理用の状態を初期化"]
    O["次のRequestを待つ"]

    P["Socketをclose"]
    Q["Client Connectionを削除"]

    A --> B
    B --> C

    C -- "Listen Socket<br/>新規接続" --> D
    D --> E
    E --> F
    F --> A

    C -- "Client Socket<br/>Read可能" --> G
    G --> H

    H -- "データを受信" --> I
    I --> J

    H -- "切断 / Error" --> P

    C -- "Client Socket<br/>Write可能" --> K
    K --> L

    L -- No --> A
    L -- Yes --> M

    M -- Yes --> N
    N --> O
    O --> A

    M -- No --> P

    C -- "Error / Close" --> P
    P --> Q
    Q --> A
```

## 補足

イベントループでは、次のイベントを分離して扱う。

* listen socketへの新規接続
* Client socketからの読み込み
* Client socketへの書き込み
* 切断やエラー

`recv()` や `send()` を常に最後まで実行できるとは限らない。
non-blocking I/Oでは、処理可能な分だけ進めて、続きは次のイベントで再開する。
