# Architecture

`irc-platform` keeps the HTTP and IRC protocol engines independent and joins
them at the product boundary.

```text
Browser
  │ HTTPS :443                 IRC client
  ▼                                │ TLS :6697
Webserv :8080                      ▼
  │ static UI                   ft_irc :6667
  │                                ▲
  └── WSS :8443 ── Gateway :3001 ──┘
```

## Components

### Webserv

The imported C++98 HTTP/1.1 server serves the browser application from
`/chat/`, exposes `/health/`, and retains its standalone upload and CGI
features.

### Gateway

The Node.js gateway is deliberately small. Browsers cannot open arbitrary TCP
sockets, so it translates a constrained JSON protocol over WebSocket into IRC
commands. It validates nicknames, channels, message lengths, origins, payload
sizes, and action rate.

The gateway never accepts the IRC server password from the browser. It reads
the deployment secret and performs `PASS`, `NICK`, and `USER` when opening the
upstream connection.

### IRC server

The imported C++98 IRC server remains the source of truth for realtime channel
membership, modes, operators, and message delivery. Direct IRC clients can
connect over TLS port 6697 in the deployed configuration.

## Deployment model

One machine runs all three processes. This is appropriate for a personal
project because the IRC server keeps channel state in memory and must have a
single active instance. The entrypoint treats any child-process exit as a
deployment failure and terminates the others so the platform can restart the
machine cleanly.

Fly terminates TLS for HTTP, WebSocket, and IRC connections. Internal
connections remain on loopback.

## Security boundaries

- `IRC_PASSWORD` is a platform secret.
- WebSocket origins are allowlisted in production.
- Browser messages cannot send arbitrary IRC commands.
- All public traffic uses TLS.
- The container runs without root privileges or Linux capabilities.
- Message and receive buffers have explicit limits.
- One machine remains running to preserve IRC state.
