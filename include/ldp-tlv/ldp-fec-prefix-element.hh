#ifndef LDP_FEC_PREFIX_ELEMENT_H
#define LDP_FEC_PREFIX_ELEMENT_H
#include "core/serializable.hh"

namespace ldpd {

class LdpFecPrefixElement : public Serializable {
public:
    uint8_t getType() const;

    uint32_t getPrefix() const;
    uint8_t getPrefixLength() const;

    ssize_t setPrefix(uint32_t prefix);
    ssize_t setPrefixLength(uint8_t length);

private:
    uint8_t _prelen;
    uint32_t _prefix;
};

}

#endif // LDP_FEC_PREFIX_ELEMENT_H