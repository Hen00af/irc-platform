#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <string>

class Conf;

void    server(Conf &conf);
void    boot_server(Conf &conf);

class Server {
public:
    Server();
    ~Server();

    // サーバーの初期化（socket, bind, listenまで行う）
    void    initServer(int port);
    
    // Getter（後のselect/poll/kqueueで使用）
    int     getListenFd() const;


private:
    int                 _listen_fd;    // ソケットディスクリプタ
    struct sockaddr_in  _address;      // サーバーのアドレス情報
    int                 _port;         // ポート番号
};

#endif