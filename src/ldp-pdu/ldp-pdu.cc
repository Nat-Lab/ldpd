#include "utils/value-ops.hh"
#include "ldp-pdu/ldp-pdu.hh"
#include "utils/log.hh"

#include <arpa/inet.h>

namespace ldpd {

/**
 * @brief get version field in the LDP PDU.
 * 
 * @return uint16_t version in host byte.
 */
uint16_t LdpPdu::getVersion() const {
    return _version;
}

/**
 * @brief get length field in the LDP PDU.
 * 
 * @return uint16_t length in host byte.
 */
uint16_t LdpPdu::getLength() const {
    return _length;
}

/**
 * @brief get router id field in the LDP PDU.
 * 
 * @return uint16_t router id in network byte.
 */
uint32_t LdpPdu::getRouterId() const {
    return _routerId;
}

/**
 * @brief get label space field in the LDP PDU.
 * 
 * @return uint16_t label space in host byte.
 */
uint16_t LdpPdu::getLabelSpace() const {
    return _labelSpace;
}

/**
 * @brief set version field in the LDP PDU.
 * 
 * @param version version in host byte.
 */
void LdpPdu::setVersion(uint16_t version) {
    _version = version;
}

/**
 * @brief set length field in the LDP PDU.
 * 
 * @param version length in host byte.
 */
void LdpPdu::setLength(uint16_t length) {
    _version = length;
}

/**
 * @brief set router id field in the LDP PDU.
 * 
 * @param version router id in network byte.
 */
void LdpPdu::setRouterId(uint32_t routerId) {
    _routerId = routerId;
}

/**
 * @brief set lable space field in the LDP PDU.
 * 
 * @param version lable space in host byte.
 */
void LdpPdu::setLableSpace(uint16_t lableSpace) {
    _labelSpace = lableSpace;
}

/**
 * @brief add tlv to this pdu.
 * 
 * note: LdpPdu class handles freeing of the LdpTlv objects. DO NOT free it your
 * self after adding tlvs to pdu. DO NOT pass local variable pointer.
 * 
 * @param tlv 
 */
void LdpPdu::addTlv(LdpRawTlv *tlv) {
    _tlvs.push_back(tlv);
}

/**
 * @brief remove all tlvs from this pdu.
 * 
 */
void LdpPdu::clearTlvs() {
    for (LdpRawTlv *tlv : _tlvs) {
        delete tlv;
    }

    _tlvs.clear();
}

/**
 * @brief get the list of tlvs.
 * 
 * @return const std::vector<LdpTlv * list of tlvs.
 */
const std::vector<LdpRawTlv *> LdpPdu::getTlvs() const {
    return _tlvs;
}

/**
 * @brief re-calculate the appropriate length for the length field.
 * 
 * this call updates the length filed and return the updated value.
 * 
 * @return uint16_t new label space in host byte.
 */
uint16_t LdpPdu::recalculateLength() {
    _length = 0;

    for (const LdpRawTlv *tlv : _tlvs) {
        _length += tlv->length();
    }

    _length += sizeof(_routerId) + sizeof(_labelSpace);

    return _length;
}

/**
 * @brief parse ldp pdu from buffer.
 * 
 * @param from source.
 * @param msg_sz buffer length.
 * @return ssize_t btyes parsed, or -1 on error.
 */
ssize_t LdpPdu::parse(const uint8_t *from, size_t msg_sz) {
    const uint8_t *ptr = from;
    size_t buf_remaining = msg_sz;

    if (buf_remaining < (sizeof(_version) + sizeof(_length) + sizeof(_routerId) + sizeof(_labelSpace))) {
        log_fatal("invalid packet: too small (size is %zu)\n", buf_remaining);
        return -1;
    }

    GETVAL_S(ptr, buf_remaining, uint16_t, _version, ntohs);
    GETVAL_S(ptr, buf_remaining, uint16_t, _length, ntohs);
    GETVAL_S(ptr, buf_remaining, uint32_t, _routerId, );
    GETVAL_S(ptr, buf_remaining, uint16_t, _labelSpace, ntohs);

    size_t tlvs_len = _length - sizeof(_routerId) - sizeof(_labelSpace);

    if (tlvs_len > buf_remaining) {
        log_fatal("tlvs_len (%zu) greater then remaining buffer (%zu), packet truncated?\n", );
        return -1;
    }

    while (tlvs_len > 0) {
        LdpRawTlv *tlv = new LdpRawTlv();

        ssize_t res = tlv->parse(ptr, tlvs_len);

        if (res < 0) {
            return -1;
        }

        ptr += res;
        tlvs_len -= res;
    }

    return ptr - from;
}

}