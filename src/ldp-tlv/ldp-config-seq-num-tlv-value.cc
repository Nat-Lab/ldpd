#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-tlv-types.hh"
#include "ldp-tlv/ldp-config-seq-num-tlv-value.hh"

#include <arpa/inet.h>

namespace ldpd {

LdpConfigSeqNumTlvValue::LdpConfigSeqNumTlvValue() {
    _seq = 0;
}

LdpConfigSeqNumTlvValue::~LdpConfigSeqNumTlvValue() {

}

uint16_t LdpConfigSeqNumTlvValue::getType() const {
    return LDP_TLVTYPE_CONFIGURATION_SEQ;
}

uint32_t LdpConfigSeqNumTlvValue::getSeq() const {
    return _seq;
}

ssize_t LdpConfigSeqNumTlvValue::setSeq(uint32_t seq) {
    _seq = seq;

    return sizeof(_seq);
}

ssize_t LdpConfigSeqNumTlvValue::parse(const uint8_t *from, size_t tlv_sz) {
    if (tlv_sz != this->length()) {
        log_fatal("bad length. need len %zu, but got %zu.\n", this->length(), tlv_sz);
        return -1;
    }

    const uint8_t *ptr = from;
    size_t buf_remaining = tlv_sz;

    GETVAL_S(ptr, buf_remaining, uint32_t, _seq, ntohl, -1);

    return ptr - from;
}

ssize_t LdpConfigSeqNumTlvValue::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < this->length()) {
        log_fatal("buf too small, can not write.\n");
        return -1;
    }

    uint8_t *ptr = to;
    size_t buf_remaining = buf_sz;

    PUTVAL_S(ptr, buf_remaining, uint32_t, _seq, htonl, -1);

    return ptr - to;
}

size_t LdpConfigSeqNumTlvValue::length() const {
    return sizeof(_seq);
}

}