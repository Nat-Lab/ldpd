#ifndef LDP_MESSAGE_BODY_H
#define LDP_MESSAGE_BODY_H
#include "core/serializable.hh"

namespace ldpd {

class LdpMessageBody : public Serializable {
public:
    virtual ~LdpMessageBody() {};
    virtual uint16_t getType() const = 0;
};

}

#endif // LDP_MESSAGE_BODY_H