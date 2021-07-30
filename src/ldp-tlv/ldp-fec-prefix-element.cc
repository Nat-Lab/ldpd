#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-fec-prefix-element.hh"

#include <arpa/inet.h>

namespace ldpd {

LdpFecPrefixElement::LdpFecPrefixElement() {
    _prefix = 0;
    _prelen = 0;
}    

uint8_t LdpFecPrefixElement::getType() const {
    return 0x02;
}

uint32_t LdpFecPrefixElement::getPrefix() const {
    return _prefix;
}

uint8_t LdpFecPrefixElement::getPrefixLength() const {
    return _prelen;
}

ssize_t LdpFecPrefixElement::setPrefix(uint32_t prefix) {
    _prefix = prefix;

    return sizeof(_prefix);
}

ssize_t LdpFecPrefixElement::setPrefixLength(uint8_t length) {
    _prelen = length;

    return sizeof(_prelen);
}

const char* LdpFecPrefixElement::getPrefixString() const {
    return inet_ntoa(*((in_addr *) &_prefix));
}

ssize_t LdpFecPrefixElement::setPrefixString(const char *prefix) {
if (inet_pton(AF_INET, prefix, &_prefix) != 1) {
        return -1;
    }

    return sizeof(_prefix);
}

ssize_t LdpFecPrefixElement::parse(const uint8_t *from, size_t buf_sz) {
    size_t min_ele_sz = sizeof(_prelen) + sizeof(uint16_t);

    if (buf_sz < min_ele_sz) {
        log_fatal("buf_sz (%zu) smaller then ele_sz (%zu), packet truncated?\n", buf_sz, min_ele_sz);
        return -1;
    }

    size_t buf_remaining = buf_sz;
    const uint8_t *ptr = from;

    uint16_t af;

    GETVAL_S(ptr, buf_remaining, uint16_t, af, ntohs, -1);

    if (af != 1) {
        log_fatal("unknow af: %u\n", af);
        return -1;
    }

    GETVAL_S(ptr, buf_remaining, uint8_t, _prelen, , -1);

    size_t prefix_buf_len = (_prelen + 7) / 8;

    if (buf_remaining < prefix_buf_len) {
        log_fatal("buf_remaining (%zu) smaller then prefix_buf_len (%zu), packet truncated?\n", buf_remaining, prefix_buf_len);
        return -1;
    }

    _prefix = 0;
    memcpy(&_prefix, ptr, prefix_buf_len);

    ptr += prefix_buf_len;
    buf_remaining -= prefix_buf_len;

    return ptr - from;
}

ssize_t LdpFecPrefixElement::write(uint8_t *to, size_t buf_sz) const {
    size_t prefix_buf_len = (_prelen + 7) / 8;
    size_t ele_len = prefix_buf_len + sizeof(uint16_t) + sizeof(uint8_t);

    if (buf_sz < ele_len) {
        log_fatal("buf_sz (%zu) too small - want (%zu)\n", buf_sz, ele_len);
        return -1;
    }

    size_t buf_remaining = buf_sz;
    uint8_t *ptr = to;

    PUTVAL_S(ptr, buf_remaining, uint16_t, 1, htons, -1);
    PUTVAL_S(ptr, buf_remaining, uint8_t, _prelen, , -1);

    memcpy(ptr, &_prefix, prefix_buf_len);

    return ele_len;
}

size_t LdpFecPrefixElement::length() const {
    size_t prefix_buf_len = (_prelen + 7) / 8;
    size_t ele_len = prefix_buf_len + sizeof(uint16_t) + sizeof(uint8_t);

    return ele_len;
}

}