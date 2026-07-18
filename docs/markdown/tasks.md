# ft_irc Design and Implementation Tasks

## Design Documents

- [x] Create documentation directory structure
- [x] Confirm ft_irc subject text source
- [x] Create requirements extract
- [x] Create overall design Markdown source
- [x] Create initial Mermaid diagram sources
- [x] Generate and visually verify overall design PDF
- [x] Create Japanese overall design Markdown and PDF
- [ ] Review overall design with pair partner
- [x] Create detailed class design
- [x] Create network and buffer design
- [x] Create command design
- [x] Create MODE design
- [x] Create error and reply design
- [ ] Create test specification
- [ ] Create development operation guide
- [x] Generate PDF versions for the detailed design documents
- [ ] Generate PDF versions for the test and development operation documents

## Mandatory Implementation Roadmap

- [ ] Argument validation
- [ ] Server socket setup
- [ ] Non-blocking setup
- [ ] `poll()` event loop
- [ ] Client accept
- [ ] Client receive buffer
- [ ] Parser
- [ ] Client send buffer
- [ ] PASS
- [ ] NICK
- [ ] USER
- [ ] JOIN
- [ ] PRIVMSG
- [ ] TOPIC
- [ ] INVITE
- [ ] KICK
- [ ] MODE `i`
- [ ] MODE `t`
- [ ] MODE `k`
- [ ] MODE `o`
- [ ] MODE `l`
- [ ] QUIT
- [ ] PART
- [ ] PING/PONG
- [ ] Integration test with `nc`
- [ ] Integration test with real IRC client
- [ ] Memory leak check
- [ ] FD leak check

## Open Items

- [ ] Confirm WeeChat as the final reference IRC client with pair partner
- [x] Decide IPv4 only or IPv4/IPv6
- [x] Decide server name used in replies
- [x] Decide nickname validation rules
- [x] Decide channel name normalization rules
- [x] Decide send/receive buffer size limits
- [x] Decide behavior when last channel operator leaves
