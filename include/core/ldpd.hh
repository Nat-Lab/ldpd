#ifndef LDP_LDPD
#define LDP_LDPD
#include <stdint.h>

typedef ssize_t (*tx_handler_t) (const uint8_t *, size_t);

namespace ldpd {

class LdpFsm;

class Ldpd {
public:
    Ldpd();
    ~Ldpd();

    uint32_t getRouterId() const;
    uint32_t getTransportAddress() const;
    uint16_t getKeepaliveTimer() const;

    tx_handler_t getTransmitHandler(LdpFsm* requester);
    ssize_t handleMessage(LdpFsm* from, uint32_t neighborId, uint16_t neighborLabelSpace, const LdpMessage *msg);

    void freeTransmitHandler(LdpFsm* requester);
    void shutdown(LdpFsm* of);
};

}

#endif // LDP_LDPD