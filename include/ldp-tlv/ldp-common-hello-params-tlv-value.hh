#ifndef LDP_COMMON_HELLO_PARAMS_TLV_H
#define LDP_COMMON_HELLO_PARAMS_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"
#include "ldp-fec-element.hh"

namespace ldpd {

class LdpCommonHelloParamsTlvValue : public LdpTlvValue {
public:
    LdpCommonHelloParamsTlvValue();
    ~LdpCommonHelloParamsTlvValue();
    uint16_t getType() const;

    uint16_t getHoldTime() const;
    bool targeted() const;
    bool requestTargeted() const;

    ssize_t setHoldTime(uint16_t holdTime);
    ssize_t setTargeted(bool targeted);
    ssize_t setRequestTargeted(bool requestTargeted);

private:
    uint16_t _holdTime;
    bool _targeted;
    bool _requestTargeted;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t tlv_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_COMMON_HELLO_PARAMS_TLV_H