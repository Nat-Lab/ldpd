#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-fec-prefix-element.hh"
#include "ldp-tlv/ldp-fec-wildcard-element.hh"
#include "ldp-tlv/ldp-fec-tlv-value.hh"

namespace ldpd {

LdpFecTlvValue::LdpFecTlvValue() : _elements() {

}

LdpFecTlvValue::~LdpFecTlvValue() {
    this->clearElements();
}

uint16_t LdpFecTlvValue::getType() const {
    return 0x0100;
}

const std::vector<LdpFecElement *> LdpFecTlvValue::getElements() const {
    return _elements;
}

void LdpFecTlvValue::clearElements() {
    for (LdpFecElement *el : _elements) {
        delete el;
    }

    _elements.clear();
}

ssize_t LdpFecTlvValue::addElement(LdpFecElement *element) {
    _elements.push_back(element);

    return element->length();
}

ssize_t LdpFecTlvValue::parse(const uint8_t *from, size_t tlv_len) {
    if (tlv_len < 1) {
        log_fatal("tlv_len (%zu) is not correct (fec tlv value must be > 1 byte)\n", tlv_len);
        return -1;
    }

    size_t buf_remaining = tlv_len;
    const uint8_t *ptr = from;

    while (buf_remaining > 0) {
        uint8_t type;
        GETVAL_S(ptr, buf_remaining, uint8_t, type, , -1);

        LdpFecElement *el = nullptr;

        switch (type) {
            case 0x01: {
                el = (LdpFecElement *) new LdpFecWildcardElement();
                break;
            }
            case 0x02: {
                el = (LdpFecElement *) new LdpFecPrefixElement();
                break;
            }
            default:
                log_fatal("unknow fec element type (0x%.2x)\n", type);
                return -1;
        }

        if (el == nullptr) {
            log_fatal("no fec parser selected? type is 0x%.2x\n", type);
            return -1;
        }

        PARSE_S(ptr, buf_remaining, el, -1);
    }

    if (buf_remaining != 0) {
        log_fatal("reached the end but buf not end (%zu over).\n", buf_remaining);
        return -1;
    }

    return ptr - from;
}

ssize_t LdpFecTlvValue::write(uint8_t *to, size_t buf_sz) const {
    size_t len = this->length();

    if (buf_sz < 1) {
        log_fatal("buf_sz (%zu) too small (need %zu)\n", buf_sz, len);
        return -1;
    }

    size_t buf_remaining = buf_sz;
    uint8_t *ptr = to;

    for (LdpFecElement *el : _elements) {
        PUTVAL_S(ptr, buf_remaining, uint8_t, el->getType(), , -1);
        WRITE_S(ptr, buf_remaining, el, -1);
    }

    return len;
}

size_t LdpFecTlvValue::length() const {
    size_t len = 0;

    for (LdpFecElement *el : _elements) {
        len += sizeof(uint8_t) + el->length();
    }

    return len;
}

}