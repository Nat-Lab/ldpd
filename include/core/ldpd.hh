#ifndef LDP_LDPD
#define LDP_LDPD
#include "ldp-message/ldp-message.hh"
#include "abstraction/router.hh"
#include <time.h>
#include <stdint.h>
#include <map>

#define LDP_TCP_BACKLOG 16
#define LDP_MIN_LBL 16
#define LDP_MAX_LBL 1048576
#define LDP_PORT 646

#define LDP_KEY(lsr_id, lbl_space) ((((uint64_t) lsr_id) << sizeof(uint16_t)) + lbl_space)

namespace ldpd {

class LdpFsm;

class Ldpd {
public:
    Ldpd(uint32_t routerId, uint16_t labelSpace, Router *router);
    ~Ldpd();

    int start();
    int stop();

    void run();

    uint32_t getRouterId() const;
    uint16_t getLabelSpace() const;
    uint32_t getTransportAddress() const;
    uint16_t getKeepaliveTimer() const;

    uint32_t getNextMessageId();

    void setTransportAddress(uint32_t address);
    void setKeepaliveTimer(uint16_t timer);

    ssize_t transmit(LdpFsm* by, const uint8_t *buffer, size_t len);
    ssize_t handleMessage(LdpFsm* from, const LdpMessage *msg);

    std::vector<LdpFsm *> getSessions() const;

    void shutdownSession(LdpFsm* of);
    void removeSession(LdpFsm* of);

    void tick();

private:

    void scanInterfaces();

    void handleSession();
    void handleSession(int fd);
    void handleHello();

    void sendHello();
    void createSession(uint32_t nei_id, uint16_t nei_ls);

    uint32_t _id;
    uint16_t _space;
    uint32_t _transport;

    uint32_t _msg_id;

    bool _running;

    // running sessions - key is (lsrid << 16 + labelspace), value is fsm.
    std::map<uint64_t, LdpFsm *> _fsms;

    // opened tcp socket fds for sessions
    std::map<int, LdpFsm *> _fds;

    // hello adjacencies - key is (lsrid << 16 + labelspace), value is last hello time.
    std::map<uint64_t, time_t> _hellos;

    // transport addresses leared from hellos, FIXME: what if another lsr w/ same id?
    std::map<uint64_t, uint32_t> _transports;

    // interface cache
    std::vector<Interface> _ifaces;

    // timers
    uint16_t _hello;
    uint16_t _keep;
    uint16_t _hold;
    uint16_t _ifscan;

    // time last hello is sent out
    time_t _last_hello;

    // time last interfaces scan
    time_t _last_scan;

    // time now.
    time_t _now;

    // fd for the master tcp socket (the one use w/ accept syscall)
    int _tfd;

    // fd for the mcast udp listening socket
    int _ufd;

    // router api
    Router *_router;
};

}

#endif // LDP_LDPD