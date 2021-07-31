#ifndef LDP_COMMON_SESSION_PARAMS_TLV_H
#define LDP_COMMON_SESSION_PARAMS_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"

namespace ldpd {

class LdpCommonSessionParamsTlvValue : public LdpTlvValue {
public:
    LdpCommonSessionParamsTlvValue();
    ~LdpCommonSessionParamsTlvValue();
    uint16_t getType() const;

    uint16_t getProtocolVersion() const;
    uint16_t getKeepaliveTime() const;
    uint16_t getPathVectorLimit() const;
    uint16_t getMaxPduLength() const;
    uint32_t getReceiverRouterId() const;
    uint16_t getReceiverLabelSpace() const;

    ssize_t setProtocolVersion(uint16_t protocolVersion);
    ssize_t setKeepaliveTime(uint16_t keepaliveTime);
    ssize_t setPathVectorLimit(uint16_t pathVectorLimit);
    ssize_t setMaxPduLength(uint16_t maxPduLength);
    ssize_t setReceiverRouterId(uint32_t receiverRouterId);
    ssize_t setReceiverLabelSpace(uint16_t receiverLabelSpace);

    bool loopDetection() const;

    ssize_t setloopDetection(bool loopDetection);

    const char* getReceiverRouterIdString() const;
    ssize_t setReceiverRouterIdString(const char* id);    

private:

    uint16_t _protocolVersion;
    uint16_t _keepaliveTime;
    uint16_t _pathVectorLimit;
    uint16_t _maxPduLength;
    uint32_t _receiverRouterId;
    uint16_t _receiverLabelSpace;

    bool _loopDetection;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t tlv_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_COMMON_SESSION_PARAMS_TLV_H