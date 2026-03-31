/* ============================================================
 * poll_echo_server.c
 *
 * pollベースのエコーサーバー
 * Webserv / IRC 共通の骨格。
 * 多段パイプの picoshell と同じ感覚で理解できるように書いた。
 *
 * picoshell:  コマンドA → pipe → コマンドB → pipe → コマンドC
 * このサーバー: クライアント → socket → サーバー → socket → クライアント
 *
 * 使い方:
 *   gcc -Wall -Wextra -Werror poll_echo_server.c -o echo_server
 *   ./echo_server
 *   # 別ターミナルで:
 *   telnet localhost 4242
 *   # 文字を打つとそのまま返ってくる
 *   # 複数ターミナルから同時接続しても全部捌ける
 * ============================================================ */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <fcntl.h>
 #include <errno.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <poll.h>
 
 #define PORT        4242
 #define MAX_CLIENTS 128
 #define BUF_SIZE    1024
 
 /* ============================================================
  * 構造体: picoshell の cmds[] に相当
  *
  * picoshell は char **cmds[] でコマンドを管理していた。
  * サーバーでは struct pollfd fds[] でfdを管理する。
  *
  * pollfd の中身:
  *   .fd      → 監視するファイルディスクリプタ
  *   .events  → 何を監視するか (POLLIN=読み取り可能か)
  *   .revents → 実際に起きたこと (pollが埋めてくれる)
  * ============================================================ */
 struct pollfd   fds[MAX_CLIENTS + 1]; /* +1 はlistenソケット用 */
 int             nfds = 0;             /* 現在監視中のfd数 */
 
 /* ============================================================
  * fd を poll 監視リストに追加
  * ============================================================ */
 void add_fd(int fd)
 {
     if (nfds >= MAX_CLIENTS + 1)
     {
         fprintf(stderr, "最大接続数に達しました\n");
         close(fd);
         return;
     }
     fds[nfds].fd = fd;
     fds[nfds].events = POLLIN;  /* 読み取り可能になったら教えて */
     fds[nfds].revents = 0;
     nfds++;
 }
 
 /* ============================================================
  * fd を poll 監視リストから削除
  * ============================================================ */
 void remove_fd(int index)
 {
     close(fds[index].fd);
     printf("クライアント切断: fd=%d\n", fds[index].fd);
     /* 最後の要素で上書きして詰める */
     fds[index] = fds[nfds - 1];
     nfds--;
 }
 
 /* ============================================================
  * フェーズ1: listen ソケットの準備
  *
  * picoshell でいう pipe() に相当。
  * pipe() は2つのfdを作って繋げたが、
  * socket+bind+listen は「外部から接続できるfd」を1つ作る。
  * ============================================================ */
 int setup_server(void)
 {
     int server_fd;
     struct sockaddr_in addr;
     int opt = 1;
 
     /* socket(): fdを作る */
     server_fd = socket(AF_INET, SOCK_STREAM, 0);
     if (server_fd == -1)
     {
         perror("socket");
         exit(1);
     }
 
     /* SO_REUSEADDR: サーバー再起動時に "Address already in use" を防ぐ */
     setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
     /* ノンブロッキングに設定
      * picoshell では不要だった（子プロセスが勝手に処理するから）。
      * サーバーでは1つのプロセスで全部やるので、
      * read/accept がブロックすると他のクライアントが止まる。 */
     fcntl(server_fd, F_SETFL, O_NONBLOCK);
 
     /* bind(): このポートを使うとOSに宣言 */
     memset(&addr, 0, sizeof(addr));
     addr.sin_family = AF_INET;
     addr.sin_addr.s_addr = INADDR_ANY;  /* 全インターフェースで受付 */
     addr.sin_port = htons(PORT);        /* ネットワークバイトオーダーに変換 */
 
     if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
     {
         perror("bind");
         exit(1);
     }
 
     /* listen(): 接続受付を開始 */
     if (listen(server_fd, SOMAXCONN) == -1)
     {
         perror("listen");
         exit(1);
     }
 
     printf("サーバー起動: port %d\n", PORT);
     return server_fd;
 }
 
 /* ============================================================
  * フェーズ2-A: 新しいクライアントを受け入れる
  *
  * picoshell でいう fork() に近い概念。
  * fork() は新しいプロセスを作ったが、
  * accept() は新しい「接続（fd）」を作る。
  * ============================================================ */
 void handle_new_connection(int server_fd)
 {
     int client_fd;
 
     client_fd = accept(server_fd, NULL, NULL);
     if (client_fd == -1)
     {
         if (errno != EAGAIN && errno != EWOULDBLOCK)
             perror("accept");
         return;
     }
 
     /* クライアントもノンブロッキングに */
     fcntl(client_fd, F_SETFL, O_NONBLOCK);
     add_fd(client_fd);
     printf("新しい接続: fd=%d (現在 %d クライアント)\n",
            client_fd, nfds - 1); /* -1 はlistenソケット分 */
 }
 
 /* ============================================================
  * フェーズ2-B: クライアントからのデータを処理
  *
  * picoshell では pipe 経由で read/write していたのと同じ。
  * ここでは recv/send を使う（ソケット用の read/write）。
  *
  * Webserv ならここで HTTP リクエストをパースする。
  * IRC ならここで JOIN, PRIVMSG 等のコマンドをパースする。
  * 今はエコー（そのまま返す）だけ。
  * ============================================================ */
 void handle_client_data(int index)
 {
     char buf[BUF_SIZE];
     ssize_t n;
 
     n = recv(fds[index].fd, buf, sizeof(buf) - 1, 0);
 
     if (n <= 0)
     {
         /* n == 0: クライアントが正常に切断
          * n <  0: エラー */
         remove_fd(index);
         return;
     }
 
     buf[n] = '\0';
     printf("fd=%d から受信 (%zd bytes): %s", fds[index].fd, n, buf);
 
     /* エコー: 受け取ったデータをそのまま返す */
     send(fds[index].fd, buf, n, 0);
 }
 
 /* ============================================================
  * メインループ
  *
  * picoshell との対比:
  *
  * picoshell:
  *   for (i = 0; cmds[i]; i++)    ← コマンドを1つずつ順番に処理
  *     fork → execvp
  *
  * サーバー:
  *   while (1)                     ← 永遠にループ
  *     poll(fds)                   ← 全fdを一括監視
  *     for (i = 0; i < nfds; i++) ← イベントがあったfdだけ処理
  *
  * picoshell は「コマンドの数だけ」ループした。
  * サーバーは「何か起きるたびに」ループする。永遠に。
  * ============================================================ */
 int main(void)
 {
     int server_fd;
     int i;
 
     /* フェーズ1: サーバー準備 */
     server_fd = setup_server();
     add_fd(server_fd);  /* fds[0] = listenソケット */
 
     /* フェーズ2: メインループ */
     while (1)
     {
         /* poll: 何かイベントが起きるまでここで待つ (-1 = タイムアウトなし) */
         if (poll(fds, nfds, -1) == -1)
         {
             perror("poll");
             break;
         }
 
         /* イベントがあったfdを探して処理 */
         for (i = 0; i < nfds; i++)
         {
             if (fds[i].revents == 0)
                 continue;  /* このfdには何も起きていない */
 
             if (fds[i].revents & POLLIN)
             {
                 if (fds[i].fd == server_fd)
                     handle_new_connection(server_fd);  /* 新しい接続 */
                 else
                     handle_client_data(i);             /* データ受信 */
             }
 
             /* クライアントが異常切断した場合 */
             if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
             {
                 if (fds[i].fd != server_fd)
                     remove_fd(i);
             }
         }
     }
 
     close(server_fd);
     return 0;
 }