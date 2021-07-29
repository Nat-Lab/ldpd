#ifndef LDP_TLV_H
#define LDP_TLV_H
#include "serializable.hh"

namespace ldpd {

class LdpTlv : public Serializable {
    bool unknown() const;
    bool forwardUnknown() const;

    uint16_t getType() const;
    uint16_t getLength() const;
    const uint8_t* getRawValue() const;

protected:
    uint8_t *_raw_buffer;
    size_t *_raw_buffer_size; // might not match len field in case of bad pkt.

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    ssize_t length() const;

protected:
    ssize_t doPrint(size_t indent, uint8_t *to, size_t buf_sz) const;
};

class LdpRawTlv : public LdpTlv {
public:
    void setType(uint16_t type);
    void setLength(uint16_t length);
    void setRawValue(size_t sz, const uint8_t *buffer);

    LdpCookedTlv *getCooked() const;
};

class LdpCookedTlv : public LdpTlv {
public:
    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;
    virtual ssize_t length() const = 0;

protected:
    virtual ssize_t doPrint(size_t indent, uint8_t *to, size_t buf_sz) const = 0;
};

class LdpFecTlv : public LdpCookedTlv {

protected:
    

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    ssize_t length() const;

protected:
    ssize_t doPrint(size_t indent, uint8_t *to, size_t buf_sz) const;
};

}

#endif // LDP_TLV_H