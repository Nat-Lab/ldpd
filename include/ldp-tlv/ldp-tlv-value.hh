#ifndef LDP_TLV_VALUE_H
#define LDP_TLV_VALUE_H
#include "core/serializable.hh"

namespace ldpd {

class LdpTlvValue : public Serializable {
public:
    virtual ~LdpTlvValue() {};
    virtual uint16_t getType() const = 0;
};

}

#endif // LDP_TLV_VALUE_H