#ifndef LDP_RAW_TLV_H
#define LDP_TLLDP_RAW_TLV_HV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"

namespace ldpd {

class LdpRawTlv : public Serializable {
public:
    LdpRawTlv();
    ~LdpRawTlv();

    bool unknown() const;
    bool forwardUnknown() const;

    uint16_t getType() const;
    uint16_t getLength() const;
    const uint8_t* getRawValue() const;

    void setType(uint16_t type);
    void setLength(uint16_t length);
    void setRawValue(size_t sz, const uint8_t *buffer);

    LdpTlvValue* getParsedValue();

protected:
    uint8_t *_raw_buffer;
    size_t _raw_buffer_size; // might not match len field in case of bad pkt.

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    ssize_t length() const;

protected:
    ssize_t doPrint(size_t indent, uint8_t *to, size_t buf_sz) const;
};

}

#endif // LDP_RAW_TLV_H