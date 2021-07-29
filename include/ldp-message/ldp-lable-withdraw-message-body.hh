#ifndef LDP_LABLE_WITHDRAW_MESSAGE_H
#define LDP_LABLE_WITHDRAW_MESSAGE_H
#include <vector>
#include "ldp-message/ldp-message-body.hh"
#include "ldp-tlv/ldp-tlv.hh"

namespace ldpd {

class LdpLableWithdrawMessageBody : public LdpMessageBody {
public:
    LdpLableWithdrawMessageBody();
    ~LdpLableWithdrawMessageBody();
    uint16_t getType() const;

    void addFecTlv(LdpRawTlv *tlv);
    void addLabelTlv(LdpRawTlv *tlv);

    void clearFecTlvs();
    void clearLabelTlvs();

    const std::vector<LdpRawTlv *> getFecTlvs() const;
    const std::vector<LdpRawTlv *> getLabelTlvs() const;

private:
    std::vector<LdpRawTlv *> _fecs;
    std::vector<LdpRawTlv *> _labels;

    uint8_t *_tmp_raw_buffer;
    size_t _tmp_raw_buffer_sz;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}


#endif // LDP_LABLE_WITHDRAW_MESSAGE_H