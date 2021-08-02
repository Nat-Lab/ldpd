ldpd
---
A LDP (label distribution protocol) implemtation. 

This project implements a subset of the LDP features, as specified by RFC5036. The following features are not implemented:

- IPv6
- Frame relay
- ATM

#### What's included 

- A functional LDP PDU parser with support of most messages and TLVs defined in RFC5036; unknown TLVs are not parsed but can still be processed.
- A functional LDP FSM and daemon, which can interact with the OS using the routing abstraction layer; see below.
- Routing API abstractions. Provides abstraction of the OS routing tables, so the nat-lab `ldpd` can be easily portable to other OSes.
    - A `rtnetlink` backed implementation is included.
