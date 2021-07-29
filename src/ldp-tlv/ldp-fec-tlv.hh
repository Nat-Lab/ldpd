#ifndef LDP_FEC_TLV_H
#define LDP_FEC_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"

namespace ldpd {

class LdpFecTlv : public LdpTlvValue {

protected:
    

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    ssize_t length() const;

protected:
    ssize_t doPrint(size_t indent, uint8_t *to, size_t buf_sz) const;
};

}

#endif // LDP_TLV_FEC_H