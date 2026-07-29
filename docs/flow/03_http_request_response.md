# HTTP Request and Response Flow

Clientから受信したデータを解析し、HTTP Responseを生成して送信Bufferへ追加するまでの流れ。

```mermaid
---
config:
  layout: fixed
---
flowchart TB
    A["受信Bufferを<br/>Request Parserへ渡す"]
    B{"HTTP Requestを<br/>最後まで受信したか？"}

    C["次のReadイベントを待つ"]

    D["Request Objectを生成"]
    E["Host Headerなどを検証"]
    F["Server Configを選択"]
    G["URLとConfigを照合"]
    H["適用するLocationを決定"]

    I{"Methodは<br/>許可されているか？"}
    J["405 Method Not Allowed"]

    K{"処理の種類"}

    L["静的ファイルを読み取る"]
    M["Bodyを処理・保存する"]
    N["対象ファイルを削除する"]
    O["CGIを実行する"]
    P["Error Responseを生成"]

    Q["Response Objectを生成"]
    R["Status Lineを生成"]
    S["Headersを生成"]
    T["Bodyを設定"]
    U["HTTP Responseとして<br/>シリアライズ"]
    V["送信Bufferへ追加"]
    W["Writeイベントを監視"]

    A --> B

    B -- No --> C
    B -- Yes --> D

    D --> E
    E --> F
    F --> G
    G --> H
    H --> I

    I -- No --> J
    I -- Yes --> K

    K -- GET --> L
    K -- "POST / PUT" --> M
    K -- DELETE --> N
    K -- CGI --> O
    K -- Error --> P

    L --> Q
    M --> Q
    N --> Q
    O --> Q
    P --> Q
    J --> Q

    Q --> R
    R --> S
    S --> T
    T --> U
    U --> V
    V --> W
```

## 補足

この処理の責務は、ResponseをClientへ直接送信することではない。

HTTP処理側では、Responseをシリアライズして送信Bufferへ追加するところまで進める。
実際の `send()` は、Client socketが書き込み可能になったときにイベントループ側で行う。

これにより、巨大なファイルや通信速度が遅いClientにも対応しやすくなる。
