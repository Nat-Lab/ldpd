#ifndef LDP_LABLE_WITHDRAW_MESSAGE_H
#define LDP_LABLE_WITHDRAW_MESSAGE_H
#include <vector>
#include "ldp-message/ldp-message-body.hh"
#include "ldp-tlv/ldp-tlv.hh"

namespace ldpd {

class LdpLableWithdrawMessageBody : public LdpMessageBody {
public:
    uint16_t getType() const;

    void addFecTlv(LdpFecTlv *tlv);
    void addLabelTlv(LdpLabelTlv *tlv);

    void clearFecTlvs();
    void clearLabelTlvs();

    const std::vector<LdpFecTlv *> getFecTlvs() const;
    const std::vector<LdpLabelTlv *> getLabelTlvs() const;

private:
    std::vector<LdpFecTlv *> _fecs;
    std::vector<LdpFecTlv *> _labels;

// ----------------------------------------------------------------------------

public:
    ssize_t print(uint8_t *to, size_t buf_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz) const;

    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;
    virtual size_t length() const = 0;
};

}


#endif // LDP_LABLE_WITHDRAW_MESSAGE_H