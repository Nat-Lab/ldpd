#ifndef LDP_MESSAGE_H
#define LDP_MESSAGE_H

#include <vector>
#include "ldp-tlv/ldp-raw-tlv.hh"

#define LDP_MSGTYPE_NOTIFICATION 0x0001
#define LDP_MSGTYPE_HELLO 0x0100
#define LDP_MSGTYPE_INITIALIZE 0x0200
#define LDP_MSGTYPE_KEEPALIVE 0x0201
#define LDP_MSGTYPE_ADDRESS 0x0300
#define LDP_MSGTYPE_ADDRESS_WITHDRAW 0x0301
#define LDP_MSGTYPE_LABEL_MAPPING 0x0400
#define LDP_MSGTYPE_LABEL_REQUEST 0x0401
#define LDP_MSGTYPE_LABEL_WITHDRAW 0x0402
#define LDP_MSGTYPE_LABEL_RELEASE 0x0403
#define LDP_MSGTYPE_LABEL_ABORT 0x0404

namespace ldpd {

class LdpMessage : public Serializable {
public:
    LdpMessage();
    ~LdpMessage();

    bool unknown() const;

    uint16_t getType() const;
    uint16_t getLength() const;
    uint32_t getId() const;

    ssize_t setType(uint16_t type);
    ssize_t setLength(uint16_t length);
    ssize_t setId(uint32_t id);
    ssize_t setUnknown(bool unknown);

    ssize_t addTlv(LdpRawTlv *message);
    void clearTlvs();

    uint16_t recalculateLength();

    const std::vector<LdpRawTlv *> getTlvs() const;

protected:
    uint16_t _type;
    uint16_t _length;
    uint32_t _id;
    bool _unknown;

    std::vector<LdpRawTlv *> _tlvs;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t buf_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_MESSAGE_H