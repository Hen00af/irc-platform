# Webserv Initialization Flow

Webservを起動してから、listen socketをイベント監視へ登録するまでの流れ。

```mermaid
---
config:
  layout: fixed
---
flowchart TB
    A["Webserv 起動"]
    B["Configファイルを読み込む"]
    C{"Configは正しいか？"}
    D["エラーを表示して終了"]

    E["listenするポートを抽出"]
    F["ポートごとにSocketを作成"]
    G["Socket optionを設定"]
    H["bind"]
    I["listen"]
    J["listen socketを<br/>イベント監視へ登録"]
    K["イベントループ開始"]

    A --> B
    B --> C

    C -- No --> D
    C -- Yes --> E

    E --> F
    F --> G
    G --> H
    H --> I
    I --> J
    J --> K
```

## 補足

listen socketは、HTTP Requestを直接処理するsocketではない。

listen socketの役割は、新しいClient Connectionを受け付けること。
Clientから実際にデータを受信するためのsocketは、`accept()` によって別途作成する。
