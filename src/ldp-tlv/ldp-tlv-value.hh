#ifndef LDP_TLV_VALUE_H
#define LDP_TLV_VALUE_H
#include "core/serializable.hh"

namespace ldpd {

class LdpTlvValue : public Serializable {
public:
    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;
    virtual ssize_t length() const = 0;
    virtual uint16_t getType() const = 0;
    virtual uint16_t getLength() const = 0;
};

}

#endif // LDP_TLV_VALUE_H