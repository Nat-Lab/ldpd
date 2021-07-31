#include <stdio.h>
#include "ldp-pdu/ldp-pdu.hh"
#include "ldp-tlv/ldp-tlv.hh"

#include <arpa/inet.h>
#include <stdlib.h>

void hex_dump(const void* data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

int diff(const uint8_t *a, const uint8_t *b, size_t sz) {
    for (unsigned int i = 0; i < sz; ++i) {
        if (a[i] != b[i]) {
            return i;
        }
    }

    return -1;
}

ldpd::LdpRawTlv* parse_and_writeback_tlv(const ldpd::LdpRawTlv *parsed_raw_tlv) {
    ldpd::LdpRawTlv *writeback_tlv = new ldpd::LdpRawTlv();

    ldpd::LdpTlvValue *parsed_val = parsed_raw_tlv->getParsedValue();

    if (parsed_val == nullptr) {
        return nullptr;
    }

    switch (parsed_val->getType()) {
        case LDP_TLVTYPE_FEC: {
            printf("====== val type is fec.\n");
            ldpd::LdpFecTlvValue *casted = (ldpd::LdpFecTlvValue *) parsed_val;
            ldpd::LdpFecTlvValue *writeback = new ldpd::LdpFecTlvValue();

            for (const ldpd::LdpFecElement *el : casted->getElements()) {
                switch (el->getType()) {
                    case 0x01: {
                        printf("======== fec el: wildcard.\n");

                        writeback->addElement(new ldpd::LdpFecWildcardElement());

                        break;
                    }
                    case 0x02: {
                        const ldpd::LdpFecPrefixElement *casted_el = (const ldpd::LdpFecPrefixElement *) el;
                        printf("======== fec el: pfx: %s/%d.\n", casted_el->getPrefixString(), casted_el->getPrefixLength());

                        ldpd::LdpFecPrefixElement *writeback_el = new ldpd::LdpFecPrefixElement();

                        writeback_el->setPrefix(casted_el->getPrefix());
                        writeback_el->setPrefixLength(casted_el->getPrefixLength());

                        writeback->addElement(writeback_el);
                        
                        break;
                    }
                    default: {
                        printf("unknow fec ele type?\n");
                        return nullptr;
                    }
                }

                writeback_tlv->setValue(writeback);
                delete writeback;
            }
            break;
        }
        case LDP_TLVTYPE_ADDRESS_LIST: {
            ldpd::LdpAddressTlvValue *casted = (ldpd::LdpAddressTlvValue *) parsed_val;
            printf("====== val type is addr-list, addr count: %zu\n", casted->getAddresses().size());

            ldpd::LdpAddressTlvValue *writeback = new ldpd::LdpAddressTlvValue();

            for (const uint32_t &addr : casted->getAddresses()) {
                printf("======== addr: %s.\n", inet_ntoa(*(in_addr *) &addr));
                writeback->addAddress(addr);
            }

            writeback_tlv->setValue(writeback);
            delete writeback;

            break;
        }
        case LDP_TLVTYPE_GENERIC_LABEL: {
            ldpd::LdpGenericLabelTlvValue *casted = (ldpd::LdpGenericLabelTlvValue *) parsed_val;

            printf("====== val type is g-lbl. val: %u\n", casted->getLabel());

            ldpd::LdpGenericLabelTlvValue *writeback = new ldpd::LdpGenericLabelTlvValue();

            writeback->setLabel(casted->getLabel());

            writeback_tlv->setValue(writeback);
            delete writeback;

            break;
        }
        case LDP_TLVTYPE_COMMON_HELLO: {
            ldpd::LdpCommonHelloParamsTlvValue *casted = (ldpd::LdpCommonHelloParamsTlvValue *) parsed_val;

            printf("====== val type is common hello. hold-t: %u, T: %d, R: %d.\n", casted->getHoldTime(), casted->targeted(), casted->requestTargeted());

            ldpd::LdpCommonHelloParamsTlvValue *writeback = new ldpd::LdpCommonHelloParamsTlvValue();
            writeback->setHoldTime(casted->getHoldTime());
            writeback->setRequestTargeted(casted->requestTargeted());
            writeback->setTargeted(casted->targeted());

            writeback_tlv->setValue(writeback);
            delete writeback;

            break;
        }
        case LDP_TLVTYPE_IPV4_TRANSPORT: {
            ldpd::LdpIpv4TransportAddressTlvValue *casted = (ldpd::LdpIpv4TransportAddressTlvValue *) parsed_val;

            printf("====== val type is v4-t-addr. addr: %s.\n", casted->getAddressString());

            ldpd::LdpIpv4TransportAddressTlvValue *writeback = new ldpd::LdpIpv4TransportAddressTlvValue();
            writeback->setAddress(casted->getAddress());

            writeback_tlv->setValue(writeback);
            delete writeback;

            break;
        }
        default: {
            printf("test case not impl? type is 0x%.4x\n", parsed_val->getType());
            return nullptr;
        }
    }

    delete parsed_val;

    return writeback_tlv;
}

int parse_and_writeback(const uint8_t *buffer, size_t len) {
    ldpd::LdpPdu parsed_pdu = ldpd::LdpPdu();

    ssize_t parse_rslt = parsed_pdu.parse(buffer, len);

    if (parse_rslt < 0) {
        return 1;
    }

    ldpd::LdpPdu writeback_pdu = ldpd::LdpPdu();

    printf("input size: %zu, parsed size: %zu\n", len, parse_rslt);
    printf("messages count: %zu\n", parsed_pdu.getMessages().size());
    printf("router id: %s\n", parsed_pdu.getRouterIdString());

    int count = 0;
    for (const ldpd::LdpMessage *msg : parsed_pdu.getMessages()) {
        printf("== msg %d type: 0x%.4x, len: %u, (%zu tlvs)\n", ++count, msg->getType(), msg->getLength(), msg->getTlvs().size());
        
        ldpd::LdpMessage *writeback_msg = new ldpd::LdpMessage();

        int tlv_count = 0;
        for (const ldpd::LdpRawTlv *tlv : msg->getTlvs()) {
            printf("==== tlv %d type: 0x%.4x, len: %u\n", ++tlv_count, tlv->getType(), tlv->getLength());

            ldpd::LdpRawTlv *writeback_tlv = parse_and_writeback_tlv(tlv);

            if (writeback_tlv == nullptr) {
                return 1;
            }

            writeback_msg->addTlv(writeback_tlv);
        }

        writeback_msg->setId(msg->getId());
        writeback_msg->setType(msg->getType());
        writeback_msg->recalculateLength();

        writeback_pdu.addMessage(writeback_msg);
    }

    writeback_pdu.setRouterId(parsed_pdu.getRouterId());
    writeback_pdu.setLabelSpace(parsed_pdu.getLabelSpace());
    writeback_pdu.recalculateLength();

    if (writeback_pdu.length() != parsed_pdu.length()) {
        printf("writeback pdu len mismatch.");
        return 1;
    }

    uint8_t *wb_buffer = (uint8_t *) malloc(writeback_pdu.length());
    writeback_pdu.write(wb_buffer, writeback_pdu.length());

    int diff_pos = diff(buffer, wb_buffer, writeback_pdu.length());

    if (diff_pos > 0) {
        printf("writeback pdu diff with orig pkt at byte %d\n", diff_pos);
        hex_dump(buffer,  writeback_pdu.length());
        printf("==========\n");
        hex_dump(wb_buffer,  writeback_pdu.length());
        return 1;
    }

    free(wb_buffer);

    printf("test passed.\n");

    return 0;
}

#define TEST(pdu) if (parse_and_writeback((const uint8_t *) pdu, sizeof(pdu)) != 0) { return 1; }

#define KEEPALIVE_PDU "\x00\x01\x00\x0e\x42\x06\x06\x06\x00\x00\x02\x01\x00\x04\x00\x00\x00\x02"
#define MAPPING_PDU "\x00\x01\x01\x9e\x42\x06\x06\x06\x00\x00\x03\x00\x00\x1a\x00\x00\x00\x03\x01\x01\x00\x12\x00\x01\x0a\x01\x43\x06\x0a\x01\x38\x06\x06\x06\x06\x06\x42\x06\x06\x06\x04\x00\x00\x17\x00\x00\x00\x04\x01\x00\x00\x07\x02\x00\x01\x18\x01\x01\x01\x02\x00\x00\x04\x00\x00\x00\x10\x04\x00\x00\x17\x00\x00\x00\x05\x01\x00\x00\x07\x02\x00\x01\x18\x02\x02\x02\x02\x00\x00\x04\x00\x00\x00\x11\x04\x00\x00\x17\x00\x00\x00\x06\x01\x00\x00\x07\x02\x00\x01\x18\x03\x03\x03\x02\x00\x00\x04\x00\x00\x00\x12\x04\x00\x00\x17\x00\x00\x00\x07\x01\x00\x00\x07\x02\x00\x01\x18\x04\x04\x04\x02\x00\x00\x04\x00\x00\x00\x13\x04\x00\x00\x17\x00\x00\x00\x08\x01\x00\x00\x07\x02\x00\x01\x18\x05\x05\x05\x02\x00\x00\x04\x00\x00\x00\x14\x04\x00\x00\x17\x00\x00\x00\x09\x01\x00\x00\x07\x02\x00\x01\x18\x42\x06\x06\x02\x00\x00\x04\x00\x00\x00\x03\x04\x00\x00\x17\x00\x00\x00\x0a\x01\x00\x00\x07\x02\x00\x01\x18\x06\x06\x06\x02\x00\x00\x04\x00\x00\x00\x03\x04\x00\x00\x17\x00\x00\x00\x0b\x01\x00\x00\x07\x02\x00\x01\x18\x07\x07\x07\x02\x00\x00\x04\x00\x00\x00\x15\x04\x00\x00\x17\x00\x00\x00\x0c\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x0c\x02\x00\x00\x04\x00\x00\x00\x16\x04\x00\x00\x17\x00\x00\x00\x0d\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x17\x02\x00\x00\x04\x00\x00\x00\x17\x04\x00\x00\x17\x00\x00\x00\x0e\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x2d\x02\x00\x00\x04\x00\x00\x00\x18\x04\x00\x00\x17\x00\x00\x00\x0f\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x22\x02\x00\x00\x04\x00\x00\x00\x19\x04\x00\x00\x17\x00\x00\x00\x10\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x38\x02\x00\x00\x04\x00\x00\x00\x03\x04\x00\x00\x17\x00\x00\x00\x11\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x43\x02\x00\x00\x04\x00\x00\x00\x03"
#define WITHDRAWAL_PDU "\x00\x01\x01\xba\x21\x03\x03\x03\x00\x00\x04\x02\x00\x18\x00\x00\x06\x08\x01\x00\x00\x08\x02\x00\x01\x20\x01\x01\x01\x01\x02\x00\x00\x04\x00\x00\x01\x35\x04\x02\x00\x18\x00\x00\x06\x09\x01\x00\x00\x08\x02\x00\x01\x20\x02\x02\x02\x02\x02\x00\x00\x04\x00\x00\x01\x36\x04\x02\x00\x17\x00\x00\x06\x0a\x01\x00\x00\x07\x02\x00\x01\x18\x03\x03\x03\x02\x00\x00\x04\x00\x00\x00\x03\x04\x02\x00\x17\x00\x00\x06\x0b\x01\x00\x00\x07\x02\x00\x01\x18\x04\x04\x04\x02\x00\x00\x04\x00\x00\x01\x2d\x04\x02\x00\x17\x00\x00\x06\x0c\x01\x00\x00\x07\x02\x00\x01\x18\x05\x05\x05\x02\x00\x00\x04\x00\x00\x01\x31\x04\x02\x00\x18\x00\x00\x06\x0d\x01\x00\x00\x08\x02\x00\x01\x20\x06\x06\x06\x06\x02\x00\x00\x04\x00\x00\x01\x32\x04\x02\x00\x17\x00\x00\x06\x0e\x01\x00\x00\x07\x02\x00\x01\x18\x07\x07\x07\x02\x00\x00\x04\x00\x00\x01\x33\x04\x02\x00\x17\x00\x00\x06\x0f\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x0c\x02\x00\x00\x04\x00\x00\x01\x34\x04\x02\x00\x17\x00\x00\x06\x10\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x17\x02\x00\x00\x04\x00\x00\x00\x03\x04\x02\x00\x17\x00\x00\x06\x11\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x22\x02\x00\x00\x04\x00\x00\x00\x03\x04\x02\x00\x17\x00\x00\x06\x18\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x2d\x02\x00\x00\x04\x00\x00\x01\x2e\x04\x02\x00\x17\x00\x00\x06\x19\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x38\x02\x00\x00\x04\x00\x00\x01\x2f\x04\x02\x00\x17\x00\x00\x06\x1a\x01\x00\x00\x07\x02\x00\x01\x18\x0a\x01\x43\x02\x00\x00\x04\x00\x00\x01\x30\x04\x02\x00\x18\x00\x00\x06\x1b\x01\x00\x00\x08\x02\x00\x01\x20\x0b\x01\x01\x01\x02\x00\x00\x04\x00\x00\x01\x37\x04\x02\x00\x17\x00\x00\x06\x1c\x01\x00\x00\x07\x02\x00\x01\x18\x21\x03\x03\x02\x00\x00\x04\x00\x00\x00\x03\x04\x02\x00\x17\x00\x00\x06\x1d\x01\x00\x00\x07\x02\x00\x01\x18\xb1\x07\x07\x02\x00\x00\x04\x00\x00\x01\x38"
#define HELLO_PDU "\00\x01\x00\x1e\x0a\x00\x01\x01\x00\x00\x01\x00\x00\x14\x00\x00\x00\x00\x04\x00\x00\x04\x00\x0f\x00\x00\x04\x01\x00\x04\x0a\x00\x01\x01"

int main() {

    TEST(KEEPALIVE_PDU);
    TEST(MAPPING_PDU);
    TEST(WITHDRAWAL_PDU);
    TEST(HELLO_PDU);

    return 0; 
}