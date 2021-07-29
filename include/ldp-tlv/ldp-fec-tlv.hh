#ifndef LDP_FEC_TLV_H
#define LDP_FEC_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"

namespace ldpd {

class LdpFecTlv : public LdpTlvValue {
public:
    uint16_t getType() const;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_TLV_FEC_H