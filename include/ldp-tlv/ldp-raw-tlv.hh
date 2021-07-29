#ifndef LDP_RAW_TLV_H
#define LDP_RAW_TLV_H
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

    const uint8_t* peekRawValue() const;

    ssize_t setType(uint16_t type);
    ssize_t setLength(uint16_t length);

    ssize_t setRawValue(size_t rawValueSize, const uint8_t *src);
    ssize_t setValue(const LdpTlvValue *value);
    
    LdpTlvValue* getParsedValue() const;

protected:
    uint8_t *_raw_buffer;
    size_t _raw_buffer_size; // might not match len field in case of bad pkt.

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_RAW_TLV_H