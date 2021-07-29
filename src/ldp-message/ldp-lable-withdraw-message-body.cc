#include "utils/log.hh"
#include "ldp-message/ldp-lable-withdraw-message-body.hh"

#include <stdlib.h>

namespace ldpd {

LdpLableWithdrawMessageBody::LdpLableWithdrawMessageBody() : _fecs(), _labels() {
    _tmp_raw_buffer = nullptr;
    _tmp_raw_buffer_sz = 0;
}

LdpLableWithdrawMessageBody::~LdpLableWithdrawMessageBody() {
    this->clearFecTlvs();
    this->clearLabelTlvs();

    if (_tmp_raw_buffer != nullptr) {
        free(_tmp_raw_buffer);
    }
}

/**
 * @brief add fec tlv.
 * 
 * note: we handle freeing of tlv ourself. 
 * 
 * @param tlv tlv.
 */
void LdpLableWithdrawMessageBody::addFecTlv(LdpRawTlv *tlv) {
    uint16_t type = tlv->getType();

    if (type != 0x0100) {
        log_error("tlv not fec tlv: want type 0x0100 but saw 0x%.4x\n", type);
        return;
    }

    _fecs.push_back(tlv);
}

/**
 * @brief add lable tlv.
 * 
 * note: we handle freeing of tlv ourself. 
 * 
 * @param tlv tlv.
 */
void LdpLableWithdrawMessageBody::addLabelTlv(LdpRawTlv *tlv) {
    uint16_t type = tlv->getType();

    if (type != 0x0200 && type != 0x0201 && type != 0x0202) {
        log_error("tlv not lbl tlv: want type 0x0200, 0x0201 or 0x0202 but saw 0x%.4x\n", type);
        return;
    }

    _labels.push_back(tlv);
}

/**
 * @brief clear all fec tlvs.
 * 
 */
void LdpLableWithdrawMessageBody::clearFecTlvs() {
    for (LdpRawTlv *tlv : _fecs) {
        delete tlv;
    }

    _fecs.clear();
}

/**
 * @brief clear all label tlvs.
 * 
 */
void LdpLableWithdrawMessageBody::clearLabelTlvs() {
    for (LdpRawTlv *tlv : _labels) {
        delete tlv;
    }

    _fecs.clear();
}

/**
 * @brief get all fec tlvs.
 * 
 * @return const std::vector<LdpRawTlv *> tlvs.
 */
const std::vector<LdpRawTlv *> LdpLableWithdrawMessageBody::getFecTlvs() const {
    return _fecs;
}

/**
 * @brief get all label tlvs.
 * 
 * @return const std::vector<LdpRawTlv *> tlvs.
 */
const std::vector<LdpRawTlv *> LdpLableWithdrawMessageBody::getLabelTlvs() const {
    return _labels;
}

/**
 * @brief parse a withdraw message.
 * 
 * @param from source buffer.
 * @param msg_sz msg size from the msg hdr.
 * @return ssize_t bytes read, or -1 on error.
 */
ssize_t LdpLableWithdrawMessageBody::parse(const uint8_t *from, size_t msg_sz) {

}


ssize_t write(uint8_t *to, size_t buf_sz) const;
size_t length() const;

}