#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-tlv-types.hh"
#include "ldp-tlv/ldp-common-hello-params-tlv-value.hh"

#include <arpa/inet.h>

namespace ldpd {

LdpCommonHelloParamsTlvValue::LdpCommonHelloParamsTlvValue() {
    _holdTime = 0;
    _targeted = false;
    _requestTargeted = false;
}

LdpCommonHelloParamsTlvValue::~LdpCommonHelloParamsTlvValue() {

}

uint16_t LdpCommonHelloParamsTlvValue::getType() const {
    return LDP_TLVTYPE_COMMON_HELLO;
}

uint16_t LdpCommonHelloParamsTlvValue::getHoldTime() const {
    return _holdTime;
}

bool LdpCommonHelloParamsTlvValue::targeted() const {
    return _targeted;
}

bool LdpCommonHelloParamsTlvValue::requestTargeted() const {
    return _requestTargeted;
}

ssize_t LdpCommonHelloParamsTlvValue::setHelloTime(uint16_t holdTime) {
    _holdTime = holdTime;

    return sizeof(_holdTime);
}

ssize_t LdpCommonHelloParamsTlvValue::setTargeted(bool targeted) {
    _targeted = targeted;

    return sizeof(uint16_t);
}

ssize_t LdpCommonHelloParamsTlvValue::setRequestTargeted(bool requestTargeted) {
    _requestTargeted = requestTargeted;

    return sizeof(uint16_t);
}

ssize_t LdpCommonHelloParamsTlvValue::parse(const uint8_t *from, size_t tlv_sz) {
    if (tlv_sz != this->length()) {
        log_fatal("bad length. common-hello-params-tlv must be len %zu, but got %zu.\n", this->length(), tlv_sz);
        return -1;
    }

    const uint8_t *ptr = from;
    size_t buf_remaining = tlv_sz;

    GETVAL_S(ptr, buf_remaining, uint16_t, _holdTime, ntohs, -1);

    uint16_t bitmap;
    GETVAL_S(ptr, buf_remaining, uint16_t, bitmap, ntohs, -1);

    _targeted = bitmap & 0b1000000000000000;
    _requestTargeted = bitmap & 0b0100000000000000;

    return ptr - from;
}

ssize_t LdpCommonHelloParamsTlvValue::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < this->length()) {
        log_fatal("buf too small, can not write.\n");
        return -1;
    }

    uint8_t *ptr = to;
    size_t buf_remaining = buf_sz;

    PUTVAL_S(ptr, buf_remaining, uint16_t, _holdTime, htons, -1);

    uint16_t bitmap = 0;

    if (_targeted) {
        bitmap |= 0b1000000000000000;
    }

    if (_requestTargeted) {
        bitmap |= 0b0100000000000000;
    }

    PUTVAL_S(ptr, buf_remaining, uint16_t, bitmap, htons, -1);

    return ptr - to;
}

size_t LdpCommonHelloParamsTlvValue::length() const {
    return sizeof(_holdTime) + sizeof(uint16_t);
}

}

