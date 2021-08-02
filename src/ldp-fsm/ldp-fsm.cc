#include "utils/log.hh"
#include "ldp-fsm/ldp-fsm.hh"
#include "ldp-pdu/ldp-pdu.hh"
#include "ldp-tlv/ldp-tlv.hh"

#include <arpa/inet.h>

namespace ldpd {

LdpFsm::LdpFsm(Ldpd *ldpd) {
    _ldpd = ldpd;
    _state = Initialized;
    _neighId = 0;
    _neighLs = 0;
}

ssize_t LdpFsm::receive(const uint8_t *packet, size_t size) {
    LdpPdu pdu = LdpPdu();

    ssize_t parsed_len = pdu.parse(packet, size);

    if (parsed_len < 0) {
        _state = LdpSessionState::Invalid;
        // todo: send notification
        return parsed_len;
    }

    for (const LdpMessage *msg : pdu.getMessages()) {
        if (_state == LdpSessionState::Invalid) {
            log_fatal("this fsm should be deleted and the tcp session should be closed.\n");
            _state = LdpSessionState::Invalid;
            // todo: send notification
            return -1;
        }

        if (_state == Initialized) {
            if (msg->getType() != LDP_MSGTYPE_INITIALIZE) {
                log_error("got message of type 0x%.4x in init state.\n", msg->getType());
                _state = LdpSessionState::Invalid;
                // todo: send notification
                return -1;
            }

            // todo: handle init msg

            LdpPdu init = LdpPdu();
            createInitPdu(init);
            ssize_t rslt = send(init);

            if (rslt < 0) {
                _state = LdpSessionState::Invalid;
                return rslt;
            }

            rslt = sendKeepalive();

            if (rslt < 0) {
                _state = LdpSessionState::Invalid;
                return rslt;
            }

            _state = OpenReceived;
            continue;
        }

        if (_state == OpenReceived) {
            if (msg->getType() != LDP_MSGTYPE_KEEPALIVE) {
                log_error("got message of type 0x%.4x in open-received state.\n", msg->getType());
                _state = LdpSessionState::Invalid;
                // todo: send notification
                return -1;
            }

            _state = Operational;
            continue;
        }

        if (_state == OpenSent) {
            if (msg->getType() != LDP_MSGTYPE_INITIALIZE) {
                log_error("got message of type 0x%.4x in open-sent state.\n", msg->getType());
                _state = LdpSessionState::Invalid;
                // todo: send notification
                return -1;
            }

            LdpPdu init = LdpPdu();
            createInitPdu(init);
            ssize_t rslt = send(init);

            if (rslt < 0) {
                _state = LdpSessionState::Invalid;
                return rslt;
            }

            rslt = sendKeepalive();

            if (rslt < 0) {
                _state = LdpSessionState::Invalid;
                return rslt;
            }

            _state = LdpSessionState::OpenReceived;
            continue;
        }

        if (_state == Operational) {
            ssize_t rslt = _ldpd->handleMessage(this, msg);
            if (rslt < 0) {
                _state = Invalid;
                return rslt;
            }
        }

        log_fatal("fsm in invalid state: %d\n", _state);
        return -1;
    }

    return parsed_len;
}

ssize_t LdpFsm::step() {
    if (_state != LdpSessionState::Initialized) {
        return 0;
    }

    LdpPdu init = LdpPdu();
    createInitPdu(init);
    ssize_t rslt = send(init);

    if (rslt < 0) {
        _state = LdpSessionState::Invalid;
        return rslt;
    }

    _state = LdpSessionState::OpenSent;

    return 0;
}

LdpSessionState LdpFsm::getState() const {
    return _state;
}

uint32_t LdpFsm::getLocalId() const {
    return _ldpd->getRouterId();
}

uint16_t LdpFsm::getLocalLabelSpace() const {
    return _ldpd->getLabelSpace();
}

uint32_t LdpFsm::getNeighborId() const {
    return _neighId;
}

uint16_t LdpFsm::getNeighborLabelSpace() const {
    return _neighLs;
}

ssize_t LdpFsm::send(LdpPdu &pdu) {
    fillPduHeader(pdu);

    size_t len = pdu.length();
    uint8_t *buffer = (uint8_t *) malloc(len);

    ssize_t res = pdu.write(buffer, len);
    if (res < 0) {
        log_fatal("failed to write pdu.\n");
        // send notify?
        _state = LdpSessionState::Invalid;
        return res;
    }

    return _ldpd->transmit(this, buffer, len);
}

void LdpFsm::fillPduHeader(LdpPdu &pdu) const {
    pdu.setRouterId(_ldpd->getRouterId());
    pdu.setLabelSpace(_ldpd->getLabelSpace());
    pdu.recalculateLength();
}

ssize_t LdpFsm::sendKeepalive() {
    LdpPdu pdu = LdpPdu();

    LdpMessage *keep = new LdpMessage();

    keep->setType(LDP_MSGTYPE_KEEPALIVE);
    keep->setId(_ldpd->getNextMessageId());
    keep->recalculateLength();

    pdu.addMessage(keep);

    return send(pdu);
}

void LdpFsm::createInitPdu(LdpPdu &to) {
    LdpMessage *init = new LdpMessage();

    init->setType(LDP_MSGTYPE_INITIALIZE);
    init->setId(_ldpd->getNextMessageId());
    
    LdpCommonSessionParamsTlvValue session = LdpCommonSessionParamsTlvValue();

    session.setReceiverLabelSpace(_neighLs);
    session.setReceiverRouterId(_neighId);
    session.setKeepaliveTime(_ldpd->getKeepaliveTimer());

    LdpRawTlv *tlv = new LdpRawTlv();
    tlv->setValue(&session);

    init->addTlv(tlv);
    init->recalculateLength();

    to.addMessage(init);

    fillPduHeader(to);
}

}