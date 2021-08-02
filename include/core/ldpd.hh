#ifndef LDP_LDPD
#define LDP_LDPD
#include <stdint.h>
#include <map>
#include <vector>

typedef ssize_t (*tx_handler_t) (const uint8_t *, size_t);

namespace ldpd {

class LdpFsm;

class Ldpd {
public:
    Ldpd(uint32_t routerId);
    ~Ldpd();

    uint32_t getRouterId() const;
    uint32_t getTransportAddress() const;
    uint16_t getKeepaliveTimer() const;

    void setTransportAddress(uint32_t address);
    void setKeepaliveTimer(uint16_t timer);

    tx_handler_t getTransmitHandler(LdpFsm* requester);
    ssize_t handleMessage(LdpFsm* from, uint32_t neighborId, uint16_t neighborLabelSpace, const LdpMessage *msg);

    std::vector<LdpFsm *> getSessions() const;

    void shutdownSession(LdpFsm* of);

    int start();
    int stop();
private:
    bool _running;

    std::vector<LdpFsm *> _fsms;
    std::map<const LdpFsm *, tx_handler_t> _tx_handlers;

    // timers
    uint16_t _hello;
    uint16_t _keep;
    uint16_t _hold;
};

}

#endif // LDP_LDPD