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

    const char *nei_id_str = inet_ntoa(*(struct in_addr *) &_neighId);

    _last_recv = _ldpd->now();

    if (parsed_len < 0) {
        changeState(LdpSessionState::Invalid);
        sendNotification(0, 0, LDP_SC_SHUTDOWN);
        return parsed_len;
    }

    for (const LdpMessage *msg : pdu.getMessages()) {
        if (_state == LdpSessionState::Invalid) {
            log_fatal("(%s:%u) this fsm should be deleted and the tcp session should be closed.\n", nei_id_str, _neighLs);
            changeState(LdpSessionState::Invalid);
            return -1;
        }

        if (_state == Initialized) {
            if (msg->getType() != LDP_MSGTYPE_INITIALIZE) {
                log_error("(%s:%u) got message of type 0x%.4x in init state.\n", nei_id_str, _neighLs, msg->getType());
                changeState(LdpSessionState::Invalid);
                sendNotification(0, 0, LDP_SC_SHUTDOWN);
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
                log_error("(%s:%u) got message of type 0x%.4x in open-received state.\n", nei_id_str, _neighLs, msg->getType());
                changeState(LdpSessionState::Invalid);
                sendNotification(0, 0, LDP_SC_SHUTDOWN);
                return -1;
            }

            _ldpd->handleNewSession(this);
            changeState(LdpSessionState::Operational);
            continue;
        }

        if (_state == OpenSent) {
            if (msg->getType() != LDP_MSGTYPE_INITIALIZE) {
                log_error("(%s:%u) got message of type 0x%.4x in open-sent state.\n", nei_id_str, _neighLs, msg->getType());
                changeState(LdpSessionState::Invalid);
                sendNotification(0, 0, LDP_SC_SHUTDOWN);
                return -1;
            }

            if (processInit(msg) < 0) {
                changeState(LdpSessionState::Invalid);
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
            if (msg->getType() == LDP_MSGTYPE_KEEPALIVE) {
                continue;
            }

            ssize_t rslt = _ldpd->handleMessage(this, msg);

            if (rslt < 0) {
                changeState(LdpSessionState::Invalid);
                return rslt;
            }

            continue;
        }

        sendNotification(0, 0, LDP_SC_INTERNAL_ERROR);
        log_fatal("(%s:%u) fsm in invalid state: %d.\n", nei_id_str, _neighLs, _state);
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

    res = _ldpd->transmit(this, buffer, len);
    free(buffer);

    return res;
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

ssize_t LdpFsm::sendNotification(uint32_t msgid, uint16_t msgtype, uint32_t code) {
    LdpPdu pdu = LdpPdu();

    LdpMessage *note = new LdpMessage();

    note->setType(LDP_MSGTYPE_NOTIFICATION);
    note->setId(_ldpd->getNextMessageId());
    
    LdpRawTlv *status = new LdpRawTlv();
    status->setType(LDP_TLVTYPE_STATUS);

    LdpStatusTlvValue status_val = LdpStatusTlvValue();
    status_val.setMessageId(msgid);
    status_val.setMessageType(msgtype);
    status_val.setStatusCode(code);

    status->setValue(&status_val);

    note->addTlv(status);
    note->recalculateLength();

    pdu.addMessage(note);

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
    const char *nei_id_str = inet_ntoa(*(struct in_addr *) &(_neighId));

    if (session == nullptr) {
        sendNotification(init->getId(), 0, LDP_SC_MISSING_MSG_PARAM);
        log_error("(%s:%u) common session params tlv not found in init msg.\n", nei_id_str, _neighLs);
        return -1;
    }

    LdpCommonSessionParamsTlvValue *params = (LdpCommonSessionParamsTlvValue  *) session->getParsedValue();

    if (params == nullptr) {
        sendNotification(init->getId(), 0, LDP_SC_MALFORMED_TLV_VAL);
        log_error("(%s:%u) cannot understand common session param tlv value.\n", nei_id_str, _neighLs);
        return -1;
    }

    if (params->loopDetection()) { // todo
        changeState(LdpSessionState::Invalid);
        sendNotification(init->getId(), params->getType(), LDP_SC_INTERNAL_ERROR);
        log_error("loop detection not yet implemented.\n");
        return -1;
    }

    uint16_t keep = params->getKeepaliveTime();

    if (keep == 0) {
        changeState(LdpSessionState::Invalid);
        sendNotification(init->getId(), params->getType(), LDP_SC_BAD_KEEPALIVE);
        log_error("(%s:%u) sent a invalid keepalive timer.\n", nei_id_str, _neighLs);
        return -1;
    }

    if (_keep > keep) {
        _keep = keep;
    }

    uint32_t id = params->getReceiverRouterId();
    uint32_t space = params->getReceiverLabelSpace();

    delete params;

    if (id != _ldpd->getRouterId() || space != _ldpd->getLabelSpace()) {
        changeState(LdpSessionState::Invalid);
        sendNotification(init->getId(), 0, LDP_SC_SESSION_REJ_NOHELLO);
        log_error("(%s:%u) target is not us.\n", nei_id_str, _neighLs);
        return -1;
    }

    log_debug("(%s:%u) session params: keep = %u.\n", nei_id_str, _neighLs, _keep);

    return 0;
}

void LdpFsm::tick() {
    if (_state == LdpSessionState::Operational) {
        if ((_ldpd->now() - _last_send) > _keep - 10) {
            sendKeepalive();
        }

        if ((_ldpd->now() - _last_recv) > _keep) {
            log_error("(%s:%u) hold timer expired.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs);
            _ldpd->shutdownSession(this, LDP_SC_KEEPALIVE_EXPIRED);
        }
    }
}

void LdpFsm::changeState(LdpSessionState newState) {
    log_debug("(%s:%u) changing state %s -> %s.\n", inet_ntoa(*(struct in_addr *) &_neighId), _neighLs, LdpSessionStateText[_state], LdpSessionStateText[newState]);
    _state = newState;
}

}