#ifndef LDP_STATUS_TLV_H
#define LDP_STATUS_TLV_H
#include "core/serializable.hh"
#include "ldp-tlv/ldp-tlv-value.hh"

namespace ldpd {

#define LDP_SC_SUCCESS 0X00000000
#define LDP_SC_BAD_LDP_ID 0X00000001
#define LDP_SC_BAD_PROTO_VER 0X00000002
#define LDP_SC_BAD_PDU_LEN 0X00000003
#define LDP_SC_UNKNOWN_MSG_TYPE 0X00000004
#define LDP_SC_BAD_MSG_LEN 0X00000005
#define LDP_SC_UNKNOWN_TLV 0X00000006
#define LDP_SC_BAD_TLV_LEN 0X00000007
#define LDP_SC_MALFORMED_TLV_VAL 0X00000008
#define LDP_SC_HOLD_EXPIRED 0X00000009
#define LDP_SC_SHUTDOWN 0X0000000A
#define LDP_SC_LOOP_DETECTED 0X0000000B
#define LDP_SC_UNKNOWN_FEC 0X0000000C
#define LDP_SC_NO_ROUTE 0X0000000D
#define LDP_SC_NO_LBL_RES 0X0000000E
#define LDP_SC_LBL_RES_AVAIL 0X0000000F
#define LDP_SC_SESSION_REJ_NOHELLO 0X00000010
#define LDP_SC_SESSION_REJ_PAM 0X00000011
#define LDP_SC_SESSION_REJ_PDU_LEN 0X00000012
#define LDP_SC_SESSION_REJ_LBL_RANGE 0X00000013
#define LDP_SC_KEEPALIVE_EXPIRED 0X00000014
#define LDP_SC_LBL_REQ_ABORTED 0X00000015
#define LDP_SC_MISSING_MES_PARAM 0X00000016
#define LDP_SC_UNSUPPORTED_AF 0X00000017
#define LDP_SC_BAD_KEEPALIVE 0X00000018
#define LDP_SC_INTERNAL_ERROR 0X00000019

class LdpStatusTlvValue : public LdpTlvValue {
public:
    LdpStatusTlvValue();
    ~LdpStatusTlvValue();
    uint16_t getType() const;

    uint32_t getStatusCode() const;
    uint32_t getMessageId() const;
    uint16_t getMessageType() const;

    ssize_t setStatusCode(uint32_t statusCode);
    ssize_t setMessageId(uint32_t messageId);
    ssize_t setMessageType(uint16_t messageType);

    bool fatal() const;
    bool forward() const;

    ssize_t setFatal(bool fatal);
    ssize_t setForward(bool forward);

    const char *getStatusCodeText() const;

private:

    bool _fatal;
    bool _forward;

    uint32_t _statusCode;
    uint32_t _messageId;
    uint16_t _messageType;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t tlv_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_STATUS_TLV_H