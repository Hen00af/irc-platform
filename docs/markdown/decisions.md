# ft_irc Design Decisions

## DD-001: Mandatory Only

Status: Accepted

The initial design targets only the mandatory part of ft_irc.

Bonus features, including file transfer and bot functionality, are excluded from the initial design.

## DD-002: Server Owns Clients and Channels

Status: Accepted

`Server` owns all `Client` and `Channel` objects.

Clients are stored by FD.

Channels are stored by channel name.

## DD-003: Client and Channel Use ID-Based References

Status: Accepted

Client stores joined channel names.

Channel stores client FDs.

Raw pointers between Client and Channel are not used.

## DD-004: No Intermediate Membership Table

Status: Accepted

No separate membership table or membership class is used for the mandatory design.

The Client-to-Channel relationship is represented by:

- `Client::_joinedChannels`
- `Channel::_members`

Operator and invite state are stored in Channel because they are channel-specific.

## DD-005: Relationship Updates Go Through Server

Status: Accepted

Client and Channel relationship changes are performed only by Server-level operations.

Examples:

- `joinChannel`
- `leaveChannel`
- `kickClient`
- `disconnectClient`

This keeps both sides of the relationship consistent.

## DD-006: Send Buffer Is Included in Client

Status: Accepted

Each Client has a send buffer.

Reason:

- Sockets are non-blocking.
- `send()` may write only part of a message.
- Writes must be coordinated with `poll()` via `POLLOUT`.

## DD-007: IPv4 Is the Initial Transport

Status: Accepted

The mandatory implementation uses TCP/IPv4.

IPv6 is allowed by the subject but is not required for the initial implementation.

## DD-008: IRC Lookup Uses Normalized Keys

Status: Accepted

Displayed nicknames and channel names retain their original spelling.

Lookup keys use IRC case folding.

## DD-009: Event-Loop Deletion Is Deferred

Status: Accepted

Client FDs are placed in a pending-disconnect set while poll events are being iterated.

The pollfd vector and client map are erased only after the current event scan.

## DD-010: One I/O Call Per Readiness Notification

Status: Accepted

Each readiness notification causes at most one corresponding `accept()`, `recv()`, or `send()` call.

Completed IRC lines already present in memory may all be processed without another poll.

## DD-011: Protocol And Buffer Limits

Status: Accepted

- IRC line: 512 bytes including CRLF
- Receive buffer: 64 KiB per Client
- Send buffer: 1 MiB per Client
- Receive chunk: 4096 bytes

## DD-012: No Automatic Operator Transfer

Status: Accepted

If the last channel operator leaves or loses `+o`, the server does not automatically promote another member.

The subject does not define an automatic transfer rule.

## DD-013: Server Identity

Status: Accepted

Numeric replies and server-originated commands use `ircserv.local` as the server name and `1.0` as the version.
