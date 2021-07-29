#ifndef LDP_PDU_H
#define LDP_PDU_H
#include <memory>
#include <vector>

#include "serializable.hh"
#include "ldp-tlv.hh"

namespace ldpd {

class LdpPdu : public Serializable {
public:
    uint16_t getVersion() const;
    uint16_t getLength() const;
    uint32_t getRouterId() const;
    uint16_t getLabelSpace() const;

    void setVersion(uint16_t version);
    void setLength(uint16_t version);
    void setRouterId(uint16_t version);
    void setLableSpace(uint16_t version);

    void addTlv(const LdpTlv &tlv);
    void clearTlvs();

    std::vector<std::shared_ptr<const LdpTlv>> getTlvs() const;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    ssize_t length() const;

protected:
    virtual ssize_t doPrint(size_t indent, uint8_t *to, size_t buf_sz) const = 0;

};

}

#endif // LDP_PDU_H