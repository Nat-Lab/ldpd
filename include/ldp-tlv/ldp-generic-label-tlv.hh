#ifndef LDP_LABEL_TLV_H
#define LDP_LABEL_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"

namespace ldpd {

class LdpGenericLabelTlv : public LdpTlvValue {
public:
    uint16_t getType() const;
    uint32_t getLabel() const;
    void setLabel(uint32_t label);

private:
    uint32_t _label;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_LABEL_TLV_H