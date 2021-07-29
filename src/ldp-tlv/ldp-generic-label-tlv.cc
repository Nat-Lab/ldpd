#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-generic-label-tlv.hh"

namespace ldpd {

LdpGenericLabelTlv::LdpGenericLabelTlv() {
    _label = 0;
}

/**
 * @brief get type of generic label tlv in host byte order.
 * 
 * @return uint16_t type - always 0x0200.
 */
uint16_t LdpGenericLabelTlv::getType() const {
    return 0x0200;
}

/**
 * @brief get label of this tlv in host byte order.
 * 
 * @return uint32_t label - should be treated as a 20 bits value.
 */
uint32_t LdpGenericLabelTlv::getLabel() const {
    return _label;
}

/**
 * @brief set label of this tlv.
 * 
 * @param label lable in host byte order. will be treated as 20 bit value on
 * write().
 */
void LdpGenericLabelTlv::setLabel(uint32_t label) {
    _label = label;
}

ssize_t LdpGenericLabelTlv::parse(const uint8_t *from, size_t buf_sz) {
    if (buf_sz < sizeof(uint32_t)) {
        log_fatal("buf_sz (%zu) too small (generic lbl tlv value must be size %zu), packet truncated?\n", buf_sz, sizeof(uint32_t));
        return -1;
    }

    GETVAL_S(from, buf_sz, uint32_t, _label, );

    return sizeof(uint32_t);
}

ssize_t LdpGenericLabelTlv::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < sizeof(uint32_t)) {
        log_fatal("buf_sz (%zu) too small to fit (generic lbl tlv value must be size %zu).\n", buf_sz, sizeof(uint32_t));
        return -1;
    }

    PUTVAL_S(to, buf_sz, uint32_t, _label, );

    return sizeof(uint32_t);
}

size_t LdpGenericLabelTlv::length() const {
    return sizeof(uint32_t);
}

}