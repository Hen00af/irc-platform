```mermaid
---
config:
  layout: fixed
---
flowchart TB

    subgraph INIT["1. Webserv 初期化"]
        direction TB

        A["Webserv 起動"]
        B["Configファイルを読み込む"]
        C{"Configは正しいか？"}
        D["エラーを表示して終了"]
        E["listenするポートごとに<br/>Socketを作成"]
        F["bind"]
        G["listen"]
        H["イベント監視へ登録"]

        A --> B
        B --> C
        C -- No --> D
        C -- Yes --> E
        E --> F
        F --> G
        G --> H
    end

    subgraph EVENT_LOOP["2. イベントループ"]
        direction TB

        I["イベントを待機"]
        J{"発生したイベントの種類"}

        K["accept"]
        L["Client Connectionを作成"]
        M["Client Socketを<br/>イベント監視へ登録"]

        N["recvでデータを受信"]
        O["受信Bufferへ追加"]

        P["send可能な分だけ送信"]
        Q{"Responseを<br/>すべて送信したか？"}

        R["Socketをclose"]

        I --> J

        J -- "Listen Socket<br/>新規接続" --> K
        K --> L
        L --> M
        M --> I

        J -- "Client Socket<br/>Read可能" --> N
        N --> O

        J -- "Client Socket<br/>Write可能" --> P
        P --> Q
        Q -- No --> I

        J -- "切断 / Error" --> R
        R --> I
    end

    subgraph REQUEST["3. HTTP Request 解析"]
        direction TB

        S["Request Parserへ渡す"]
        T{"HTTP Requestを<br/>最後まで受信したか？"}
        U["Request Objectを生成"]
        V["ConfigとURLを照合"]
        W["適用するLocationを決定"]
        X{"Methodは<br/>許可されているか？"}
        Y["405 Method Not Allowed"]
        Z["Handlerを選択"]

        S --> T
        T -- Yes --> U
        U --> V
        V --> W
        W --> X
        X -- No --> Y
        X -- Yes --> Z
    end

    subgraph HANDLER["4. Request Handler"]
        direction TB

        AA{"処理の種類"}
        AB["静的ファイルを読み取る"]
        AC["Bodyを処理・保存する"]
        AD["対象ファイルを削除する"]
        AE["CGIを実行する"]

        Z --> AA
        AA -- GET --> AB
        AA -- "POST / PUT" --> AC
        AA -- DELETE --> AD
        AA -- CGI --> AE
    end

    subgraph RESPONSE["5. HTTP Response 生成"]
        direction TB

        AF["Response Objectを生成"]
        AG["Status Line・Header・Bodyを<br/>組み立てる"]
        AH["HTTP Responseとして<br/>シリアライズ"]
        AI["送信Bufferへ追加"]
        AJ["Writeイベントを監視"]
        AK{"Connectionを<br/>維持するか？"}
        AL["Request用の状態を初期化"]

        AF --> AG
        AG --> AH
        AH --> AI
        AI --> AJ
    end

    H --> I

    O --> S
    T -- No --> I

    AB --> AF
    AC --> AF
    AD --> AF
    AE --> AF
    Y --> AF

    AJ --> I

    Q -- Yes --> AK
    AK -- Yes --> AL
    AL --> I
    AK -- No --> R
```



01_initialization.md
    イベントループ開始
            ↓
02_event_loop.md
    recv → 受信Bufferへ追加
            ↓
03_http_request_response.md
    Responseを送信Bufferへ追加
            ↓
02_event_loop.md
    Write可能イベント → send