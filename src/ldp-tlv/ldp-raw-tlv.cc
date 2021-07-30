#include "utils/log.hh"
#include "utils/value-ops.hh"
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
 * @brief get current raw value buffer pointer.
 * 
 * note: this can return nullptr if not set.
 * 
 * note: pointer may not be valid anymore if any changes is made to the object.
 * 
 * @return const uint8_t* value buffer. 
 */
const uint8_t* LdpRawTlv::peekRawValue() const {
    return _raw_buffer;
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

    return sizeof(uint16_t);
}

/**
 * @brief set length field in the tlv.
 * 
 * @param length length value in the host byte order.
 * 
 * @return ssize_t bytes changed, or -1 on error.
 */
ssize_t LdpRawTlv::setLength(uint16_t length) {
    NEED_MIN_BUFSZ(2 * sizeof(uint16_t), -1);

    ((uint16_t *) _raw_buffer)[1] = htons(length);

    return sizeof(uint16_t);
}

/**
 * @brief set the raw value buffer.
 * 
 * note: this uses memcpy. if you try to setRawValue with a previously returned
 * raw buffer from the same tlv, you may have problem (memcpy w/ overlapping
 * buffer).
 * 
 * note: this automatically extend/allocates internal buffer. old pointers
 * got from getRawValue may no longer valid.
 * 
 * note: this sets only the value part of the tlv. it also updates the length
 * field in the tlv.
 * 
 * @param size source buffer size.
 * @param src source buffer.
 * 
 * @returns new buffer length (the entire tlv).
 */
ssize_t LdpRawTlv::setRawValue(size_t size, const uint8_t *src) {
    size_t tlv_hdr_sz = 2 * sizeof(uint16_t);

    _raw_buffer_size = tlv_hdr_sz + size;

    if (_raw_buffer == nullptr) {
        _raw_buffer = (uint8_t *) malloc(_raw_buffer_size);
    }

    if (_raw_buffer != nullptr) {
        _raw_buffer = (uint8_t *) realloc(_raw_buffer, _raw_buffer_size);
    }

    memcpy(_raw_buffer + tlv_hdr_sz, src, size);
    this->setLength(size);

    return _raw_buffer_size;
}

/**
 * @brief set value to the given value object. also updates len and type field.
 * 
 * @param value value object.
 * 
 * @return new buffer length (the entire tlv), or -1 on error.
 */
ssize_t LdpRawTlv::setValue(const LdpTlvValue *value) {
    if (_raw_buffer != nullptr) {
        free(_raw_buffer);
    }

    uint16_t val_sz = value->length();
    uint16_t val_type = value->getType();

    size_t tlv_hdr_sz = sizeof(val_sz) + sizeof(val_type);

    _raw_buffer_size = tlv_hdr_sz + value->length();

    _raw_buffer = (uint8_t *) malloc(_raw_buffer_size);

    uint8_t *ptr = _raw_buffer;
    size_t buf_remaining = _raw_buffer_size;

    PUTVAL_S(ptr, buf_remaining, uint16_t, val_type, htons, -1);
    PUTVAL_S(ptr, buf_remaining, uint16_t, val_sz, htons, -1);

    value->write(ptr, buf_remaining);

    return _raw_buffer_size;
}

/**
 * @brief get a parsed value object.
 * 
 * @return LdpTlvValue* parsed value. you must free this yourself after use.
 * return null if can't be parsed.
 */
LdpTlvValue* LdpRawTlv::getParsedValue() const {
    uint16_t type = this->getType();
    uint16_t len = this->getLength();

    size_t hdr_len = sizeof(type) + sizeof(len);
    size_t tot_len = hdr_len + len;

    if (tot_len > _raw_buffer_size) {
        log_fatal("tot_len (%zu) greater then raw buffer size (%zu), bad packet?\n", tot_len, _raw_buffer_size);
        return nullptr;
    }

    LdpTlvValue *val = nullptr;

    switch(type) {
        case 0x0100: {
            val = new LdpFecTlvValue();
            break;
        }
        case 0x0200: {
            val = new LdpGenericLabelTlvValue();
            break;
        }
        default:
            log_fatal("unknow tlv type (0x%.4x)\n", type);
            return nullptr;
    }

    if (val == nullptr) {
        log_fatal("no val parser selected? type is 0x%.4x\n", type);
        return nullptr;
    }

    uint8_t *ptr = _raw_buffer + hdr_len;

    PARSE_S(ptr, len, val, nullptr);

    return val;
}

// ----------------------------------------------------------------------------

/**
 * @brief "parse" a tlv into raw, un-parsed buffer.
 * 
 * note: this only read the length field and read that many bytes plus the
 * header size into a buffer. it does not do additional parsing. use
 * getParsedValue to parse the value.
 * 
 * @param from source buffer.
 * @param sz source buffer size.
 * @return ssize_t bytes read, or -1 on error.
 */
ssize_t LdpRawTlv::parse(const uint8_t *from, size_t sz) {
    const uint8_t *buffer = from;
    size_t buf_remaining = sz;

    uint16_t type, tlv_len;

    GETVAL_S(buffer, buf_remaining, uint16_t, type, ntohs, -1);
    GETVAL_S(buffer, buf_remaining, uint16_t, tlv_len, ntohs, -1);

    if (tlv_len > buf_remaining) {
        log_fatal("tlv_len (%zu) greater then remaining buffer (%zu), packet truncated?\n", tlv_len, buf_remaining);
        return -1;
    }

    size_t tot_tlv_len = tlv_len + sizeof(type) + sizeof(tlv_len);

    if (_raw_buffer != nullptr) {
        free(_raw_buffer);
    }

    _raw_buffer = (uint8_t *) malloc(tot_tlv_len);

    memcpy(_raw_buffer, from, tot_tlv_len);
    _raw_buffer_size = tot_tlv_len;
    
    return tot_tlv_len;
}

/**
 * @brief write tlv into buffer.
 * 
 * @param to dst buffer.
 * @param buf_sz dst buffer size.
 * @return ssize_t bytes written, or -1 on error.
 */
ssize_t LdpRawTlv::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < _raw_buffer_size) {
        log_fatal("buf_sz (%zu) too small - can not fit tlv (size is %zu)\n", buf_sz, _raw_buffer_size);
        return -1;
    }

    memcpy(to, _raw_buffer, _raw_buffer_size);

    return _raw_buffer_size;
}

/**
 * @brief get raw value buffer size.
 * 
 * @return size_t buffer size.
 */
size_t LdpRawTlv::length() const {
    return _raw_buffer_size;
}

}