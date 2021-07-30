#ifndef LDP_FEC_TLV_H
#define LDP_FEC_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"
#include "ldp-fec-element.hh"

#include <vector>

namespace ldpd {

class LdpFecTlvValue : public LdpTlvValue {
public:
    LdpFecTlvValue();
    ~LdpFecTlvValue();
    uint16_t getType() const;

    const std::vector<LdpFecElement *> getElements() const;

    void clearElements();

    ssize_t addElement(LdpFecElement *element);

private:
    std::vector<LdpFecElement *> _elements;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t tlv_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_TLV_FEC_H