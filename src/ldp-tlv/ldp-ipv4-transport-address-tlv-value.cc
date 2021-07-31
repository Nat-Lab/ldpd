#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-tlv/ldp-tlv-types.hh"
#include "ldp-tlv/ldp-ipv4-transport-address-tlv-value.hh"

#include <arpa/inet.h>

namespace ldpd {

LdpIpv4TransportAddressTlvValue::LdpIpv4TransportAddressTlvValue() {
    _address = 0;
}

LdpIpv4TransportAddressTlvValue::~LdpIpv4TransportAddressTlvValue() {

}

uint16_t LdpIpv4TransportAddressTlvValue::getType() const {
    return LDP_TLVTYPE_IPV4_TRANSPORT;
}

uint32_t LdpIpv4TransportAddressTlvValue::getAddress() const {
    return _address;
}

const char* LdpIpv4TransportAddressTlvValue::getAddressString() const {
    return inet_ntoa(*((in_addr *) &_address));
}

ssize_t LdpIpv4TransportAddressTlvValue::setAddress(uint32_t address) {
    _address = address;

    return sizeof(_address);
}

ssize_t LdpIpv4TransportAddressTlvValue::setAddressString(const char* address) {
    if (inet_pton(AF_INET, address, &_address) != 1) {
        return -1;
    }

    return sizeof(_address);
}

ssize_t LdpIpv4TransportAddressTlvValue::parse(const uint8_t *from, size_t tlv_sz) {
    if (tlv_sz != this->length()) {
        log_fatal("bad length. need len %zu, but got %zu.\n", this->length(), tlv_sz);
        return -1;
    }

    const uint8_t *ptr = from;
    size_t buf_remaining = tlv_sz;

    GETVAL_S(ptr, buf_remaining, uint32_t, _address, , -1);

    return ptr - from;
}

ssize_t LdpIpv4TransportAddressTlvValue::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < this->length()) {
        log_fatal("buf too small, can not write.\n");
        return -1;
    }

    uint8_t *ptr = to;
    size_t buf_remaining = buf_sz;

    PUTVAL_S(ptr, buf_remaining, uint32_t, _address, , -1);

    return ptr - to;
}

size_t LdpIpv4TransportAddressTlvValue::length() const {
    return sizeof(_address);
}

}