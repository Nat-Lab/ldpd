#include "ldp-tlv/ldp-tlv.hh"

#include <stdlib.h>
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
 * @brief get type field in host byte order.
 * 
 * @return uint16_t 
 */
uint16_t LdpRawTlv::getType() const {
    NEED_MIN_BUFSZ(sizeof(uint16_t), 0);

    return ntohs(((uint16_t *) _raw_buffer)[0]);
}

/**
 * @brief get length field in host byte order.
 * 
 * @return uint16_t 
 */
uint16_t LdpRawTlv::getLength() const {
    NEED_MIN_BUFSZ(2 * sizeof(uint16_t), 0);

    return ntohs(((uint16_t *) _raw_buffer)[1]);
}



}