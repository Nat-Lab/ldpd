#include "utils/value-ops.hh"
#include "ldp-pdu/ldp-pdu.hh"
#include "utils/log.hh"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace ldpd {

LdpPdu::LdpPdu() : _messages() {
    _version = LDP_VERSION;
    _length = 0;
    _routerId = 0;
    _labelSpace = 0;
}

LdpPdu::~LdpPdu() {
    this->clearMessages();
}

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
 * 
 * @return bytes changed.
 */
ssize_t LdpPdu::setVersion(uint16_t version) {
    _version = version;

    return sizeof(_version);
}

/**
 * @brief set length field in the LDP PDU.
 * 
 * @param version length in host byte.
 * 
 * @return bytes changed.
 */
ssize_t LdpPdu::setLength(uint16_t length) {
    _version = length;

    return sizeof(_version);
}

/**
 * @brief set router id field in the LDP PDU.
 * 
 * @param version router id in network byte.
 * 
 * @return bytes changed.
 */
ssize_t LdpPdu::setRouterId(uint32_t routerId) {
    _routerId = routerId;

    return sizeof(_routerId);
}

/**
 * @brief set label space field in the LDP PDU.
 * 
 * @param version label space in host byte.
 * 
 * @return bytes changed.
 */
ssize_t LdpPdu::setLabelSpace(uint16_t labelSpace) {
    _labelSpace = labelSpace;

    return sizeof(_routerId);
}

/**
 * @brief add a message to this pdu.
 * 
 * note: LdpPdu class handles freeing of the LdpMessage objects. DO NOT free it
 * yourself after adding message to pdu. DO NOT pass local variable pointer.
 * 
 * note: this also update length field.
 * 
 * @param message message 
 * @returns bytes added.
 */
ssize_t LdpPdu::addMessage(LdpMessage *message) {
    _messages.push_back(message);

    return message->length();
}

/**
 * @brief remove all messages from this pdu.
 * 
 */
void LdpPdu::clearMessages() {
    for (LdpMessage *msg : _messages) {
        delete msg;
    }

    _messages.clear();
}

/**
 * @brief get router id as IPv4 numbers-and-dots notation string.
 * 
 * note: uses inet_ntoa. not thread safe.
 * 
 * @return const char* router id in IPv4 numbers-and-dots notation string.
 */
const char* LdpPdu::getRouterIdString() const {
    return inet_ntoa(*((in_addr *) &_routerId));
}

/**
 * @brief set router id with IPv4 numbers-and-dots notation string.
 * 
 * @param id router id in IPv4 numbers-and-dots notation string.
 * 
 * @return bytes changed, or -1 on error.
 */
ssize_t LdpPdu::setRouterIdString(const char* id) {
    if (inet_pton(AF_INET, id, &_routerId) != 1) {
        return -1;
    }

    return sizeof(_routerId);
}

/**
 * @brief get the list of messages.
 * 
 * @return const std::vector<LdpRawMessage * list of messages.
 */
const std::vector<LdpMessage *> LdpPdu::getMessages() const {
    return _messages;
}

const LdpMessage* LdpPdu::getMessage(uint16_t type) const {
    for (const LdpMessage *msg : _messages) {
        if (msg->getType() == type) {
            return msg;
        }
    }

    return nullptr;
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

    for (const LdpMessage *msg : _messages) {
        _length += msg->length();
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

    GETVAL_S(ptr, buf_remaining, uint16_t, _version, ntohs, -1);
    GETVAL_S(ptr, buf_remaining, uint16_t, _length, ntohs, -1);
    GETVAL_S(ptr, buf_remaining, uint32_t, _routerId, , -1);
    GETVAL_S(ptr, buf_remaining, uint16_t, _labelSpace, ntohs, -1);

    if (_version != LDP_VERSION) {
        log_fatal("unknow ldp version: %u\n", _version);
        return -1;
    }

    size_t msgs_len = _length - sizeof(_routerId) - sizeof(_labelSpace);

    if (msgs_len > buf_remaining) {
        log_fatal("msgs_len (%zu) greater then remaining buffer (%zu), packet truncated?\n", msgs_len, buf_remaining);
        return -1;
    }

    while (msgs_len > 0) {
        LdpMessage *msg = new LdpMessage();

        PARSE_S(ptr, msgs_len, msg, -1, true);

        this->addMessage(msg);
    }

    return ptr - from;
}

/**
 * @brief write pdu to buffer.
 * 
 * @param to dst buffer.
 * @param buf_sz size of dst buffer.
 * @return ssize_t bytes written, or -1 on error.
 */
ssize_t LdpPdu::write(uint8_t *to, size_t buf_sz) const {
    size_t len = this->length();

    size_t buf_remaining = buf_sz;
    uint8_t *ptr = to;

    if (len > buf_sz) {
        log_fatal("buf_sz (%zu) too small - can not fit pdu (size is %zu)\n", buf_sz, len);
        return -1;
    }

    PUTVAL_S(ptr, buf_remaining, uint16_t, _version, htons, -1);
    PUTVAL_S(ptr, buf_remaining, uint16_t, _length, htons, -1);
    PUTVAL_S(ptr, buf_remaining, uint32_t, _routerId, , -1);
    PUTVAL_S(ptr, buf_remaining, uint16_t, _labelSpace, htons, -1);

    for (const LdpMessage *msg : _messages) {
        WRITE_S(ptr, buf_remaining, msg, -1);
    }

    return ptr - to;
}

/**
 * @brief get length of the pdu.
 * 
 * @return size_t 
 */
size_t LdpPdu::length() const {
    size_t len = sizeof(_version) + sizeof(_length) + sizeof(_routerId) + sizeof(_labelSpace);

    for (const LdpMessage *msg : _messages) {
        len += msg->length();
    }

    return len;
}

}