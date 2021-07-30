#include "utils/log.hh"
#include "ldp-pdu/ldp-pdu.hh"
#include "ldp-tlv/ldp-tlv.hh"

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

ldpd::LdpRawTlv* parse_and_writeback_tlv(ldpd::LdpRawTlv* parsed_raw_tlv) {

}

ldpd::LdpRawMessage* parse_and_writeback_msg(const ldpd::LdpRawMessage *parsed_raw_msg) {
    ldpd::LdpRawMessage *writeback_msg = new ldpd::LdpRawMessage();

    ldpd::LdpMessageBody* body = parsed_raw_msg->getParsedBody();

    switch(body->getType()) {
        
    }

    delete body;

    return writeback_msg;
}

ldpd::LdpPdu* parse_and_writeback_pdu(const ldpd::LdpPdu *parsed_pdu) {
    ldpd::LdpPdu *writeback_pdu = new ldpd::LdpPdu();

    log_info("messages count: %zu\n", parsed_pdu->getMessages().size());
    log_info("router id: %s\n", parsed_pdu->getRouterIdString());

    int count = 0;
    for (const ldpd::LdpRawMessage *msg : parsed_pdu->getMessages()) {
        log_info("== msg %d type: 0x%.4x, len: %zu\n", ++count, msg->getType(), msg->getLength());
        
        ldpd::LdpRawMessage *writeback_msg = parse_and_writeback_msg(msg);

        if (writeback_msg == nullptr) {
            return nullptr;
        }

        writeback_pdu->addMessage(writeback_msg);
    }

    return writeback_pdu;
}