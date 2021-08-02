#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-tlv-types.hh"
#include "ldp-tlv/ldp-status-tlv-value.hh"

#include <arpa/inet.h>

namespace ldpd {

const char *StatusCodeText[] = {
    "Success",
    "Bad LDP Identifier",
    "Bad Protocol Version",
    "Bad PDU Length",
    "Unknown Message Type",
    "Bad Message Length", 
    "Unknown TLV",
    "Bad TLV Length",
    "Malformed TLV Value",
    "Hold Timer Expired",
    "Shutdown",
    "Loop Detected",
    "Unknown FEC",
    "No Route",
    "No Lable Resources",
    "Label Resources / Available",
    "Session Rejected / No Hello",
    "Session Rejected / Parameters Advertisement Mode",
    "Session Rejected / Parameters Max PDU Length",
    "Session Rejected / Parameters Label Range",
    "KeepAlive Timer Expired",
    "Label Request Aborted",
    "Missing Message Parameters",
    "Unsupported Address Family",
    "Session Rejected / Bad KeepAlive Time",
    "Internal Error"
};

const bool StatusCodeFatalBit[] = {
    false, true, true, true, false, true, false, true, true, true, true, false,
    false, false, false, false, true, true, true, true, true, false, false, false, 
    true, true
};

LdpStatusTlvValue::LdpStatusTlvValue() {
    _statusCode = 0;
    _messageId = 0;
    _messageType = 0;
    _fatal = false;
    _forward = false;
}

LdpStatusTlvValue::~LdpStatusTlvValue() {

}

uint16_t LdpStatusTlvValue::getType() const {
    return LDP_TLVTYPE_STATUS;
}

uint32_t LdpStatusTlvValue::getStatusCode() const {
    return _statusCode;
}

uint32_t LdpStatusTlvValue::getMessageId() const {
    return _messageId;
}

uint16_t LdpStatusTlvValue::getMessageType() const {
    return _messageType;
}

ssize_t LdpStatusTlvValue::setStatusCode(uint32_t statusCode) {
    _statusCode = statusCode;

    if (statusCode < sizeof(StatusCodeFatalBit)) {
        _fatal = StatusCodeFatalBit[statusCode];
    }

    return sizeof(_statusCode);
}

ssize_t LdpStatusTlvValue::setMessageId(uint32_t messageId) {
    _messageId = messageId;

    return sizeof(_messageId);
}

ssize_t LdpStatusTlvValue::setMessageType(uint16_t messageType) {
    _messageType = messageType;

    return sizeof(_messageType);
}

bool LdpStatusTlvValue::fatal() const {
    return _fatal;
}


bool LdpStatusTlvValue::forward() const {
    return _forward;
}

ssize_t LdpStatusTlvValue::setFatal(bool fatal) {
    _fatal = fatal;

    return sizeof(_statusCode);
}

ssize_t LdpStatusTlvValue::setForward(bool forward) {
    _forward = forward;

    return sizeof(_statusCode);
}

const char* LdpStatusTlvValue::getStatusCodeText() const {
    if (sizeof(StatusCodeText) < _statusCode) {
        return "Unknow status";
    }

    return StatusCodeText[_statusCode];
}

ssize_t LdpStatusTlvValue::parse(const uint8_t *from, size_t tlv_sz) {
    if (tlv_sz != this->length()) {
        log_fatal("bad length. need len %zu, but got %zu.\n", this->length(), tlv_sz);
        return -1;
    }

    const uint8_t *ptr = from;
    size_t buf_remaining = tlv_sz;

    GETVAL_S(ptr, buf_remaining, uint32_t, _statusCode, ntohl, -1);

    if (_statusCode & 0b10000000000000000000000000000000) {
        _fatal = true;
    }

    if (_statusCode & 0b01000000000000000000000000000000) {
        _forward = true;
    }

    _statusCode &= 0b00111111111111111111111111111111;

    if (_statusCode < sizeof(StatusCodeFatalBit)) {
        if (StatusCodeFatalBit[_statusCode] != _fatal) {
            log_warn("status code %u (%s) shoud%s have fatal bit set.\n", _statusCode, StatusCodeText[_statusCode], StatusCodeFatalBit[_statusCode] ? "" : " not");
        }
    }

    GETVAL_S(ptr, buf_remaining, uint32_t, _messageId, ntohl, -1);
    GETVAL_S(ptr, buf_remaining, uint16_t, _messageType, ntohs, -1);

    return ptr - from;
}

ssize_t LdpStatusTlvValue::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < this->length()) {
        log_fatal("buf too small, can not write.\n");
        return -1;
    }

    uint8_t *ptr = to;
    size_t buf_remaining = buf_sz;

    uint32_t code = _statusCode;

    if (_fatal) {
        code |= 0b10000000000000000000000000000000;
    }

    if (_forward) {
        code |= 0b01000000000000000000000000000000;
    }

    PUTVAL_S(ptr, buf_remaining, uint32_t, code, htonl, -1);
    PUTVAL_S(ptr, buf_remaining, uint32_t, _messageId, htonl, -1);
    PUTVAL_S(ptr, buf_remaining, uint16_t, _messageType, htons, -1);

    return ptr - to;
}

size_t LdpStatusTlvValue::length() const {
    return sizeof(_statusCode) + sizeof(_messageId) + sizeof(_messageType);
}

}