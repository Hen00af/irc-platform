server:
socket()
→ bind()
→ listen()
→ accept()
→ select()
→ recv()
→ 処理
→ send()
→ close()

client:
socket()
→ gethostbyname()
→ connect()
→ send()
→ recv()
→ close()

簡単なHTTP通信

ircはこの中に状態管理を伴う。
pdfに記載されている、select(), kqueue(), or
epoll()
これらを使用しての状態管理。

もっと言えば、それ以外はただTCP接続するのみに留まりそう。

