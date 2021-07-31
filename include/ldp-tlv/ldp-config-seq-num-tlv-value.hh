#ifndef LDP_CONFIG_SEQ_NUM_TLV_H
#define LDP_CONFIG_SEQ_NUM_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"

namespace ldpd {

class LdpConfigSeqNumTlvValue : public LdpTlvValue {
public:
    LdpConfigSeqNumTlvValue();
    ~LdpConfigSeqNumTlvValue();
    uint16_t getType() const;

    uint32_t getSeq() const;

    ssize_t setSeq(uint32_t seq);

private:

    uint32_t _seq;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t tlv_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_CONFIG_SEQ_NUM_TLV_H