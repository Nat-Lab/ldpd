#ifndef LDP_LABEL_WITHDRAW_MESSAGE_H
#define LDP_LABEL_WITHDRAW_MESSAGE_H
#include "ldp-message/ldp-message-body.hh"
#include "ldp-tlv/ldp-tlv.hh"

namespace ldpd {

class LdpLabelWithdrawMessageBody : public LdpMessageBody {
public:
    LdpLabelWithdrawMessageBody();
    virtual ~LdpLabelWithdrawMessageBody();
    uint16_t getType() const;

    void setFecTlv(LdpRawTlv *tlv);
    void setLabelTlv(LdpRawTlv *tlv);

    const LdpRawTlv* getFecTlv() const;
    const LdpRawTlv* getLabelTlv() const;

private:
    LdpRawTlv *_fec;
    LdpRawTlv *_label;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}


#endif // LDP_LABEL_WITHDRAW_MESSAGE_H