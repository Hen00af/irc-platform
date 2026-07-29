# ft_irc Requirements Extract

Source: ft_irc subject, Version 11.0

This file is a compact requirements extract used as a reference while writing the design documents.

## Mandatory Scope

- Implement an IRC server in C++98.
- Do not implement an IRC client.
- Do not implement server-to-server communication.
- The executable is `ircserv`.
- Run format: `./ircserv <port> <password>`.

## Submission

- `Makefile`
- `*.h`
- `*.hpp`
- `*.cpp`
- `*.tpp`
- `*.ipp`
- Optional configuration file

## Makefile Rules

- `$(NAME)`
- `all`
- `clean`
- `fclean`
- `re`

## Build Constraints

- `c++ -Wall -Wextra -Werror`
- C++98 compatible
- Must compile with `-std=c++98`
- External libraries are forbidden
- Boost is forbidden

## Allowed External Functions

```text
socket, close, setsockopt, getsockname,
getprotobyname, gethostbyname, getaddrinfo,
freeaddrinfo, bind, connect, listen, accept,
htons, htonl, ntohs, ntohl, inet_addr, inet_ntoa,
inet_ntop, send, recv, signal, sigaction,
sigemptyset, sigfillset, sigaddset, sigdelset,
sigismember, lseek, fstat, fcntl, poll
```

`poll()` may be replaced by an equivalent mechanism such as `select()`, `kqueue()`, or `epoll()`.

This design uses `poll()`.

## Networking Requirements

- Multiple clients must be handled simultaneously.
- `fork()` is forbidden.
- All I/O must be non-blocking.
- A single `poll()` or equivalent must handle listen, read, write, and other I/O events.
- The server must not call `read`/`recv` or `write`/`send` on an FD without readiness from `poll()` or equivalent.
- The server must not rely on `errno` after `read`/`recv` or `write`/`send` to decide the next operation.
- Client/server communication must use TCP/IP, IPv4 or IPv6.

## Reference Client

- Choose one real IRC client as the reference client.
- The reference client will be used during evaluation.
- It must connect to the server without errors.
- Using the reference client with this server should be similar to using it with an official IRC server, within the mandatory feature scope.

## Mandatory IRC Features

- Authentication
- Nickname setting
- Username setting
- Join a channel
- Send and receive private messages
- Forward channel messages to every other client in the channel
- Operators and regular users

## Mandatory Operator Commands

- `KICK`: eject a client from a channel
- `INVITE`: invite a client to a channel
- `TOPIC`: change or view the channel topic
- `MODE`: change channel mode

## Mandatory Channel Modes

- `i`: set/remove invite-only channel
- `t`: set/remove TOPIC restriction to channel operators
- `k`: set/remove channel key
- `o`: give/take channel operator privilege
- `l`: set/remove user limit

## MacOS Constraint

On MacOS, `fcntl()` is allowed only as:

```cpp
fcntl(fd, F_SETFL, O_NONBLOCK);
```

Any other flag is forbidden.

## Partial Receive Requirement

The server must handle partial data, low bandwidth, and split packets.

Example:

```text
com
man
d\n
```

must be aggregated and processed as:

```text
command\n
```

## README Requirements

The repository root must include `README.md`.

The README must:

- Be written in English
- Start with the required italicized 42 curriculum sentence
- Include Description
- Include Instructions
- Include Resources
- Describe how AI was used and for which parts of the project

## Bonus

Bonus is out of scope for the current design.

Bonus features:

- File transfer
- Bot

Bonus is evaluated only if the mandatory part is perfect.

