#ifndef LDP_TLV_H
#define LDP_TLV_H
#include "serializable.hh"

namespace ldpd {

class LdpTlv : public Serializable {
public:
    uint16_t getType() const;
    uint16_t getLength() const;
    const uint8_t* getRawValue() const;

    void setType(uint16_t type);
    void setLength(uint16_t length);
    void setRawValue(size_t sz, const uint8_t *buffer);

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    ssize_t length() const;

protected:
    virtual ssize_t doPrint(size_t indent, uint8_t *to, size_t buf_sz) const = 0;

};

}

#endif // LDP_TLV_H