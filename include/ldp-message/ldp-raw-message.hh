#ifndef LDP_RAW_MESSAGE_H
#define LDP_RAW_MESSAGE_H
#include "core/serializable.hh"
#include "ldp-message/ldp-message-body.hh"

namespace ldpd {

class LdpRawMessage : public Serializable {
public:
    LdpRawMessage();
    ~LdpRawMessage();

    bool unknown() const;

    uint16_t getType() const;
    uint16_t getLength() const;

    const uint8_t* peekRawBody() const;

    ssize_t setType(uint16_t type);
    ssize_t setLength(uint16_t length);

    void setRawBody(size_t size, const uint8_t *src);
    void setBody(const LdpMessageBody *value);
    
    LdpMessageBody* getParsedBody();

protected:
    uint8_t *_raw_buffer;
    size_t _raw_buffer_size; // might not match len field in case of bad pkt.

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_RAW_MESSAGE_H