#include "ldp-tlv/ldp-tlv.hh"

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define NEED_MIN_BUFSZ(sz, fail_value) {\
    if (_raw_buffer == nullptr) { return fail_value; }\
    if (_raw_buffer_size < (sz)) { return fail_value; }\
}

namespace ldpd {
    
LdpRawTlv::LdpRawTlv() {
    _raw_buffer = nullptr;
    _raw_buffer_size = 0;
}

LdpRawTlv::~LdpRawTlv() {
    if (_raw_buffer != nullptr) {
        free(_raw_buffer);
    }
}

/**
 * @brief test if the unknown bit is set in the type field.
 * 
 * @return true if set.
 * @return false if not set.
 */
bool LdpRawTlv::unknown() const {
    NEED_MIN_BUFSZ(1, false);

    return _raw_buffer[0] & 0b10000000;
}

/**
 * @brief test if the foward-unknown bit is set in the type field.
 * 
 * @return true if set.
 * @return false if not set.
 */
bool LdpRawTlv::forwardUnknown() const {
    NEED_MIN_BUFSZ(1, false);

    return _raw_buffer[0] & 0b01000000;
}

/**
 * @brief get type field in the tlv.
 * 
 * @return uint16_t type field in host byte order.
 */
uint16_t LdpRawTlv::getType() const {
    NEED_MIN_BUFSZ(sizeof(uint16_t), 0);

    return ntohs(((uint16_t *) _raw_buffer)[0]);
}

/**
 * @brief get length field in the tlv.
 * 
 * @return uint16_t length field in host byte order.
 */
uint16_t LdpRawTlv::getLength() const {
    NEED_MIN_BUFSZ(2 * sizeof(uint16_t), 0);

    return ntohs(((uint16_t *) _raw_buffer)[1]);
}

/**
 * @brief get raw value buffer.
 * 
 * @return const uint8_t* value buffer. can be nullptr if not set.
 */
const uint8_t* LdpRawTlv::getRawValue() const {
    return _raw_buffer;
}

/**
 * @brief get raw value buffer size.
 * 
 * @return size_t buffer size.
 */
size_t LdpRawTlv::getRawValueSize() const {
    return _raw_buffer_size;
}

/**
 * @brief set type field in the tlv.
 * 
 * @param type type value in the host byte order.
 * 
 * @return ssize_t bytes changed, or -1 on error.
 */
ssize_t LdpRawTlv::setType(uint16_t type) {
    NEED_MIN_BUFSZ(sizeof(uint16_t), -1);

    ((uint16_t *) _raw_buffer)[0] = htons(type);
}

/**
 * @brief set length field in the tlv.
 * 
 * @param length length value in the host byte order.
 * 
 * @return ssize_t bytes changed, or -1 on error.
 */
ssize_t LdpRawTlv::setType(uint16_t length) {
    NEED_MIN_BUFSZ(2 * sizeof(uint16_t), -1);

    ((uint16_t *) _raw_buffer)[1] = htons(length);
}

/**
 * @brief set the raw value buffer.
 * 
 * note: this frees the internal buffer first. If you try to setRawValue with a
 * previously returned raw buffer from the same tlv, you will have problem.
 * 
 * @param size source buffer size.
 * @param src source buffer.
 */
void LdpRawTlv::setRawValue(size_t size, const uint8_t *src) {
    if (_raw_buffer != nullptr) {
        free(_raw_buffer);
    }

    _raw_buffer = (uint8_t *) malloc(size);
    memcpy(_raw_buffer, src, size);
}

}