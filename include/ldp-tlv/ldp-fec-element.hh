#ifndef LDP_FEC_ELEMENT_H
#define LDP_FEC_ELEMENT_H
#include "core/serializable.hh"

namespace ldpd {

class LdpFecElement : public Serializable {
public:
    virtual ~LdpFecElement() {};
    virtual uint8_t getType() const = 0;
};

}

#endif // LDP_FEC_ELEMENT_H