*This project has been created as part of the 42 curriculum by Hen00af.*

## Description

**ft_irc** is an IRC (Internet Relay Chat) server implemented in C++ 98.
It implements the core IRC protocol (RFC 1459) and supports multiple simultaneous
clients using a single `poll()`-based non-blocking I/O loop.

### Features

- Multi-client support without forking — all handled in one `poll()` loop
- Non-blocking TCP/IP sockets (IPv4)
- Full registration flow: `PASS` → `NICK` → `USER`
- Channel management with operators and regular users
- Partial-data handling: messages spanning multiple TCP packets are buffered correctly

### Supported Commands

| Command   | Description |
|-----------|-------------|
| `PASS`    | Authenticate with server password |
| `NICK`    | Set / change nickname |
| `USER`    | Set username and real name |
| `JOIN`    | Join one or more channels |
| `PART`    | Leave a channel |
| `PRIVMSG` | Send a message to a channel or user |
| `NOTICE`  | Send a notice (no error replies) |
| `KICK`    | Eject a user from a channel (operator only) |
| `INVITE`  | Invite a user to a channel (operator only) |
| `TOPIC`   | View or set the channel topic |
| `MODE`    | Change channel modes (see below) |
| `WHO`     | List channel members |
| `PING`    | Keepalive (responds with `PONG`) |
| `QUIT`    | Disconnect from the server |

### Channel Modes (`MODE`)

| Flag | Description |
|------|-------------|
| `+i` | Invite-only channel |
| `+t` | Only operators can change the topic |
| `+k` | Channel key (password) |
| `+o` | Grant / revoke operator privilege |
| `+l` | Set maximum user limit |

## How to Run

```bash
cd prd
make
./ircserv <port> <password>
```

**Example:**
```bash
./ircserv 6667 mypassword
```

Connect with any IRC client (e.g. [irssi](https://irssi.org/) or [WeeChat](https://weechat.org/)):
```bash
irssi -c 127.0.0.1 -p 6667 -w mypassword
```

Or with `nc` as a quick sanity check:
```bash
nc -C 127.0.0.1 6667
PASS mypassword
NICK Alice
USER alice 0 * :Alice Smith
JOIN #test
PRIVMSG #test :Hello!
```

## Repository Layout

```
ft_irc/
├── prd/                   Production IRC server (C++ 98)
│   ├── Makefile
│   ├── main.cpp
│   ├── interface/
│   │   ├── Server.hpp     poll()-based server class
│   │   └── Server.cpp     Command parsing, channel/client management
│   └── domain/
│       ├── Client.hpp/cpp Client state (registration, buffers)
│       └── Channel.hpp/cpp Channel state (modes, members)
└── lab/
    ├── C/                 Reference: poll-based echo server (C)
    └── C++/               Reference: IRC message parser experiment (C++98)
```

## References

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — IRC Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Modern IRC specification](https://modern.ircdocs.horse/)
