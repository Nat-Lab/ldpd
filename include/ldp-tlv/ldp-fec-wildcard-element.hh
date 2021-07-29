#ifndef LDP_FEC_WILDCARD_ELEMENT_H
#define LDP_FEC_WILDCARD_ELEMENT_H
#include "core/serializable.hh"

namespace ldpd {

class LdpFecWildcardElement : public Serializable {
public:
    uint8_t getType() const;
};

}

#endif // LDP_FEC_WILDCARD_ELEMENT_H