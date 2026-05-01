1. Makefile
2. main.cpp
3. socket.cpp
4. server.cpp
5. client.cpp
6. request parser
7. cgi.cpp
これらを見る

# webserv で盗むもの

## socket lifecycle
socket -> bind -> listen -> accept -> recv -> send

## event loop
selectでserver_fdとclient_fdを監視する

## client state
fd
read_buffer
write_buffer
request
response
last_active_time

## HTTP parser state
REQUEST_LINE
HEADERS
BODY
DONE

## response build flow
request -> route -> file/cgi/error -> response

## CGI flow
pipe -> fork -> dup2 -> execve -> read stdout