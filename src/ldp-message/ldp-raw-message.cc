#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-message/ldp-message.hh"

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define NEED_MIN_BUFSZ(sz, fail_value) {\
    if (_raw_buffer == nullptr) { return fail_value; }\
    if (_raw_buffer_size < (sz)) { return fail_value; }\
}

namespace ldpd {
    
LdpRawMessage::LdpRawMessage() {
    _raw_buffer = nullptr;
    _raw_buffer_size = 0;
}

LdpRawMessage::~LdpRawMessage() {
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
bool LdpRawMessage::unknown() const {
    NEED_MIN_BUFSZ(1, false);

    return _raw_buffer[0] & 0b10000000;
}

/**
 * @brief get type field in the message.
 * 
 * @return uint16_t type field in host byte order.
 */
uint16_t LdpRawMessage::getType() const {
    NEED_MIN_BUFSZ(sizeof(uint16_t), 0);

    return ntohs(((uint16_t *) _raw_buffer)[0]);
}

/**
 * @brief get length field in the message.
 * 
 * @return uint16_t length field in host byte order.
 */
uint16_t LdpRawMessage::getLength() const {
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
const uint8_t* LdpRawMessage::peekRawBody() const {
    return _raw_buffer;
}

/**
 * @brief set type field in the message.
 * 
 * @param type type value in the host byte order.
 * 
 * @return ssize_t bytes changed, or -1 on error.
 */
ssize_t LdpRawMessage::setType(uint16_t type) {
    NEED_MIN_BUFSZ(sizeof(uint16_t), -1);

    ((uint16_t *) _raw_buffer)[0] = htons(type);

    return sizeof(uint16_t);
}

/**
 * @brief set length field in the message.
 * 
 * @param length length value in the host byte order.
 * 
 * @return ssize_t bytes changed, or -1 on error.
 */
ssize_t LdpRawMessage::setLength(uint16_t length) {
    NEED_MIN_BUFSZ(2 * sizeof(uint16_t), -1);

    ((uint16_t *) _raw_buffer)[1] = htons(length);

    return sizeof(uint16_t);
}

/**
 * @brief set the raw value buffer.
 * 
 * note: this uses memcpy. if you try to setRawValue with a previously returned
 * raw buffer from the same message, you may have problem (memcpy w/ overlapping
 * buffer).
 * 
 * note: this automatically extend/allocates internal buffer. old pointers
 * got from getRawValue may no longer valid.
 * 
 * note: this sets only the value part of the message. it also updates the
 * length field in the message.
 * 
 * @param size source buffer size.
 * @param src source buffer.
 * 
 * @return new body size.
 */
ssize_t LdpRawMessage::setRawBody(size_t size, const uint8_t *src) {
    size_t msg_hdr_sz = 2 * sizeof(uint16_t);

    _raw_buffer_size = msg_hdr_sz + size;

    if (_raw_buffer == nullptr) {
        _raw_buffer = (uint8_t *) malloc(_raw_buffer_size);
    }

    if (_raw_buffer != nullptr) {
        _raw_buffer = (uint8_t *) realloc(_raw_buffer, _raw_buffer_size);
    }

    memcpy(_raw_buffer + msg_hdr_sz, src, size);
    this->setLength(size);

    return size;
}

// ----------------------------------------------------------------------------

/**
 * @brief "parse" a message into raw, un-parsed buffer.
 * 
 * note: this only read the length field and read that many bytes plus the
 * header size into a buffer. it does not do additional parsing. use
 * getParsedValue to parse the value.
 * 
 * @param from source buffer.
 * @param sz source buffer size.
 * @return ssize_t bytes read, or -1 on error.
 */
ssize_t LdpRawMessage::parse(const uint8_t *from, size_t sz) {
    const uint8_t *buffer = from;
    size_t buf_remaining = sz;

    uint16_t type, msg_len;

    GETVAL_S(buffer, buf_remaining, uint16_t, type, ntohs, -1);
    GETVAL_S(buffer, buf_remaining, uint16_t, msg_len, ntohs, -1);

    if (msg_len > buf_remaining) {
        log_fatal("msg_len (%zu) greater then remaining buffer (%zu), packet truncated?\n", msg_len, buf_remaining);
        return -1;
    }

    size_t tot_msg_len = msg_len + sizeof(type) + sizeof(msg_len);

    if (_raw_buffer != nullptr) {
        free(_raw_buffer);
    }

    _raw_buffer = (uint8_t *) malloc(tot_msg_len);

    memcpy(_raw_buffer, from, tot_msg_len);
    _raw_buffer_size = tot_msg_len;
    
    return tot_msg_len;
}

/**
 * @brief write message into buffer.
 * 
 * @param to dst buffer.
 * @param buf_sz dst buffer size.
 * @return ssize_t bytes written, or -1 on error.
 */
ssize_t LdpRawMessage::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < _raw_buffer_size) {
        log_fatal("buf_sz (%zu) too small - can not fit message (size is %zu)\n", buf_sz, _raw_buffer_size);
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
size_t LdpRawMessage::length() const {
    return _raw_buffer_size;
}

}