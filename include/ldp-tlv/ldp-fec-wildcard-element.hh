#ifndef LDP_FEC_WILDCARD_ELEMENT_H
#define LDP_FEC_WILDCARD_ELEMENT_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-fec-element.hh"

namespace ldpd {

class LdpFecWildcardElement : public LdpFecElement {
public:
    uint8_t getType() const;
    
// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t buf_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_FEC_WILDCARD_ELEMENT_H