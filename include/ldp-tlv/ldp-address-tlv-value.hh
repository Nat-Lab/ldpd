#ifndef LDP_ADDRESS_TLV_H
#define LDP_ADDRESS_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"
#include "ldp-fec-element.hh"

#include <vector>

namespace ldpd {

class LdpAddressTlvValue : public LdpTlvValue {
public:
    LdpAddressTlvValue();
    ~LdpAddressTlvValue();
    uint16_t getType() const;

    const std::vector<uint32_t> getAddresses() const;

    void clearAddresses();

    ssize_t addAddress(uint32_t address);

private:
    std::vector<uint32_t> _addresses;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t tlv_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_ADDRESS_TLV_H