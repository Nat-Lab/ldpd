#include "utils/log.hh"
#include "ldp-message/ldp-label-mapping-message-body.hh"

#include <stdlib.h>

namespace ldpd {

LdpLabelMappingMessageBody::LdpLabelMappingMessageBody() {
    _fec = nullptr;
    _label = nullptr;
}

LdpLabelMappingMessageBody::~LdpLabelMappingMessageBody() {
    if (_fec != nullptr) {
        delete _fec;
    }

    if (_label != nullptr) {
        delete _label;
    }
}

uint16_t LdpLabelMappingMessageBody::getType() const {
    return 0x0400;
}

/**
 * @brief set fec tlv.
 * 
 * @param tlv tlv - we will handle freeing this.
 */
void LdpLabelMappingMessageBody::setFecTlv(LdpRawTlv *tlv) {
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
void LdpLabelMappingMessageBody::setLabelTlv(LdpRawTlv *tlv) {
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
const LdpRawTlv* LdpLabelMappingMessageBody::getFecTlv() const {
    return _fec;
}

/**
 * @brief get all label tlv.
 * 
 * @return const LdpRawTlv* tlv.
 */
const LdpRawTlv* LdpLabelMappingMessageBody::getLabelTlv() const {
    return _label;
}

/**
 * @brief parse a mapping message.
 * 
 * @param from source buffer - must start after message id field.
 * @param msg_sz msg size from the msg hd minus the size of msg id field.
 * @return ssize_t bytes read, or -1 on error.
 */
ssize_t LdpLabelMappingMessageBody::parse(const uint8_t *from, size_t msg_sz) {
    const uint8_t *ptr = from;
    size_t buf_remaining = msg_sz;

    LdpRawTlv *fec = new LdpRawTlv();

    PARSE_S(ptr, buf_remaining, fec, -1, true);

    this->setFecTlv(fec);

    LdpRawTlv *lbl = new LdpRawTlv();

    PARSE_S(ptr, buf_remaining, lbl, -1, true);

    this->setLabelTlv(lbl);

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
ssize_t LdpLabelMappingMessageBody::write(uint8_t *to, size_t buf_sz) const {
    uint8_t *ptr = to;
    size_t buf_remaining = buf_sz;

    if (_fec == nullptr) {
        log_fatal("fec not set - not valid.\n");
        return -1;
    }

    WRITE_S(ptr, buf_remaining, _fec, -1);

    if (_label == nullptr) {
        return ptr - to;
    }

    WRITE_S(ptr, buf_remaining, _label, -1);

    return ptr - to;
}

/**
 * @brief get length of the body - no incl. msg id.
 * 
 * @return size_t length.
 */
size_t LdpLabelMappingMessageBody::length() const {
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