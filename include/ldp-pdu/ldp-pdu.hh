#ifndef LDP_PDU_H
#define LDP_PDU_H
#include <vector>

#include "core/serializable.hh"
#include "ldp-message/ldp-message.hh"

namespace ldpd {

class LdpPdu : public Serializable {
public:
    LdpPdu();
    ~LdpPdu();

    uint16_t getVersion() const;
    uint16_t getLength() const;
    uint32_t getRouterId() const;
    uint16_t getLabelSpace() const;

    ssize_t setVersion(uint16_t version);
    ssize_t setLength(uint16_t version);
    ssize_t setRouterId(uint32_t version);
    ssize_t setLableSpace(uint16_t version);

    ssize_t addMessage(LdpRawMessage *message);
    void clearMessages();

    const char* getRouterIdString() const;
    ssize_t setRouterIdString(const char* id);

    const std::vector<LdpRawMessage *> getMessages() const;

    uint16_t recalculateLength();

private:
    uint16_t _version;
    uint16_t _length;
    uint32_t _routerId;
    uint16_t _labelSpace;
    std::vector<LdpRawMessage *> _messages;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_PDU_H