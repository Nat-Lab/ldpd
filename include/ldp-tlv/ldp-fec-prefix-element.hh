#ifndef LDP_FEC_PREFIX_ELEMENT_H
#define LDP_FEC_PREFIX_ELEMENT_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-fec-element.hh"

namespace ldpd {

class LdpFecPrefixElement : public LdpFecElement {
public:
    LdpFecPrefixElement();
    uint8_t getType() const;

    uint32_t getPrefix() const;
    uint8_t getPrefixLength() const;

    ssize_t setPrefix(uint32_t prefix);
    ssize_t setPrefixLength(uint8_t length);

    const char* getPrefixString() const;
    ssize_t setPrefixString(const char *prefix);

private:
    uint8_t _prelen;
    uint32_t _prefix;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t buf_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_FEC_PREFIX_ELEMENT_H