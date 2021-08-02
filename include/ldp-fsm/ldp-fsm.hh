#ifndef LDP_FSM
#define LDP_FSM
#include <unistd.h>
#include <stdint.h>
#include <string>
#include "core/ldpd.hh"
#include "ldp-pdu/ldp-pdu.hh"

namespace ldpd {
    
enum LdpRole {
    Passive, Active
};

enum LdpSessionState {
    Initialized, OpenReceived, OpenSent, Operational, Invalid
};

class LdpFsm {
public:
    LdpFsm(Ldpd *ldpd, uint16_t localLabelSpace, uint32_t neighborId, uint16_t neighborLabelSpace);

    ssize_t receive(const uint8_t *packet, size_t size);

    ssize_t step();

    LdpSessionState getState() const;
    LdpRole getRole() const;

    uint32_t getLocalId() const;
    uint16_t getLocalLabelSpace() const;

    uint32_t getNeighborId() const;
    uint16_t getNeighborLabelSpace() const;

    ssize_t send(LdpPdu &pdu);
    ssize_t sendKeepalive();

private:
    void fillPduHeader(LdpPdu &to) const;
    void fillInitMessage(LdpPdu &to);
    void addInitTlv(LdpPdu &to);

    uint32_t _neighId;
    uint16_t _neighLs;
    uint16_t _localLs;
    LdpSessionState _state;
    Ldpd *_ldpd;

    uint32_t _msgId;
};

}

#endif // LDP_FSM