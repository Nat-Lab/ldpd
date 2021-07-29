#include "utils/log.hh"
#include "ldp-message/ldp-label-withdraw-message-body.hh"

#include <stdlib.h>

namespace ldpd {

LdpLabelWithdrawMessageBody::LdpLabelWithdrawMessageBody() {
    _fec = nullptr;
    _label = nullptr;
}

LdpLabelWithdrawMessageBody::~LdpLabelWithdrawMessageBody() {
    if (_fec != nullptr) {
        delete _fec;
    }

    if (_label != nullptr) {
        delete _label;
    }
}

uint16_t LdpLabelWithdrawMessageBody::getType() const {
    return 0x0402;
}

/**
 * @brief set fec tlv.
 * 
 * @param tlv tlv - we will handle freeing this.
 */
void LdpLabelWithdrawMessageBody::setFecTlv(LdpRawTlv *tlv) {
    uint16_t type = tlv->getType();

    if (type != 0x0100) {
        log_error("tlv not fec tlv: want type 0x0100 but saw 0x%.4x\n", type);
        return;
    }

    if (_fec != nullptr) {
        delete _fec;
    }

    _fec = tlv;
}

/**
 * @brief set label tlv.
 * 
 * @param tlv tlv - we will handle freeing this. set to nullptr to remove lbl
 * tlv.
 */
void LdpLabelWithdrawMessageBody::setLabelTlv(LdpRawTlv *tlv) {
    if (tlv == nullptr) {
        if (_label != nullptr) {
            delete _label;
        }

        _label = nullptr;
        return;
    }

    uint16_t type = tlv->getType();

    if (type != 0x0200 && type != 0x0201 && type != 0x0202) {
        log_error("tlv not lbl tlv: want type 0x0200, 0x0201 or 0x0202 but saw 0x%.4x\n", type);
        return;
    }

    if (_label != nullptr) {
        delete _label;
    }

    _label = tlv;
}

/**
 * @brief get all fec tlvs.
 * 
 * @return const LdpRawTlv* tlv.
 */
const LdpRawTlv* LdpLabelWithdrawMessageBody::getFecTlv() const {
    return _fec;
}

/**
 * @brief get all label tlv.
 * 
 * @return const LdpRawTlv* tlv.
 */
const LdpRawTlv* LdpLabelWithdrawMessageBody::getLabelTlv() const {
    return _label;
}

/**
 * @brief parse a withdraw message.
 * 
 * @param from source buffer - must start after message id field.
 * @param msg_sz msg size from the msg hd minus the size of msg id field.
 * @return ssize_t bytes read, or -1 on error.
 */
ssize_t LdpLabelWithdrawMessageBody::parse(const uint8_t *from, size_t msg_sz) {
    const uint8_t *ptr = from;
    size_t buf_remaining = msg_sz;

    LdpRawTlv *fec = new LdpRawTlv();

    ssize_t ret = fec->parse(ptr, buf_remaining);

    if (ret < 0) {
        delete fec;
        return -1;
    }

    buf_remaining -= ret;
    ptr += ret;

    this->setFecTlv(fec);

    LdpRawTlv *lbl = new LdpRawTlv();
    ret = lbl->parse(ptr, buf_remaining);

    if (ret < 0) {
        delete lbl;
        return -1;
    }

    this->setLabelTlv(lbl);

    buf_remaining -= ret;
    ptr += ret;

    if (buf_remaining != 0) {
        log_fatal("reached the end but buf not end (%zu over).\n", buf_remaining);
        return -1;
    }

    return ptr - from;
}


/**
 * @brief write message body - not incl msg id.
 * 
 * @param to dst buffer
 * @param buf_sz dst buffer size.
 * @return ssize_t bytes written, or -1 on error.
 */
ssize_t LdpLabelWithdrawMessageBody::write(uint8_t *to, size_t buf_sz) const {
    uint8_t *ptr = to;
    size_t buf_remaining = buf_sz;

    if (_fec == nullptr) {
        log_fatal("fec not set - not valid.\n");
        return -1;
    }

    ssize_t ret = _fec->write(to, buf_remaining);

    if (ret < 0) {
        return -1;
    }

    buf_remaining -= ret;
    ptr += ret;

    if (_label == nullptr) {
        return ptr - to;
    }

    ret = _label->write(to, buf_remaining);

    if (ret < 0) {
        return -1;
    }

    buf_remaining -= ret;
    ptr += ret;

    return ptr - to;
}

/**
 * @brief get length of the body - no incl. msg id.
 * 
 * @return size_t length.
 */
size_t LdpLabelWithdrawMessageBody::length() const {
    size_t len = 0;

    if (_fec != nullptr) {
        len += _fec->length();
    }

    if (_label != nullptr) {
        len += _label->length();
    }

    return len;
}

}