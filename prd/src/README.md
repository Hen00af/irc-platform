# 概要
本プロジェクトはC++を使用したHTTPサーバを作成するプロジェクトのrレポジトリです。
主なフローは以下の通りになります。

    ./conf/nginx.confで記述されている設定のパース

    読み取った情報を使用してのTCP/IPを使用した多重化I/O接続を行う。

    クライアントからとってきたBuffのパース

# ディレクトリ構成

```sh
.
├── conf
│    └── ~~
├── log
|    └── ~~
├── main.cpp
├── Makefile
├── src
│   ├── http
│   │   ├── request.cpp
│   │   └── request.hpp
│   ├── logging
│   │   ├── logging.cpp
│   │   └── logging.hpp
│   ├── persing
│   │   ├── _conf.md
│   │   ├── persing_conf_util.cpp
│   │   ├── persing_conf.cpp
│   │   ├── persing_conf.hpp
│   │   ├── persing_request.cpp
│   │   ├── persing_request.hpp
│   │   └── persing.cpp
│   ├── README.md
│   ├── rooting
│   │   ├── rooting.cpp
│   │   └── rooting.hpp
│   ├── server
│   │   ├── server.cpp
│   │   └── server.hpp
│   └── test
│       ├── _ConfTest.cpp
│       ├── _ConfTest.sh
│       ├── _MonoServer_test.cpp
│       ├── _MonoServer_test.sh
│       └── conf_test
└── www
    ├── error.html
    └── index.html
```