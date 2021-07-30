#ifndef LDP_ADDRESS_MESSAGE_H
#define LDP_ADDRESS_MESSAGE_H
#include "ldp-message/ldp-message-body.hh"
#include "ldp-tlv/ldp-tlv.hh"

namespace ldpd {

class LdpAddressMessageBody : public LdpMessageBody {
public:
    LdpAddressMessageBody();
    virtual ~LdpAddressMessageBody();
    uint16_t getType() const;

    ssize_t setAddresses(LdpRawTlv *tlv);

    const LdpRawTlv* getAddresses() const;

private:
    LdpRawTlv* _addresses;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}


#endif // LDP_ADDRESS_MESSAGE_H