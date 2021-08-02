#ifndef LDP_FSM
#define LDP_FSM
#include <unistd.h>
#include <stdint.h>
#include <string>
#include "core/ldpd.hh"
#include "ldp-pdu/ldp-pdu.hh"

namespace ldpd {
    
enum LdpSessionState {
    Invalid = 0, Initialized, OpenReceived, OpenSent, Operational
};

class LdpFsm {
public:
    LdpFsm(Ldpd *ldpd);

    ssize_t receive(const uint8_t *packet, size_t size);

    ssize_t init(uint32_t neighId, uint16_t neighLabelSpace);

    LdpSessionState getState() const;

    uint32_t getLocalId() const;
    uint16_t getLocalLabelSpace() const;

    uint32_t getNeighborId() const;
    uint16_t getNeighborLabelSpace() const;

    ssize_t send(LdpPdu &pdu);
    ssize_t sendKeepalive();

    void tick();

private:
    int processInit(const LdpMessage *init);

    void fillPduHeader(LdpPdu &to) const;
    void createInitPdu(LdpPdu &to);

    void changeState(LdpSessionState newState);

    uint16_t _keep;
    time_t _last_send, _last_recv;

    uint32_t _neighId;
    uint16_t _neighLs;
    LdpSessionState _state;
    Ldpd *_ldpd;
};

}

#endif // LDP_FSM