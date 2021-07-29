#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-fec-wildcard-element.hh"

#include <arpa/inet.h>

namespace ldpd {

uint8_t LdpFecWildcardElement::getType() const {
    return 0x01;
}

ssize_t LdpFecWildcardElement::parse(const uint8_t *from, size_t buf_sz) {
    return 0;
}

ssize_t LdpFecWildcardElement::write(uint8_t *to, size_t buf_sz) const {
    return 0;
}

size_t LdpFecWildcardElement::length() const {
    return 0;
}

}