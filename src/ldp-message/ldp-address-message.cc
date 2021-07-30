#include "utils/log.hh"
#include "utils/value-ops.hh"
#include "ldp-message/ldp-address-message.hh"

namespace ldpd {

LdpAddressMessageBody::LdpAddressMessageBody() {
    _addresses = nullptr;
}

LdpAddressMessageBody::~LdpAddressMessageBody() {
    if (_addresses != nullptr) {
        delete _addresses;
    }
}

uint16_t LdpAddressMessageBody::getType() const {
    return 0x0300;
}

ssize_t LdpAddressMessageBody::setAddresses(LdpRawTlv *tlv) {
    if (tlv->getType() != 0x0101) {
        log_fatal("want tlv type 0x0101 but saw %.4x\n", tlv->getType());
        return -1;
    }

    if (_addresses != nullptr) {
        delete _addresses;
    }

    _addresses = tlv;

    return tlv->length();
}

const LdpRawTlv* LdpAddressMessageBody::getAddresses() const {
    return _addresses;
}

ssize_t LdpAddressMessageBody::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz < 3 * sizeof(uint16_t)) {
        log_fatal("msg_sz too small, truncated?\n");
        return -1;
    }

    if (_addresses != nullptr) {
        delete _addresses;
    }

    LdpRawTlv *addresses = new LdpRawTlv();

    const uint8_t *ptr = from;
    size_t buf_remaining = msg_sz;

    PARSE_S(ptr, buf_remaining, addresses, -1, true);    

    if (this->setAddresses(addresses) < 0) {
        delete addresses;
    }

    _addresses = addresses;

    return addresses->length();
}

ssize_t LdpAddressMessageBody::write(uint8_t *to, size_t buf_sz) const {
    return _addresses->write(to, buf_sz);
}

size_t LdpAddressMessageBody::length() const {
    return _addresses->length();
}

}
