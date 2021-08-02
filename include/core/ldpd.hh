#ifndef LDP_LDPD
#define LDP_LDPD
#include "ldp-message/ldp-message.hh"
#include "abstraction/router.hh"
#include <stdint.h>
#include <map>
#include <vector>

#define LDP_MIN_LBL 16
#define LDP_MAX_LBL 1048576
#define LDP_PORT 646

typedef ssize_t (*tx_handler_t) (const uint8_t *, size_t);

namespace ldpd {

class LdpFsm;

class Ldpd {
public:
    Ldpd(uint32_t routerId, Router *router);
    ~Ldpd();

    int start();
    int stop();

    void run();

    uint32_t getRouterId() const;
    uint32_t getTransportAddress() const;
    uint16_t getKeepaliveTimer() const;

    void setTransportAddress(uint32_t address);
    void setKeepaliveTimer(uint16_t timer);

    tx_handler_t getTransmitHandler(LdpFsm* requester);
    ssize_t handleMessage(LdpFsm* from, const LdpMessage *msg);

    std::vector<LdpFsm *> getSessions() const;

    void shutdownSession(LdpFsm* of);
    void removeSession(LdpFsm* of);

private:

    uint32_t _id;
    uint32_t _transport;

    bool _running;

    // running sessions
    std::vector<LdpFsm *> _fsms;

    // packet tx handlers for sessions
    std::map<const LdpFsm *, tx_handler_t> _tx_handlers;

    // opened tcp socket fds for sessions
    std::map<const LdpFsm *, int> _fds;

    // timers
    uint16_t _hello;
    uint16_t _keep;
    uint16_t _hold;

    // fd for the master tcp socket (the one use w/ accept syscall)
    int _tfd;

    // fd for the mcast udp listening socket
    int _ufd;

    // router api
    Router *_router;
};

}

#endif // LDP_LDPD