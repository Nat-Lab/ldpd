#include "utils/log.hh"
#include "ldp-fsm/ldp-fsm.hh"
#include "ldp-pdu/ldp-pdu.hh"
#include "ldp-tlv/ldp-tlv.hh"

#include <arpa/inet.h>

namespace ldpd {

const char *LdpSessionStateText[] = {
    "Invalid", "Initialized", "OpenReceived", "OpenSent", "Operational"
};    

LdpFsm::LdpFsm(Ldpd *ldpd) {
    _ldpd = ldpd;
    _state = Initialized;
    _neighId = 0;
    _neighLs = 0;
    _keep = ldpd->getKeepaliveTime();
    _last_send = 0;
    _last_recv = 0;
}

ssize_t LdpFsm::receive(const uint8_t *packet, size_t size) {
    LdpPdu pdu = LdpPdu();

    ssize_t parsed_len = pdu.parse(packet, size);

    _last_recv = _ldpd->now();

    if (parsed_len < 0) {
        changeState(LdpSessionState::Invalid);
        // todo: send notification
        return parsed_len;
    }

    for (const LdpMessage *msg : pdu.getMessages()) {
        if (_state == LdpSessionState::Invalid) {
            log_fatal("(%s:%u) this fsm should be deleted and the tcp session should be closed.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs);
            changeState(LdpSessionState::Invalid);
            // todo: send notification
            return -1;
        }

        if (_state == Initialized) {
            if (msg->getType() != LDP_MSGTYPE_INITIALIZE) {
                log_error("(%s:%u) got message of type 0x%.4x in init state.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs, msg->getType());
                changeState(LdpSessionState::Invalid);
                // todo: send notification
                return -1;
            }

            _neighLs = pdu.getLabelSpace();
            _neighId = pdu.getRouterId();

            if (processInit(msg) < 0) {
                return -1;
            }

            LdpPdu init = LdpPdu();
            createInitPdu(init);
            ssize_t rslt = send(init);

            if (rslt < 0) {
                changeState(LdpSessionState::Invalid);
                return rslt;
            }

            rslt = sendKeepalive();

            if (rslt < 0) {
                changeState(LdpSessionState::Invalid);
                return rslt;
            }

            changeState(LdpSessionState::OpenReceived);
            continue;
        }

        if (_state == OpenReceived) {
            if (msg->getType() != LDP_MSGTYPE_KEEPALIVE) {
                log_error("(%s:%u) got message of type 0x%.4x in open-received state.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs, msg->getType());
                changeState(LdpSessionState::Invalid);
                // todo: send notification
                return -1;
            }

            changeState(LdpSessionState::Operational);
            continue;
        }

        if (_state == OpenSent) {
            if (msg->getType() != LDP_MSGTYPE_INITIALIZE) {
                log_error("(%s:%u) got message of type 0x%.4x in open-sent state.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs, msg->getType());
                changeState(LdpSessionState::Invalid);
                // todo: send notification
                return -1;
            }

            if (processInit(msg) < 0) {
                return -1;
            }

            ssize_t rslt = sendKeepalive();

            if (rslt < 0) {
                changeState(LdpSessionState::Invalid);
                return rslt;
            }

            changeState(LdpSessionState::OpenReceived);
            continue;
        }

        if (_state == Operational) {
            ssize_t rslt = _ldpd->handleMessage(this, msg);

            if (rslt < 0) {
                changeState(LdpSessionState::Invalid);
                return rslt;
            }

            continue;
        }

        log_fatal("(%s:%u) fsm in invalid state: %d\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs, _state);
        return -1;
    }

    return parsed_len;
}

ssize_t LdpFsm::init(uint32_t neighId, uint16_t neighLabelSpace) {
    _neighId = neighId;
    _neighLs = neighLabelSpace;

    if (_state != LdpSessionState::Initialized) {
        return 0;
    }

    LdpPdu init = LdpPdu();
    createInitPdu(init);
    ssize_t rslt = send(init);

    if (rslt < 0) {
        changeState(LdpSessionState::Invalid);
        return rslt;
    }

    changeState(LdpSessionState::OpenSent);

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
        log_fatal("(%s:%u) failed to write pdu.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs);
        // send notify?
        changeState(LdpSessionState::Invalid);
        return res;
    }

    _last_send = _ldpd->now();

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
    session.setKeepaliveTime(_ldpd->getKeepaliveTime());

    LdpRawTlv *tlv = new LdpRawTlv();
    tlv->setValue(&session);

    init->addTlv(tlv);
    init->recalculateLength();

    to.addMessage(init);

    fillPduHeader(to);
}

int LdpFsm::processInit(const LdpMessage *init) {
    const LdpRawTlv *session = init->getTlv(LDP_TLVTYPE_COMMON_SESSION);

    if (session == nullptr) {
        log_error("(%s:%u) common session params tlv not found in init msg.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs);
        return -1;
    }

    LdpCommonSessionParamsTlvValue *params = (LdpCommonSessionParamsTlvValue  *) session->getParsedValue();

    if (params->loopDetection()) { // todo
        log_error("loop detection not yet implemented.\n");
        return -1;
    }

    uint16_t keep = params->getKeepaliveTime();

    if (_keep > keep) {
        _keep = keep;
    }

    if (_keep = 0) {
        _keep = 15;
    }

    uint32_t id = params->getReceiverRouterId();
    uint32_t space = params->getReceiverLabelSpace();

    delete params;

    if (id != _ldpd->getRouterId() || space != _ldpd->getLabelSpace()) {
        log_error("(%s:%u) target is not us.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs);
        // no us? todo: send notify
        return -1;
    }
}

void LdpFsm::tick() {
    if (_state == LdpSessionState::Operational) {
        if ((_ldpd->now() - _last_send) > _keep / 2) {
            sendKeepalive();
        }

        if ((_ldpd->now() - _last_recv) > _keep) {
            log_error("(%s:%u) hold timer expired.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs);
            // todo
        }
    }
}

void LdpFsm::changeState(LdpSessionState newState) {
    log_debug("(%s:%u) changing state %s -> %s\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs, LdpSessionStateText[_state], LdpSessionStateText[newState]);
    _state = newState;
}

}