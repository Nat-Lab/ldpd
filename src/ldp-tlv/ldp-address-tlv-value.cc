#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-address-tlv-value.hh"

#include <arpa/inet.h>

namespace ldpd {

LdpAddressTlvValue::LdpAddressTlvValue() : _addresses() {

}

LdpAddressTlvValue::~LdpAddressTlvValue() {

}

uint16_t LdpAddressTlvValue::getType() const {
    return 0x0101;
}

const std::vector<uint32_t> LdpAddressTlvValue::getAddresses() const {
    return _addresses;
}

void LdpAddressTlvValue::clearAddresses() {
    _addresses.clear();
}

ssize_t LdpAddressTlvValue::addAddress(uint32_t address) {
    _addresses.push_back(address);

    return sizeof(uint32_t);
}

ssize_t LdpAddressTlvValue::parse(const uint8_t *from, size_t tlv_sz) {
    uint16_t af;

    const uint8_t *ptr = from;
    size_t buf_remaining = tlv_sz;

    GETVAL_S(ptr, buf_remaining, uint16_t, af, ntohs, -1);
    
    if (af != 1) {
        log_fatal("unsupported af: %u\n", af);
        return -1;
    }

    if (buf_remaining % sizeof(uint32_t) != 0) {
        log_fatal("addr list len not multiple of uint32_t, bad pkt.\n");
        return -1;
    }

    while (buf_remaining > 0) {
        uint32_t addr;
        GETVAL_S(ptr, buf_remaining, uint32_t, addr, , -1);
        this->addAddress(addr);
    }

    return ptr - from;
}

ssize_t LdpAddressTlvValue::write(uint8_t *to, size_t buf_sz) const {
    uint8_t *ptr = to;
    size_t buf_remaining = buf_sz;

    PUTVAL_S(ptr, buf_remaining, uint16_t, 1, htons, -1);

    for (const uint32_t &addr : _addresses) {
        PUTVAL_S(ptr, buf_remaining, uint32_t, addr, , -1);
    }

    return ptr - to;
}

size_t LdpAddressTlvValue::length() const {
    return sizeof(uint32_t) * _addresses.size() + sizeof(uint16_t);
}

}