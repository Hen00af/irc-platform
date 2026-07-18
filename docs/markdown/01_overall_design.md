# ft_irc Mandatory Overall Design

## 1. Document Purpose and Scope

This document defines the overall design for the mandatory part of the `ft_irc` project.

The goal is to create a design document that makes implementation straightforward: after this document and the detailed design documents are completed, implementation should mainly be translating the design into C++98 code.

Requirement source:

- ft_irc subject, Version 11.0

### Scope

- IRC server implementation in C++98
- Multiple concurrent IRC client connections
- TCP/IP socket communication
- Non-blocking I/O
- One `poll()` based event loop
- Authentication and registration
- Channel creation and management
- Private and channel messages
- Operator commands required by the subject

### Out of Scope

- IRC client implementation
- Server-to-server communication
- SSL/TLS
- Persistence or database storage
- GUI
- File transfer bonus
- Bot bonus

## 2. Requirements Summary

The program name is:

```text
ircserv
```

Execution format:

```bash
./ircserv <port> <password>
```

Submitted files:

- `Makefile`
- `*.h`
- `*.hpp`
- `*.cpp`
- `*.tpp`
- `*.ipp`
- optional configuration file

Required Makefile rules:

- `$(NAME)`
- `all`
- `clean`
- `fclean`
- `re`

Mandatory behavior:

- The server accepts multiple clients at the same time.
- The server must not use `fork()`.
- All sockets must be non-blocking.
- New connections, reads, and writes are managed through a single `poll()` loop.
- The server communicates over TCP/IP.
- The server must work with a real IRC client.
- The reference IRC client must connect without errors.
- Code must be clean and maintainable.

Build constraints:

- Compile with `c++`
- Use `-Wall -Wextra -Werror`
- Must comply with C++98
- Must still compile when `-std=c++98` is added
- External libraries and Boost are forbidden

Allowed external functions:

```text
socket, close, setsockopt, getsockname,
getprotobyname, gethostbyname, getaddrinfo,
freeaddrinfo, bind, connect, listen, accept,
htons, htonl, ntohs, ntohl, inet_addr, inet_ntoa,
inet_ntop, send, recv, signal, sigaction,
sigemptyset, sigfillset, sigaddset, sigdelset,
sigismember, lseek, fstat, fcntl, poll
```

The subject also permits an equivalent to `poll()`, such as `select()`, `kqueue()`, or `epoll()`. This design uses `poll()`.

MacOS-specific constraint:

```cpp
fcntl(fd, F_SETFL, O_NONBLOCK);
```

On MacOS, `fcntl()` is allowed only in this form for setting non-blocking mode.

Mandatory IRC features:

- Authentication: `PASS`, `NICK`, `USER`
- Channel participation: `JOIN`
- Messaging: private messages and channel messages with `PRIVMSG`
- User roles: normal user and channel operator
- Operator commands: `KICK`, `INVITE`, `TOPIC`, `MODE`
- Channel modes: `i`, `t`, `k`, `o`, `l`

Practical supporting commands:

- `PING`
- `PONG`
- `QUIT`
- `PART`

These supporting commands are not the core mandatory feature set, but they are useful for stable behavior with real IRC clients.

## 3. System Overview

The project implements only the IRC server.

```text
IRC Client
    |
    | TCP/IP socket
    v
ircserv
```

The server receives text-based IRC commands from connected clients, updates internal state, and sends IRC-formatted replies or forwarded messages.

Main server responsibilities:

- Accept TCP client connections
- Track connected clients
- Track existing channels
- Parse IRC commands
- Execute command handlers
- Send replies and broadcasts
- Clean up clients and channels on disconnect

Diagram source:

- `../diagrams/system_context.mmd`

## 4. Architecture Policy

The server uses an event-driven architecture.

The main loop repeatedly calls `poll()` and reacts to events:

- Listen socket is readable: accept new client
- Client socket is readable: receive data
- Client socket is writable: flush send buffer
- Client socket has error/hangup: disconnect client

Core architectural decisions:

- `Server` is the central owner of global state.
- `Client` represents one connected user inside the server.
- `Channel` represents one IRC channel.
- `Parser` converts raw command lines into structured messages.
- `Reply` builds IRC reply strings.
- Client and Channel keep ID-based references to each other.
- No intermediate membership table is used.
- Raw owning pointers are avoided.

## 5. Data Ownership and Reference Policy

The server owns all `Client` and `Channel` objects.

```cpp
std::map<int, Client> clients;
std::map<std::string, Channel> channels;
```

The key of `clients` is the client socket FD.

The key of `channels` is the channel name.

Client and Channel relationship:

```text
Client
  joinedChannels: set<string>

Channel
  members: set<int>
  operators: set<int>
  invited: set<int>
```

This design stores IDs rather than object pointers:

- Client stores channel names.
- Channel stores client FDs.

Reason:

- Avoid dangling pointers
- Avoid manual memory ownership problems
- Keep lookup responsibility inside `Server`
- Make deletion order safer

No intermediate table is used because the relation can be represented sufficiently by the two ID sets above.

## 6. Class Overview

Main classes and structures:

| Name | Responsibility |
|---|---|
| `Server` | Socket setup, event loop, clients, channels, command dispatch |
| `Client` | State of one connected user |
| `Channel` | State of one channel |
| `Message` | Parsed IRC command data |
| `Parser` | Raw line to `Message` conversion |
| `Reply` | IRC reply and prefix generation |

Diagram source:

- `../diagrams/class_model.mmd`

## 7. Server Design Overview

`Server` is the aggregate root of the application.

It owns:

- Listen socket FD
- Password
- Running flag
- `pollfd` list
- Client map
- Channel map

Main responsibilities:

- Initialize socket
- Set non-blocking mode
- Bind and listen
- Run the event loop
- Accept new clients
- Receive client data
- Flush client send buffers
- Dispatch parsed commands
- Search clients and channels
- Join and remove clients from channels
- Disconnect clients safely
- Delete empty channels

Relationship changes must go through `Server`.

Examples:

- `joinChannel(clientFd, channelName)`
- `leaveChannel(clientFd, channelName)`
- `kickClient(operatorFd, targetFd, channelName)`
- `disconnectClient(clientFd)`

This rule keeps Client and Channel state consistent.

## 8. Client Design Overview

`Client` represents one connected IRC user on the server side.

It is not an IRC client program.

Planned state:

- FD
- Nickname
- Username
- Realname
- Hostname
- Receive buffer
- Send buffer
- PASS accepted flag
- Registered flag
- Joined channel names

Registration is completed when:

- PASS is accepted
- NICK is set
- USER is set

The Client stores joined channel names for efficient disconnect cleanup and direct reverse lookup.

It does not store `Channel*`.

## 9. Channel Design Overview

`Channel` represents one IRC channel.

Planned state:

- Channel name
- Topic
- Member FDs
- Operator FDs
- Invited client FDs
- Invite-only mode
- Topic restricted mode
- Channel key
- User limit

The first user who creates a channel by `JOIN` becomes the channel operator.

A channel is deleted when it has no members.

## 10. Message and Parser Overview

IRC commands are received as text lines.

Example:

```text
PRIVMSG #general :hello world
```

Parser output:

```text
prefix   = ""
command  = PRIVMSG
params   = ["#general", "hello world"]
```

The trailing parameter is also stored at the end of `params`, so callers do not
need to distinguish it from a normal parameter. See the class detailed design,
section 7.

`Parser` only parses.

It must not change Client, Channel, or Server state.

Expected `Message` fields:

- Optional prefix
- Command
- Parameters
- Trailing parameter

Parsing rules:

- Remove command terminator
- Split by spaces
- Treat the first `:` parameter as trailing
- Normalize command name to uppercase
- Preserve trailing spaces only when needed by IRC rules

## 11. Network and Event Loop Overview

The server uses TCP sockets.

Setup flow:

```text
socket()
  -> setsockopt(SO_REUSEADDR)
  -> fcntl(O_NONBLOCK)
  -> bind()
  -> listen()
  -> poll()
```

The non-blocking flag is set before `bind()`. Setting it after `listen()` would
leave the listening socket blocking for any connection arriving in between. See
the network and buffer detailed design, section 3.

Event loop:

```text
poll()
  |
  +-- listen fd + POLLIN  -> acceptClient()
  +-- client fd + POLLIN  -> receiveFromClient()
  +-- client fd + POLLOUT -> flushSendBuffer()
  +-- error/hangup        -> disconnectClient()
```

Important policy:

- `recv()` is called only after `poll()` reports the FD as readable.
- `send()` is handled through the send buffer and writable events.
- The server must not loop on `recv()` or `send()` without `poll()`.

Diagram source:

- `../diagrams/event_loop.mmd`

## 12. Buffer Design Overview

TCP is a stream protocol.

One IRC command may arrive split across multiple `recv()` calls.

Multiple IRC commands may also arrive in one `recv()` call.

Each Client therefore owns a receive buffer.

Receive flow:

```text
recv()
  -> append to client.receiveBuffer
  -> extract complete lines ending with CRLF or LF
  -> parse each complete line
  -> keep incomplete data in buffer
```

Each Client also owns a send buffer.

Send flow:

```text
append message to client.sendBuffer
  -> enable POLLOUT
  -> send as much as possible
  -> remove sent bytes
  -> keep remaining bytes
  -> disable POLLOUT when empty
```

This design supports partial send behavior on non-blocking sockets.

## 13. Command Processing Overview

Command processing is centralized through a dispatcher.

```text
Message
  -> dispatchCommand()
  -> handlePass()
  -> handleNick()
  -> handleUser()
  -> handleJoin()
  -> handlePrivmsg()
  -> handleKick()
  -> handleInvite()
  -> handleTopic()
  -> handleMode()
```

Each command handler should follow the same structure:

1. Check registration state
2. Validate parameter count
3. Find required Client or Channel
4. Check permissions
5. Update state
6. Send replies or broadcasts

Mandatory command groups:

- Registration: `PASS`, `NICK`, `USER`
- Channel access: `JOIN`
- Messaging: `PRIVMSG`
- Operator management: `KICK`, `INVITE`, `TOPIC`, `MODE`

Supporting command groups:

- Connection maintenance: `PING`, `PONG`
- Cleanup: `QUIT`, `PART`

Representative command sequence diagrams:

- `../diagrams/join_sequence.mmd`
- `../diagrams/privmsg_sequence.mmd`

## 14. Lifecycle Overview

Client lifecycle:

```text
Connected
  -> PASS accepted
  -> NICK set
  -> USER set
  -> Registered
  -> Joined channels
  -> Quit or disconnected
  -> Removed
```

Diagram source:

- `../diagrams/client_lifecycle.mmd`

Channel lifecycle:

```text
Not existing
  -> first JOIN
  -> created
  -> members join/leave
  -> empty
  -> deleted
```

Disconnect cleanup:

1. Copy Client joined channel names
2. Collect the other member FDs of the shared channels, without duplicates
3. Queue the quit or part message to the collected recipients
4. For each joined channel, remove the Client FD from members and operators
5. Remove the Client FD from the invite set of every channel
6. Remove Client channel names from Client
7. Delete empty channels
8. Remove Client from the nickname index
9. Remove poll FD
10. Close FD
11. Erase Client from Server

Recipients are resolved and the message is queued before the Client is removed
from the channels. Removing first would leave no recipients, so the quit would
reach nobody. See the network and buffer detailed design, sections 18 and 19.

Disconnect cleanup diagram source:

- `../diagrams/quit_cleanup.mmd`

## 15. Error Handling and Completion Criteria

The server must not crash during normal or abnormal client behavior.

Error handling targets:

- Invalid startup arguments
- Socket setup failure
- Failed system calls
- Invalid IRC commands
- Missing command parameters
- Duplicate nickname
- Wrong password
- Unregistered command usage
- Missing channel
- Missing target client
- Permission errors
- Send/receive errors
- Unexpected disconnects

Completion criteria:

- Builds with `c++ -Wall -Wextra -Werror` and C++98
- Starts as `./ircserv <port> <password>`
- Accepts multiple clients
- Uses non-blocking sockets
- Uses one `poll()` event loop
- Handles partial receive
- Handles partial send
- Implements all mandatory commands
- Supports channel creation and deletion
- Maintains Client and Channel relationship consistency
- Works with a real IRC client
- Does not leak memory
- Does not leak FDs
- Does not crash on abnormal client disconnects

README completion criteria:

- A `README.md` exists at the repository root.
- The first line follows the required 42 curriculum sentence.
- It contains Description, Instructions, and Resources sections.
- It explains how AI was used.
- It is written in English.

### Next Documents

The following detailed documents should be created after this overall design:

- Detailed class design
- Network and buffer design
- Command design
- MODE design
- Error and reply design
- Test specification
- Development operation guide
