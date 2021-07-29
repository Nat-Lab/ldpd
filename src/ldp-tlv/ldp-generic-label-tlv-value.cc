#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-generic-label-tlv-value.hh"

namespace ldpd {

LdpGenericLabelTlvValue::LdpGenericLabelTlvValue() {
    _label = 0;
}

/**
 * @brief get type of generic label tlv in host byte order.
 * 
 * @return uint16_t type - always 0x0200.
 */
uint16_t LdpGenericLabelTlvValue::getType() const {
    return 0x0200;
}

/**
 * @brief get label of this tlv in host byte order.
 * 
 * @return uint32_t label - should be treated as a 20 bits value.
 */
uint32_t LdpGenericLabelTlvValue::getLabel() const {
    return _label;
}

/**
 * @brief set label of this tlv.
 * 
 * @param label lable in host byte order. will be treated as 20 bit value on
 * write().
 */
void LdpGenericLabelTlvValue::setLabel(uint32_t label) {
    _label = label;
}

/**
 * @brief get raw value buffer.
 * 
 * @return const uint8_t* raw value buffer.
 */
const uint8_t* LdpGenericLabelTlvValue::getRaw() const {
    return (uint8_t *) &_label;
}

/**
 * @brief parse tlv value.
 * 
 * @param from source buffer.
 * @param tlv_len length of the tlv from the len field - not the size of source
 * buffer.
 * @return ssize_t bytes read, or -1 on error.
 */
ssize_t LdpGenericLabelTlvValue::parse(const uint8_t *from, size_t tlv_len) {
    if (tlv_len != sizeof(uint32_t)) {
        log_fatal("tlv_len (%zu) is not correct (generic lbl tlv value must be size %zu)\n", tlv_len, sizeof(uint32_t));
        return -1;
    }

    GETVAL_S(from, tlv_len, uint32_t, _label, );

    return sizeof(uint32_t);
}

/**
 * @brief write value to buffer.
 * 
 * @param to dest buffer.
 * @param buf_sz size of buffer.
 * @return ssize_t bytes written, or -1 on error.
 */
ssize_t LdpGenericLabelTlvValue::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < sizeof(uint32_t)) {
        log_fatal("buf_sz (%zu) too small to fit (generic lbl tlv value must be size %zu).\n", buf_sz, sizeof(uint32_t));
        return -1;
    }

    PUTVAL_S(to, buf_sz, uint32_t, _label, );

    return sizeof(uint32_t);
}

size_t LdpGenericLabelTlvValue::length() const {
    return sizeof(uint32_t);
}

}