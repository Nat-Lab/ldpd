#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-message/ldp-message.hh"

#include <arpa/inet.h>

namespace ldpd {

LdpMessage::LdpMessage() : _tlvs() {
    _type = 0;
    _length = 0;
    _id = 0;
    _unknown = false;
}

LdpMessage::~LdpMessage() {
    this->clearTlvs();
}

bool LdpMessage::unknown() const {
    return _unknown;
}

uint16_t LdpMessage::getType() const {
    return _type;
}

uint16_t LdpMessage::getLength() const {
    return _length;
}

uint32_t LdpMessage::getId() const {
    return _id;
}

ssize_t LdpMessage::setType(uint16_t type) {
    _type = type;

    return sizeof(_type);
}

ssize_t LdpMessage::setLength(uint16_t length) {
    _length = length;

    return sizeof(_length);
}

ssize_t LdpMessage::setId(uint32_t id) {
    _id = id;

    return sizeof(_id);
}


ssize_t LdpMessage::setUnknown(bool unknown) {
    _unknown = unknown;

    return sizeof(_type);
}

ssize_t LdpMessage::addTlv(LdpRawTlv *message) {
    _tlvs.push_back(message);

    return message->length();
}

void LdpMessage::clearTlvs() {
    for (LdpRawTlv *tlv : _tlvs) {
        delete tlv;
    }

    _tlvs.clear();
}

uint16_t LdpMessage::recalculateLength() {
    size_t len = sizeof(_id);

    for (const LdpRawTlv *tlv : _tlvs) {
        len += tlv->length();
    }

    _length = len;

    return _length;
}

const std::vector<LdpRawTlv *> LdpMessage::getTlvs() const {
    return _tlvs;
}

ssize_t LdpMessage::parse(const uint8_t *from, size_t buf_sz) {
    const uint8_t *buffer = from;
    size_t buf_remaining = buf_sz;

    GETVAL_S(buffer, buf_remaining, uint16_t, _type, ntohs, -1);
    GETVAL_S(buffer, buf_remaining, uint16_t, _length, ntohs, -1);
    GETVAL_S(buffer, buf_remaining, uint32_t, _id, ntohs, -1);

    if (_type & 0b1000000000000000) {
        _unknown = true;
        _type |= 0b0111111111111111;
    }

    size_t msg_len = _length;
    
    if (msg_len > buf_remaining) {
        log_fatal("msg_len (%zu) greater then remaining buffer (%zu), packet truncated?\n", msg_len, buf_remaining);
        return -1;
    }

    while (msg_len > 0) {
        LdpRawTlv *tlv = new LdpRawTlv();

        PARSE_S(buffer, buf_remaining, tlv, -1, true);

        this->addTlv(tlv);
    }

    return buffer - from;
}

ssize_t LdpMessage::write(uint8_t *to, size_t buf_sz) const {
    size_t len = this->length();

    size_t buf_remaining = buf_sz;
    uint8_t *ptr = to;

    if (len > buf_sz) {
        log_fatal("buf_sz (%zu) too small - can not fit msg (size is %zu)\n", buf_sz, len);
        return -1;
    }

    uint16_t type_val = _type;

    if (_unknown) {
        type_val |= 0b1000000000000000;
    }

    PUTVAL_S(ptr, buf_remaining, uint16_t, type_val, htons, -1);
    PUTVAL_S(ptr, buf_remaining, uint16_t, _length, htons, -1);
    PUTVAL_S(ptr, buf_remaining, uint32_t, _id, htons, -1);

    for (const LdpRawTlv *msg : _tlvs) {
        WRITE_S(ptr, buf_remaining, msg, -1);
    }

    return ptr - to;
}

size_t LdpMessage::length() const {
    size_t len = sizeof(_type) + sizeof(_length) + sizeof(_id);

    for (const LdpRawTlv *tlv : _tlvs) {
        len += tlv->length();
    }

    return len;
}

}