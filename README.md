ldpd
---
A simple, portable, LDP (label distribution protocol) implemtation. 

This project implements a subset of the LDP feature, as defined in RFC5036. The following features are not implemented:

- IPv6
- Frame relay
- ATM

#### What's included 

- A functional LDP PDU parser with support of most messages and TLV defined in RFC5036; unknown TLVs are not parsed but can still be processed.
- A functional LDP FSM, which can interact with the OS using the socket and routing abstract layer; see below.
- Socket API abstractions. Provides abstraction of the OS socket APIs, so the nat-lab `ldpd` can be easily portable to other OSes - even some other obscure simulation systems, like `ns-3`.
    - A *nix socket layer implementation is included - uses POSIX APIs.
- Routing API abstractions. Provides abstraction of the OS routing table manipulation APIs, so the nat-lab `ldpd` can be easily portable to other OSes.
    - A Linux routing layer implementation is included - uses `rtnetlink`.
