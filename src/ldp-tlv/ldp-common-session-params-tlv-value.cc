#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-tlv-types.hh"
#include "ldp-tlv/ldp-common-session-params-tlv-value.hh"
#include "ldp-pdu/ldp-pdu.hh"

#include <arpa/inet.h>

namespace ldpd {

LdpCommonSessionParamsTlvValue::LdpCommonSessionParamsTlvValue() {
    _protocolVersion = LDP_VERSION;
    _keepaliveTime = 0;
    _pathVectorLimit = 0;
    _maxPduLength = 0;
    _receiverRouterId = 0;
    _receiverLabelSpace = 0;

    _loopDetection = false;
}

LdpCommonSessionParamsTlvValue::~LdpCommonSessionParamsTlvValue() {

}

uint16_t LdpCommonSessionParamsTlvValue::getType() const {
    return LDP_TLVTYPE_COMMON_SESSION;
}

uint16_t LdpCommonSessionParamsTlvValue::getProtocolVersion() const {
    return _protocolVersion;
}

uint16_t LdpCommonSessionParamsTlvValue::getKeepaliveTime() const {
    return _keepaliveTime;
}

uint16_t LdpCommonSessionParamsTlvValue::getPathVectorLimit() const {
    return _pathVectorLimit;
}

uint16_t LdpCommonSessionParamsTlvValue::getMaxPduLength() const {
    return _maxPduLength;
}

uint32_t LdpCommonSessionParamsTlvValue::getReceiverRouterId() const {
    return _receiverRouterId;
}

uint16_t LdpCommonSessionParamsTlvValue::getReceiverLabelSpace() const {
    return _receiverLabelSpace;
}

ssize_t LdpCommonSessionParamsTlvValue::setProtocolVersion(uint16_t protocolVersion) {
    _protocolVersion = protocolVersion;

    return sizeof(_protocolVersion);
}

ssize_t LdpCommonSessionParamsTlvValue::setKeepaliveTime(uint16_t keepaliveTime) {
    _keepaliveTime = keepaliveTime;

    return sizeof(_keepaliveTime);
}

ssize_t LdpCommonSessionParamsTlvValue::setPathVectorLimit(uint16_t pathVectorLimit) {
    _pathVectorLimit = pathVectorLimit;

    return sizeof(_pathVectorLimit);
}

ssize_t LdpCommonSessionParamsTlvValue::setMaxPduLength(uint16_t maxPduLength) {
    _maxPduLength = maxPduLength;

    return sizeof(_maxPduLength);
}

ssize_t LdpCommonSessionParamsTlvValue::setReceiverRouterId(uint32_t receiverRouterId) {
    _receiverRouterId = receiverRouterId;

    return sizeof(_receiverRouterId);
}

ssize_t LdpCommonSessionParamsTlvValue::setReceiverLabelSpace(uint16_t receiverLabelSpace) {
    _receiverLabelSpace = receiverLabelSpace;

    return sizeof(_receiverLabelSpace);
}

bool LdpCommonSessionParamsTlvValue::loopDetection() const {
    return _loopDetection;
}

ssize_t LdpCommonSessionParamsTlvValue::setloopDetection(bool loopDetection) {
    _loopDetection = loopDetection;

    return sizeof(_pathVectorLimit);
}

const char* LdpCommonSessionParamsTlvValue::getReceiverRouterIdString() const {
    return inet_ntoa(*((in_addr *) &_receiverRouterId));
}

ssize_t LdpCommonSessionParamsTlvValue::setReceiverRouterIdString(const char* id) {
    if (inet_pton(AF_INET, id, &_receiverRouterId) != 1) {
        return -1;
    }

    return sizeof(_receiverRouterId);
}

ssize_t LdpCommonSessionParamsTlvValue::parse(const uint8_t *from, size_t tlv_sz) {
    if (tlv_sz != this->length()) {
        log_fatal("bad length. need len %zu, but got %zu.\n", this->length(), tlv_sz);
        return -1;
    }

    const uint8_t *ptr = from;
    size_t buf_remaining = tlv_sz;

    GETVAL_S(ptr, buf_remaining, uint16_t, _protocolVersion, ntohs, -1);
    GETVAL_S(ptr, buf_remaining, uint16_t, _keepaliveTime, ntohs, -1);
    GETVAL_S(ptr, buf_remaining, uint16_t, _pathVectorLimit, ntohs, -1);

    if (_pathVectorLimit & 0b1000000000000000) {
        log_warn("downstream-on-demand bit set. note that atm/frame relay is not supported.\n");
    }

    _loopDetection = _pathVectorLimit & 0b0100000000000000;

    _pathVectorLimit &= 0b0000000011111111;

    GETVAL_S(ptr, buf_remaining, uint16_t, _maxPduLength, ntohs, -1);
    GETVAL_S(ptr, buf_remaining, uint32_t, _receiverRouterId, , -1);
    GETVAL_S(ptr, buf_remaining, uint16_t, _receiverLabelSpace, ntohs, -1);

    return ptr - from;
}

ssize_t LdpCommonSessionParamsTlvValue::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < this->length()) {
        log_fatal("buf too small, can not write.\n");
        return -1;
    }

    uint8_t *ptr = to;
    size_t buf_remaining = buf_sz;

    PUTVAL_S(ptr, buf_remaining, uint16_t, _protocolVersion, htons, -1);
    PUTVAL_S(ptr, buf_remaining, uint16_t, _keepaliveTime, htons, -1);

    uint16_t pvlim = _pathVectorLimit & 0b0000000011111111;

    if (_loopDetection) {
        pvlim |= 0b0100000000000000;
    }

    PUTVAL_S(ptr, buf_remaining, uint16_t, pvlim, htons, -1);
    PUTVAL_S(ptr, buf_remaining, uint16_t, _maxPduLength, htons, -1);
    PUTVAL_S(ptr, buf_remaining, uint32_t, _receiverRouterId, , -1);
    PUTVAL_S(ptr, buf_remaining, uint16_t, _receiverLabelSpace, htons, -1);

    return ptr - to;
}

size_t LdpCommonSessionParamsTlvValue::length() const {
    return sizeof(_protocolVersion) + sizeof(_keepaliveTime) + sizeof(_pathVectorLimit) + sizeof(_maxPduLength) + sizeof(_receiverRouterId) + sizeof(_receiverLabelSpace);
}

}