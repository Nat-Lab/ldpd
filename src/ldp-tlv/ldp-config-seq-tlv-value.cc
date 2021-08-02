#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-tlv-types.hh"
#include "ldp-tlv/ldp-config-seq-tlv-value.hh"

#include <arpa/inet.h>

namespace ldpd {

LdpHopCountTlvValue::LdpHopCountTlvValue() {
    _configSeq = 0;
}

LdpHopCountTlvValue::~LdpHopCountTlvValue() {

}

uint16_t LdpHopCountTlvValue::getType() const {
    return LDP_TLVTYPE_CONFIGURATION_SEQ;
}

uint32_t LdpHopCountTlvValue::getConfigSeq() const {
    return _configSeq;
}

ssize_t LdpHopCountTlvValue::setConfigSeq(uint32_t configSeq) {
    _configSeq = configSeq;

    return sizeof(_configSeq);
}

ssize_t LdpHopCountTlvValue::parse(const uint8_t *from, size_t tlv_sz) {
    if (tlv_sz != this->length()) {
        log_fatal("bad length. need len %zu, but got %zu.\n", this->length(), tlv_sz);
        return -1;
    }

    const uint8_t *ptr = from;
    size_t buf_remaining = tlv_sz;

    GETVAL_S(ptr, buf_remaining, uint32_t, _configSeq, ntohl, -1);

    return ptr - from;
}

ssize_t LdpHopCountTlvValue::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < this->length()) {
        log_fatal("buf too small, can not write.\n");
        return -1;
    }

    uint8_t *ptr = to;
    size_t buf_remaining = buf_sz;

    PUTVAL_S(ptr, buf_remaining, uint32_t, _configSeq, htonl, -1);

    return ptr - to;
}

size_t LdpHopCountTlvValue::length() const {
    return sizeof(_configSeq);
}

}