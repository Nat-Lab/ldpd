#ifndef LDP_IPV4_TRANSPORT_ADDRESS_TLV_H
#define LDP_IPV4_TRANSPORT_ADDRESS_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"

namespace ldpd {

class LdpIpv4TransportAddressTlvValue : public LdpTlvValue {
public:
    LdpIpv4TransportAddressTlvValue();
    ~LdpIpv4TransportAddressTlvValue();
    uint16_t getType() const;

    uint32_t getAddress() const;
    const char* getAddressString() const;

    ssize_t setAddress(uint32_t address);
    ssize_t setAddressString(const char* address);

private:

    uint32_t _address;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t tlv_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_IPV4_TRANSPORT_ADDRESS_TLV_H