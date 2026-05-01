tcp接続について何もわからない状態からスタート

どうやら
socket -> bind -> listen -> accept -> recv -> send
この順番に調べればいいらしい

# main flow
1. TCPサーバーを起動
2. クライアント接続を受け取る
3. 接続からHPPTリクエストを読む
4. HTTPレスポンスを書き換えす

```mermaid
[A] TCP接続 -> [B] HTTPとして読む -> [C] HTTPとして返す
```

これを行う。

## 全体像

```go
listener, err := net.Lesten("tcp", "localhost")
```

ここでサーバーの入り口を作成する。
内部的には通信に必要な情報を格納するための箱のようなものが作られるので、
そのオブジェクトに対してbind()などで情報を付加していく。

実際に作られるのはこれに近いオブジェクト

```sh
int fd = socket(AF_INET, SOCK_STREAM, 0);
# AF_INET　が”Address Family internet”, つまりipv4を表し、
# sock_streamが”socket stream”、つまり双方向通信(TCP)を表す
# 第3引数はProtocolを表し、TCP/UDPなどのProtcolをOSに伝える。0はOSに解釈させる。
# IPPROTO_TCP (Internet Protocol Protocol TCP)でも可能.
```

||
    
```sh
kernel socket object
  - family: AF_INET
  - type: SOCK_STREAM
  - protocol: TCP
  - local IP: 未設定
  - local port: 未設定
  - remote IP: 未設定
  - remote port: 未設定
  - state: created
  - receive buffer
  - send buffer
  - options
  ```

#### bind

'''sh

'''

#### accept

```go
#include <sys/socket.h>
int accept ( Socket,  Address,  AddressLength)

// if error is occared, return -1
```

ここでクライアント一人分の通信路を受け取る


```go
request, err :-  http.ReadRequest(bufio.NewReader(conn))
```

ここでTCPのバイト列をHTTPリクエストとして読む。

```go
response.Write(conn)
```

ここでHTTPレスポンスをTCP接続に書き換えす。

この４つが骨格

## 詳細説明

#### net.listen

```Go
listener, err := net.Listen("tcp", "localhost:8080")
```

これは
'''md
localhost 6060で tcp　接続を待ち受けるサーバー窓口を作る
'''
という意味。
ここで作られる listener は、クライアントと直接通信するものではない。

イメージはこう。

```
listener =  底の入り口・受付
conn     =   実際に来た客との会話用回線
```

listener はずっと待つ係。

```
listener.Accept()
```
これで実際に接続したクライアント用のconnを作成する。

#### Accept

```

```