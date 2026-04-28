#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(void)
{
    int server_fd;
    int client_fd;
    struct sockaddr_in addr;
    char buffer[1024];

    /*
        socket()
        OS内に IPv4 / TCP 用の socket object を作る。
        まだ IP / port は未定。
    */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        return (1);
    }

    /*
        addr
        bind() に渡すための「住所メモ」。
        この socket をどの IP / port に置くかを指定する。
    */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    /*
        bind()
        server_fd の socket object に local endpoint を設定する。
        つまり 0.0.0.0:8080 / TCP で待てるようにする。
    */
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        return (1);
    }

    /*
        listen()
        bind済み socket を「接続待ち受付」にする。
    */
    if (listen(server_fd, 10) == -1)
    {
        perror("listen");
        close(server_fd);
        return (1);
    }

    printf("server listening on port 8080...\n");

    /*
        accept()
        接続してきた client を1人受け取る。
        戻り値 client_fd は、その client と会話するための fd。
    */
    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1)
    {
        perror("accept");
        close(server_fd);
        return (1);
    }

    /*
        recv()
        client_fd から送られてきたデータを buffer に入れる。
    */
    int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n == -1)
    {
        perror("recv");
        close(client_fd);
        close(server_fd);
        return (1);
    }

    buffer[n] = '\0';
    printf("received: %s\n", buffer);

    close(client_fd);
    close(server_fd);
    return (0);
}