#ifndef LDP_LDPD
#define LDP_LDPD
#include "ldp-message/ldp-message.hh"
#include "abstraction/router.hh"
#include "ldp-tlv/ldp-tlv.hh"
#include "core/label-mapping.hh"
#include <time.h>
#include <stdint.h>
#include <map>

#define LDP_TCP_BACKLOG 16
#define LDP_MIN_LBL 16
#define LDP_MAX_LBL 1048576
#define LDP_PORT 646

#define LDP_DEF_HELLO_HOLD 15
#define LDP_DEF_THELLO_HOLD 45

#define LDP_KEY(lsr_id, lbl_space) ((((uint64_t) lsr_id) << sizeof(uint16_t)) + lbl_space)

namespace ldpd {

class LdpFsm;

class Ldpd {
public:
    Ldpd(uint32_t routerId, uint16_t labelSpace, Router *router, int routesMetric = 9);
    ~Ldpd();

    int start();
    int stop();

    void run();

    void addInterface(std::string ifname);

    uint32_t getRouterId() const;
    uint16_t getLabelSpace() const;
    uint32_t getTransportAddress() const;
    uint16_t getKeepaliveTime() const;

    uint32_t getNextMessageId();

    void setTransportAddress(uint32_t address);
    void setKeepaliveTimer(uint16_t timer);

    ssize_t transmit(LdpFsm* by, const uint8_t *buffer, size_t len);
    ssize_t handleMessage(LdpFsm* from, const LdpMessage *msg);

    std::vector<LdpFsm *> getSessions() const;

    void handleNewSession(LdpFsm* of);
    void shutdownSession(LdpFsm* of, int32_t code = LDP_SC_SHUTDOWN, uint32_t msgid = 0, uint16_t msgtype = 0);
    void removeSession(LdpFsm* of);

    void tick();

    time_t now() const;

private:

    void scanInterfaces();

    void handleSession();
    void handleSession(int fd);
    void handleHello();

    void sendHello();
    void createSession(uint32_t nei_id, uint16_t nei_ls);

    void installMappings();

    uint16_t getHoldTime(uint64_t of);

    uint32_t _id;
    uint16_t _space;
    uint32_t _transport;

    uint32_t _msg_id;

    bool _running;

    // interfaces to run ldp on
    std::vector<std::string> _ldp_ifaces;

    // running sessions - key is (lsrid << 16 + labelspace), value is fsm.
    std::map<uint64_t, LdpFsm *> _fsms;

    // opened tcp socket fds for sessions
    std::map<int, LdpFsm *> _fds;

    // hello adjacencies - key is (lsrid << 16 + labelspace), value is last hello time.
    std::map<uint64_t, time_t> _hellos;

    // hold timers for neighs
    std::map<uint64_t, uint16_t> _holds;

    // transport addresses leared from hellos, FIXME: what if another lsr w/ same id?
    std::map<uint64_t, uint32_t> _transports;

    // addresses of the peers.
    std::map<uint64_t, std::vector<uint32_t>> _addresses;

    // mappings of peers.
    std::map<uint64_t, std::vector<LdpLabelMapping>> _mappings;

    std::vector<LdpLabelMapping> _installed_mappings;
    std::vector<LdpLabelMapping> _pending_delete_mappings;

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

    // metric to use for routes.
    int _metric;

    // router api
    Router *_router;
};

}

#endif // LDP_LDPD