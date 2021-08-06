#include "utils/log.hh"
#include "core/ldpd.hh"
#include "ldp-fsm/ldp-fsm.hh"
#include "ldp-tlv/ldp-tlv.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include <algorithm>

namespace ldpd {

Ldpd::Ldpd(uint32_t routerId, uint16_t labelSpace, Router *router, int metric) : 
    _import(FilterAction::Reject), _export(FilterAction::Accept), _ldp_ifaces(),
    _fsms(), _fds(), _hellos(), _holds(), _transports(), _addresses(),
    _mappings(), _rejected_mappings(), _pending_delete_mappings(), _ifaces(),
    _srcs() {

    _running = false;
    _id = routerId;
    _space = labelSpace;
    _transport = _id;

    _hold = 15;
    _hello = 5;
    _keep = 45;
    _ifscan = 300;

    _tfd = -1;
    _ufd = -1;

    _last_hello = 0;
    _now = time(nullptr);

    _router = router;

    _msg_id = 0;

    _metric = metric;

    _router->onRouteChange(this, Ldpd::handleRouteChange);
}

Ldpd::~Ldpd() {
    stop();
}

void Ldpd::addInterface(std::string ifname) {
    _ldp_ifaces.push_back(ifname);
}

int Ldpd::start() {
    if (_running) {
        log_warn("already running.\n");
        return 1;
    }

    scanInterfaces(); // todo: listen to changes instead of pulling
    createLocalMappings();

    bool transport_local = false;

    for (const Interface &iface : _ifaces) {
        for (const InterfaceAddress &addr : iface.addresses) {
            if (addr.address.prefix == _transport) {
                transport_local = true;
                break;
            }
        }

        if (transport_local) {
            break;
        }
    }

    if (!transport_local) {
        log_fatal("transport address not found on local interfaces.\n");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(LDP_PORT);

    _tfd = socket(AF_INET, SOCK_STREAM, 0);

    if (_tfd < 0) {
        log_fatal("socket(): %s.\n", strerror(errno));
        return 1;
    }

    int ttl = 255;
    if (setsockopt(_tfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        log_fatal("setsockopt(): %s.\n", strerror(errno));
        return 1;
    }

    // FIXME: remove this when not in debug
    int optval = 1;
    setsockopt(_tfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval , sizeof(int));

    if (bind(_tfd, (const sockaddr *) &addr, sizeof(addr)) < 0) {
        log_fatal("bind(): %s.\n", strerror(errno));
        return 1;
    }

    if (listen(_tfd, LDP_TCP_BACKLOG) < 0) {
        log_fatal("listen(): %s.\n", strerror(errno));
        return 1;
    }

    _ufd = socket(AF_INET, SOCK_DGRAM, 0);

    if (_ufd < 0) {
        log_fatal("socket(): %s.\n", strerror(errno));
        return 1;
    }

    if (bind(_ufd, (const sockaddr *) &addr, sizeof(addr)) < 0) {
        log_fatal("bind(): %s.\n", strerror(errno));
        return 1;
    }

    ttl = 1;
    if (setsockopt(_ufd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        log_fatal("setsockopt(): %s.\n", strerror(errno));
        return 1;
    }

    int pktinfo = 1;
    if (setsockopt(_ufd, IPPROTO_IP, IP_PKTINFO, &pktinfo, sizeof(pktinfo)) < 0) {
        log_fatal("setsockopt(): %s.\n", strerror(errno));
        return 1;
    }

    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));

    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    mreq.imr_multiaddr.s_addr = htonl(INADDR_ALLRTRS_GROUP);

    if (setsockopt(_ufd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        log_fatal("setsockopt(): %s.\n", strerror(errno));
        return 1;
    }

    _running = true;
    return 0;
}

int Ldpd::stop() {
    if (!_running) {
        return 0;
    }

    log_info("shutting down...\n");

    _running = false;

    close(_ufd);
    close(_tfd);

    for (std::pair<uint64_t, LdpFsm *> fsm : _fsms) {
        shutdownSession(fsm.second);
        delete fsm.second;
    }

    _fsms.clear();
    _holds.clear();
    _fds.clear();
    _transports.clear();

    return 0;
}

void Ldpd::tick() {
    _now = time(nullptr);

    if (_now - _last_hello > _hello) {
        sendHello();
    }

    if (_now - _last_scan > _ifscan) {
        scanInterfaces();
    }

    for (std::map<uint64_t, time_t>::iterator hello = _hellos.begin(); hello != _hellos.end(); ) {
        if (_now - hello->second > getHoldTime(hello->first)) {
            uint32_t nei_id = (uint32_t) (hello->first >> sizeof(uint16_t));

            log_info("hello adj with %s removed - hold expired.\n", inet_ntoa(*(struct in_addr *) &nei_id));

            if (_holds.count(hello->first)) {
                _holds[hello->first] = 0xffff;
            }

            hello = _hellos.erase(hello);
        } else {
            ++hello;
        }
    }

    // todo: check where each peer at which iface & send out only on those iface?
    for (std::pair<uint64_t, uint16_t> hold : _holds) {
        if (_last_hello < _now && _now - _last_hello > hold.second / 2) {
            sendHello();
        }
    }

    for (std::pair<uint64_t, LdpFsm *> fsm : _fsms) {
        fsm.second->tick();
    }

    refreshMappings();

    _router->tick();
}

void Ldpd::run() {
    while(_running) {
        fd_set fds;
        struct timeval tv;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&fds);

        FD_SET(_tfd, &fds);
        FD_SET(_ufd, &fds);

        int max_fd = _tfd > _ufd ? _tfd : _ufd;

        for (std::pair<int, LdpFsm *> _fd : _fds) {
            if (_fd.first > max_fd) {
                max_fd = _fd.first;
            }
            FD_SET(_fd.first, &fds);
        }

        int ret = select(max_fd + 1, &fds, nullptr, nullptr, &tv);
        
        tick();

        if (ret < 0) {
            if (errno == EAGAIN) {
                continue;
            }

            log_error("select(): %s.\n", strerror(errno));
            continue;
        }

        // timed out
        if (ret == 0) {
            continue;
        }

        if (FD_ISSET(_tfd, &fds)) {
            handleSession();
            continue;
        }

        if (FD_ISSET(_ufd, &fds)) {
            handleHello();
            continue;
        }

        // FIXME: _fds may change after handleSession call - but don't fix it like this
        std::vector<int> pending_fds = std::vector<int>();

        for (std::pair<int, LdpFsm *> _fd : _fds) {
            if (FD_ISSET(_fd.first, &fds)) {
                pending_fds.push_back(_fd.first);
            }
        }

        for (int &fd : pending_fds) {
            handleSession(fd);
        }
    }
}

uint32_t Ldpd::getRouterId() const {
    return _id;
}

uint32_t Ldpd::getNextMessageId() {
    return ++_msg_id;
}

uint16_t Ldpd::getLabelSpace() const {
    return _space;
}

uint32_t Ldpd::getTransportAddress() const {
    return _transport;
}

uint16_t Ldpd::getKeepaliveTime() const {
    return _keep;
}

void Ldpd::setTransportAddress(uint32_t address) {
    _transport = address;
}

void Ldpd::setKeepaliveTimer(uint16_t timer) {
    _keep = timer;
}

ssize_t Ldpd::handleMessage(LdpFsm* from, const LdpMessage *msg) {
    uint32_t nei_id = from->getNeighborId();
    uint64_t key = LDP_KEY(nei_id, from->getNeighborLabelSpace());

    const char *nei_id_str = inet_ntoa(*(struct in_addr *) &(nei_id));

    if (msg->getType() == LDP_MSGTYPE_NOTIFICATION) {
        log_info("got notification from %s.\n", nei_id_str);

        const LdpRawTlv *status = msg->getTlv(LDP_TLVTYPE_STATUS);

        if (status == nullptr) {
            log_error("notification from %s does not have a status tlv.\n", nei_id_str);
            from->sendNotification(msg->getId(), 0, LDP_SC_MISSING_MSG_PARAM);
            return -1;
        }

        LdpStatusTlvValue *status_val = (LdpStatusTlvValue *) status->getParsedValue();

        if (status_val == nullptr) {
            log_error("cannot understand the status tlv in notification from %s.\n", nei_id_str);
            from->sendNotification(msg->getId(), status->getType(), LDP_SC_MALFORMED_TLV_VAL);
            return -1;
        }

        log_info("status code: %u (%s).\n", status_val->getStatusCode(), status_val->getStatusCodeText());

        if (status_val->getStatusCode() == LDP_SC_SHUTDOWN) {
            // for shutdown msg - we should reply.
            from->sendNotification(0, 0, LDP_SC_SHUTDOWN);
        }

        if (status_val->fatal()) {
            log_error("the notification states fatal error - shutting down session.\n");
            delete status_val;
            return -1;
        }

        // todo

        delete status_val;
    }

    if (msg->getType() == LDP_MSGTYPE_HELLO) {
        log_error("got hello from ldp session with %s (the tcp one), what?\n", nei_id_str);
        from->sendNotification(msg->getId(), 0, LDP_SC_SHUTDOWN);
        return -1;
    }

    if (msg->getType() == LDP_MSGTYPE_INITIALIZE) {
        log_error("got init from ldp session with %s, but session is already operational.\n", nei_id_str);
        from->sendNotification(msg->getId(), 0, LDP_SC_SHUTDOWN);
        return -1;
    }

    if (msg->getType() == LDP_MSGTYPE_KEEPALIVE) {
        log_warn("got keepalive from session with %s - this should have been handlded by the fsm.\n", nei_id_str);
        return msg->length();
    }

    if (msg->getType() == LDP_MSGTYPE_ADDRESS) {
        log_debug("got address list from ldp session with %s.\n", nei_id_str);

        const LdpRawTlv *addrs = msg->getTlv(LDP_TLVTYPE_ADDRESS_LIST);

        if (addrs == nullptr) {
            log_error("address mseesge from %s does not have a address-list tlv.\n", nei_id_str);
            from->sendNotification(msg->getId(), 0, LDP_SC_MISSING_MSG_PARAM);
            return -1;
        }

        LdpAddressTlvValue *addrs_val = (LdpAddressTlvValue *) addrs->getParsedValue();

        if (addrs_val == nullptr) {
            log_error("cannot understand the address-list tlv in address message from %s.\n", nei_id_str);
            from->sendNotification(msg->getId(), addrs->getType(), LDP_SC_MALFORMED_TLV_VAL);
            return -1;
        }

        _addresses[key] = std::vector<uint32_t>(addrs_val->getAddresses());

        for (uint32_t &addr : _addresses[key]) {
            log_debug("address: %s.\n", inet_ntoa(*(struct in_addr *) &(addr)));
        }

        delete addrs_val;

        return msg->length();
    }

    if (msg->getType() == LDP_MSGTYPE_ADDRESS_WITHDRAW) { // todo
        log_debug("got address withdraw from ldp session with %s.\n", nei_id_str);

        return msg->length();
    }

    if (msg->getType() == LDP_MSGTYPE_LABEL_MAPPING || msg->getType() == LDP_MSGTYPE_LABEL_WITHDRAW) {
        const char *msgname = msg->getType() == LDP_MSGTYPE_LABEL_MAPPING ? "lbl mapping" : "lbl withdraw";

        log_debug("got %s message from %s.\n", msgname, nei_id_str);

        const LdpRawTlv *fec = msg->getTlv(LDP_TLVTYPE_FEC);

        if (fec == nullptr) {
            log_error("%s mseesge from %s does not have a fec tlv.\n", msgname, nei_id_str);
            from->sendNotification(msg->getId(), 0, LDP_SC_MISSING_MSG_PARAM);
            return -1;
        }

        LdpFecTlvValue *fec_val = (LdpFecTlvValue *) fec->getParsedValue();

        if (fec_val == nullptr) {
            log_error("cannot understand the fec tlv in %s message from %s.\n", msgname, nei_id_str);
            from->sendNotification(msg->getId(), fec->getType(), LDP_SC_MALFORMED_TLV_VAL);
            return -1;
        }

        const LdpRawTlv *lbl = msg->getTlv(LDP_TLVTYPE_GENERIC_LABEL); // todo: other label?

        if (lbl == nullptr) {
            log_error("%s mseesge from %s does not have a label tlv.\n", msgname, nei_id_str);
            from->sendNotification(msg->getId(), 0, LDP_SC_MISSING_MSG_PARAM);
            return -1;
        }

        LdpGenericLabelTlvValue *lbl_val = (LdpGenericLabelTlvValue *) lbl->getParsedValue();

        if (lbl_val == nullptr) {
            log_error("cannot understand the label tlv in %s message from %s.\n", msgname, nei_id_str);
            from->sendNotification(msg->getId(), lbl->getType(), LDP_SC_MALFORMED_TLV_VAL);
            return -1;
        }

        if (_mappings.count(key) == 0) {
            _mappings[key] = std::vector<LdpLabelMapping>();
        }

        log_debug("label: %u\n", lbl_val->getLabel());

        for (const LdpFecElement *el : fec_val->getElements()) {
            LdpLabelMapping mapping = LdpLabelMapping();
            mapping.remote = true;
            mapping.out_label = lbl_val->getLabel();

            if (el->getType() == 0x01) {
                mapping.fec.prefix = 0;
                mapping.fec.len = 0;
                
                log_debug("prefix: 0.0.0.0/0.\n");
            }

            if (el->getType() == 0x02) {
                const LdpFecPrefixElement *e = (LdpFecPrefixElement *) el;

                mapping.fec.len = e->getPrefixLength();
                mapping.fec.prefix = e->getPrefix();

                log_debug("prefix: %s/%d.\n", inet_ntoa(*(struct in_addr *) &(mapping.fec.prefix)), mapping.fec.len);
            }

            if (msg->getType() == LDP_MSGTYPE_LABEL_MAPPING) {
                _mappings[key].push_back(mapping);
            }

            if (msg->getType() == LDP_MSGTYPE_LABEL_WITHDRAW) {
                // TODO
            }

        }

        if (msg->getType() == LDP_MSGTYPE_LABEL_WITHDRAW) {
            LdpPdu pdu = LdpPdu();

            LdpMessage *release_msg = new LdpMessage();

            release_msg->setType(LDP_MSGTYPE_LABEL_RELEASE);
            pdu.addMessage(release_msg);

            LdpRawTlv *fec = new LdpRawTlv();
            fec->setValue(fec_val);

            release_msg->addTlv(fec);

            LdpRawTlv *lbl = new LdpRawTlv();
            lbl->setValue(lbl_val);

            release_msg->addTlv(lbl);
            release_msg->recalculateLength();

            from->send(pdu);
        }
        
        delete lbl_val;
        delete fec_val;

        return msg->length();
    }

    if (msg->getType() == LDP_MSGTYPE_LABEL_REQUEST) { // todo
        log_debug("got label request from ldp session with %s.\n", nei_id_str);

        return msg->length();
    }

    if (msg->getType() == LDP_MSGTYPE_LABEL_RELEASE) { // todo
        log_debug("got label release from ldp session with %s.\n", nei_id_str);

        return msg->length();
    }

    if (msg->getType() == LDP_MSGTYPE_LABEL_ABORT) { // todo
        log_debug("got label abort from ldp session with %s.\n", nei_id_str);

        return msg->length();
    }

    from->sendNotification(msg->getId(), 0, LDP_SC_UNKNOWN_MSG_TYPE);
    return -1;
}

void Ldpd::shutdownSession(LdpFsm* of, int32_t code, uint32_t msgid, uint16_t msgtype) {
    uint64_t key = LDP_KEY(of->getNeighborId(), of->getNeighborLabelSpace());

    for (std::pair<int, LdpFsm *> _fd : _fds) {
        if (_fd.second == of) {
            log_info("closing fd %d.\n", _fd.first);
            if (code > 0) {
                of->sendNotification(msgid, msgtype, (uint32_t) code);
            }          
            
            close(_fd.first);
        }
    }

    _exported_mappings.erase(key);


}

void Ldpd::removeSession(LdpFsm* of) {
    std::map<int, LdpFsm *>::iterator fdit = _fds.begin();
    std::map<uint64_t, LdpFsm *>::iterator sit = _fsms.begin();

    bool fd_del = false;
    for (; fdit != _fds.end(); ++fdit) {
        if (fdit->second == of) {
            fd_del = true;
            delete fdit->second;
            _fds.erase(fdit);
            break;
        }
    }

    uint64_t key = 0;
    for (; sit != _fsms.end(); ++sit) {
        if (sit->second == of) {
            key = sit->first;
            if(!fd_del) {
                delete sit->second;
            }
            _fsms.erase(sit);
            break;
        }
    }

    _addresses.erase(key);

    if (_mappings.count(key) != 0) {
        for (const LdpLabelMapping &mapping : _mappings[key]) {
            _pending_delete_mappings.push_back(mapping);
        }

        _mappings.erase(key);
    }

    uint32_t nei_id = (uint32_t) (key >> sizeof(uint16_t));

    log_info("session with %s removed.\n", inet_ntoa(*(struct in_addr *) &nei_id));
}

void Ldpd::handleHello() {
    // TODO: targeted hellos?
    uint8_t buffer[8192];
    uint8_t control[128];

    struct msghdr message;
    struct sockaddr_in remote;
    struct iovec iov;

    memset(&message, 0, sizeof(message));
    memset(&remote, 0, sizeof(remote));
    memset(&iov, 0, sizeof(iov));

    iov.iov_base = buffer;
    iov.iov_len = sizeof(buffer);

    message.msg_name = &remote;
    message.msg_namelen = sizeof(remote);
    message.msg_iov = &iov;
    message.msg_iovlen = 1;
    message.msg_control = control;
    message.msg_controllen = sizeof(control);

    ssize_t len = recvmsg(_ufd, &message, 0);

    if (len < 0) {
        log_warn("recvmsg(): %s.\n", strerror(errno));
        return;
    }

    if (len == 0) {
        log_warn("recvmsg(): got eof.\n");
        return;
    }

    struct in_pktinfo *pktinfo = nullptr;

    for(struct cmsghdr *cmsg = CMSG_FIRSTHDR(&message); cmsg != nullptr; cmsg = CMSG_NXTHDR(&message, cmsg)) {
        if (cmsg->cmsg_type != IP_PKTINFO || cmsg->cmsg_level != IPPROTO_IP) {
            continue;
        }

        pktinfo = (struct in_pktinfo *) CMSG_DATA(cmsg);
        break;
    }

    if (pktinfo == nullptr) {
        log_error("recvmsg() result does not have a ip_pktinfo message.\n");
        return;
    }

    bool ldp_enabled = false;

    for (const std::string &ifname : _ldp_ifaces) {
        for (const Interface &iface : _ifaces) {
            if (iface.ifname == ifname && pktinfo->ipi_ifindex == iface.index) {
                ldp_enabled = true;
            }
        }
    }

    if (!ldp_enabled) {
        log_debug("got hello on interface %d, but ldp is not enabled on it.\n", pktinfo->ipi_ifindex);
        return;
    }

    // check if msg is from self.
    for (const Interface &iface : _ifaces) {
        for (const InterfaceAddress &addr : iface.addresses) {
            if (addr.address.prefix == remote.sin_addr.s_addr) {
                return;
            }
        }
    }

    LdpPdu pdu = LdpPdu();

    ssize_t res = pdu.parse(buffer, len);

    if (res < 0) {
        log_info("invalid pdu from %s:%u (cannot understand).\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        return;
    }

    const LdpMessage *hello = pdu.getMessage(LDP_MSGTYPE_HELLO);

    if (hello == nullptr) {
        log_info("invalid pdu from %s:%u (no hello body).\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        return;
    }

    uint32_t nei_id = pdu.getRouterId();
    uint16_t nei_ls = pdu.getLabelSpace();

    if (nei_id == _id) {
        log_error("invalid pdu from %s: they have the same router-id as us.\n", inet_ntoa(remote.sin_addr));
        return;
    }

    uint64_t key = LDP_KEY(nei_id, nei_ls);

    const LdpRawTlv *params = hello->getTlv(LDP_TLVTYPE_COMMON_HELLO);

    if (params == nullptr) {
        log_info("invalid hello msg from %s:%u (no hello params tlv).\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        return;
    }

    LdpCommonHelloParamsTlvValue *params_val = (LdpCommonHelloParamsTlvValue *) params->getParsedValue();

    if (params_val == nullptr) {
        log_info("invalid hello msg from %s:%u (cannot understand hello params tlv).\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        return;
    }

    _holds[key] = params_val->getHoldTime();

    // TODO: targeted / req_targeted, gtsm

    delete params_val;

    if (!_hellos.count(key) || _now - _hellos[key] > _hold) {
        log_info("got a new hello from %s:%u.\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        log_info("their id: %s:%u.\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_ls);
    } 

    _hellos[key] = _now;

    const LdpRawTlv *ta_tlv = hello->getTlv(LDP_TLVTYPE_IPV4_TRANSPORT);

    if (ta_tlv == nullptr) {
        _transports[key] = remote.sin_addr.s_addr;
        return;
    }

    const LdpIpv4TransportAddressTlvValue *ta_tlv_val = (const LdpIpv4TransportAddressTlvValue *) ta_tlv->getParsedValue();

    if (ta_tlv_val == nullptr) {
        log_warn("%s:%u included a transport-address tlv in hello, but we don't understand it.\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_ls);
        return;
    }

    uint32_t ta = ta_tlv_val->getAddress();

    delete ta_tlv_val;

    if (_transports.count(key) == 0 || _transports[key] != ta) {
        log_info("learned transport address for %s:%u.\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_ls);
        log_info("transport address: %s.\n", inet_ntoa(*(struct in_addr *) &ta));
        _transports[key] = ta;

        if (_fsms.count(key) == 0 && ntohl(_transport) < ntohl(ta)) {
            log_debug("no running session with %s:%d but they have higher transport-address - not sending init.\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_ls);
        }
    }

    if (_fsms.count(key) == 0) {
        if (ntohl(_transport) > ntohl(ta)) {
            log_debug("no running session with %s:%d and we have higher transport-address, sending init...\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_ls);
            createSession(nei_id, nei_ls);
        }
    }
}

void Ldpd::handleSession() {
    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));

    socklen_t addrlen = sizeof(remote);

    int fd = accept(_tfd, (struct sockaddr *) &remote, &addrlen);

    if (fd < 0) {
        log_warn("accept(): %s.\n", strerror(errno));
        return;
    }

    if (remote.sin_family != AF_INET) {
        log_warn("accept(): got non-ipv4?\n");
        return;
    }

    log_info("new tcp connection from %s:%u.\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));

    uint8_t buffer[8192];
    ssize_t len = read(fd, buffer, sizeof(buffer));

    // FIXME: make sure it has atleast one full pdu?

    if (len < 0) {
        log_warn("read(): %s.\n", strerror(errno));
        return;
    }

    if (len == 0) {
        log_warn("read(): got eof.\n");
        return;
    }

    LdpFsm *session = new LdpFsm(this);
    _fds[fd] = session;
    
    int offset = 0;

    while (len > 0) {
        ssize_t ret = session->receive(buffer + offset, len);

        if (ret < 0 || session->getState() < LdpSessionState::OpenReceived) {
            // if recv failed, or state changed, fsm has sent out notification. we don't need to send again.
            shutdownSession(session, -1);
            removeSession(session);
            return;
        }

        offset += ret;
        len -= ret;
    }

    uint32_t nei_id = session->getNeighborId();
    uint16_t nei_space = session->getNeighborLabelSpace();

    log_info("neigh lsr-id: %s:%u.\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_space);

    uint32_t key = LDP_KEY(nei_id, nei_space);

    if (_hellos.count(key) == 0 || _now - _hellos[key] > getHoldTime(key)) {
        log_warn("no hello from them or hold expired. rejecting.\n");

        shutdownSession(session, LDP_SC_SESSION_REJ_NOHELLO);
        removeSession(session);
        return;
    }
    
    if (_fsms.count(key) != 0) {
        log_warn("a session with a router w/ same lrs-id exists? rejecting.\n");
        close(fd);
        return;
    }

    _fsms[key] = session;

    log_info("looks good, registering them.\n");

}

void Ldpd::handleSession(int fd) {
    uint8_t buffer[8192];
    ssize_t len = read(fd, buffer, sizeof(buffer));

    if (_fds.count(fd) == 0) {
        return;
    }

    LdpFsm *session = _fds[fd];

    // FIXME: make sure it has atleast one full pdu?

    if (len < 0) {
        log_warn("read(): %s.\n", strerror(errno));
        return;
    }

    if (len == 0) {
        log_error("read(): got eof - removing.\n");
        removeSession(session);
        return;
    }

    int offset = 0;
    while (len > 0) {
        // FIXME: buffer read from fd may have .5 or 1.5 pkt - need to save partial pkt.
        ssize_t ret = session->receive(buffer + offset, len);

        if (ret < 0 || session->getState() == LdpSessionState::Invalid) {
            shutdownSession(session);
            removeSession(session);
            return;
        }

        offset += ret;
        len -= ret;
    }
}

void Ldpd::sendHello() {
    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));

    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(INADDR_ALLRTRS_GROUP);
    remote.sin_port = htons(LDP_PORT);

    socklen_t addrlen = sizeof(remote);

    LdpPdu pdu = LdpPdu();
    
    pdu.setRouterId(_id);
    pdu.setLabelSpace(_space);

    LdpMessage *hello = new LdpMessage();

    LdpRawTlv *common = new LdpRawTlv();
    LdpCommonHelloParamsTlvValue common_val = LdpCommonHelloParamsTlvValue();
    common_val.setHoldTime(_hold);
    common->setValue(&common_val);

    LdpRawTlv *ta = new LdpRawTlv();
    LdpIpv4TransportAddressTlvValue ta_val = LdpIpv4TransportAddressTlvValue();
    ta_val.setAddress(_transport);
    ta->setValue(&ta_val);

    LdpRawTlv *cs = new LdpRawTlv();
    LdpConfigSeqNumTlvValue cs_vl = LdpConfigSeqNumTlvValue();

    // FIXME: track local config change.
    cs_vl.setSeq(114514);
    cs->setValue(&cs_vl);

    hello->setType(LDP_MSGTYPE_HELLO);

    hello->addTlv(common);
    hello->addTlv(ta);
    hello->addTlv(cs);

    hello->setId(getNextMessageId());
    hello->recalculateLength();

    pdu.addMessage(hello);
    pdu.recalculateLength();

    size_t len = pdu.length();
    
    uint8_t *buffer = (uint8_t *) malloc(len);

    pdu.write(buffer, len);

    for (const std::string &ifname : _ldp_ifaces) {
        uint32_t outaddr = 0;

        for (const Interface &iface : _ifaces) {
            if (iface.ifname == ifname && iface.addresses.size() > 0) {
                outaddr = iface.addresses[0].address.prefix;
            }
        }

        if (outaddr == 0) {
            log_error("no interface with name %s, or no valid ip address on it.\n", ifname.c_str());
            continue;
        }

        if (setsockopt(_ufd, IPPROTO_IP, IP_MULTICAST_IF, &outaddr, sizeof(outaddr)) < 0) {
            log_error("setsockopt(): %s.\n", strerror(errno));
            continue;
        }

        ssize_t res = sendto(_ufd, buffer, len, 0, (struct sockaddr *) &remote, addrlen);
        if (res < 0) {
            log_error("sendto(): %s.\n", strerror(errno));
        }
    }



    _last_hello = _now;

    free(buffer);
}

ssize_t Ldpd::transmit(LdpFsm* by, const uint8_t *buffer, size_t len) {
    for (std::pair<int, LdpFsm *> fd : _fds) {
        if (fd.second == by) {
            return write(fd.first, buffer, len);
        }
    }

    log_error("got transmit request from unknow session.\n");

    return -1;
}

void Ldpd::createSession(uint32_t nei_id, uint16_t nei_ls) {
    uint64_t key = LDP_KEY(nei_id, nei_ls);

    log_info("creating new session with lsr-id: %s:%u...\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_ls);

    if (_transports.count(key) == 0 || _fsms.count(key) != 0) {
        log_warn("create-s called when no hello from remote, or session already exist.\n");
        return;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        log_error("socket(): %s.\n", strerror(errno));
        return;
    }

    struct sockaddr_in remote, local;

    memset(&remote, 0, sizeof(remote));
    memset(&local, 0, sizeof(local));

    remote.sin_addr.s_addr = _transports[key];
    remote.sin_family = AF_INET;
    remote.sin_port = htons(LDP_PORT);

    local.sin_addr.s_addr = _transport;
    local.sin_family = AF_INET;

    if (bind(fd, (const sockaddr *) &local, sizeof(local)) < 0) {
        log_error("bind(): %s.\n", strerror(errno));
        return;
    }

    if (connect(fd, (const sockaddr *) &remote, sizeof(local)) < 0) {
        log_error("connect(): %s.\n", strerror(errno));
        return;
    }

    int ttl = 255;
    if (setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        log_fatal("setsockopt(): %s.\n", strerror(errno));
        return;
    }

    log_info("tcp connection to %s:%u established.\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));

    LdpFsm *session = new LdpFsm(this);

    _fsms[key] = session;
    _fds[fd] = session;

    session->init(nei_id, nei_ls);
}

void Ldpd::scanInterfaces() {
    log_debug("scanning interfaces...\n");
    _last_scan = _now;
    _ifaces = _router->getInterfaces();
}

time_t Ldpd::now() const {
    return _now;
}

uint16_t Ldpd::getHoldTime(uint64_t of) {
    if (_holds.count(of) == 0) {
        return _hold;
    }

    uint16_t peer_hold = _holds[of];

    if (peer_hold == 0) {
        peer_hold = LDP_DEF_HELLO_HOLD;
    }

    return peer_hold < _hold ? peer_hold : _hold;
}

void Ldpd::installMapping(uint64_t key, LdpLabelMapping &mapping) {
    if (!mapping.remote) {
        log_error("this method handles remote mapping only.\n");
        return;
    }


    if (_addresses.count(key) == 0) {
        log_error("mapping exists, but no addresses?\n");
        return;
    }

    if (!shouldInstall(mapping, key)) {
        return;
    }

    std::vector<uint32_t> addresses = _addresses[key];

    bool local = false, filtered = false;
    Interface *nh_iface = nullptr;
    uint32_t nh_address = 0;

    for (Interface &iface : _ifaces) {
        for (InterfaceAddress &addr : iface.addresses) {
            Prefix peer = Prefix();
            peer.len = 32;
            
            for (uint32_t &address : addresses) {
                peer.prefix = address;

                if (addr.address.includes(peer)) {
                    nh_address = address;
                    nh_iface = &iface;
                    break;
                }

                if (addr.address.isInSameNetwork(mapping.fec)) {
                    local = true;
                    break;
                }
            }

            if (nh_iface != nullptr || local) {
                break;
            }
        }

        if (nh_iface != nullptr || local) {
            break;
        }
    }

    if (_export.apply(mapping.fec) != FilterAction::Accept) {
        log_info("mapping rejected by export filter: %s/%u.\n", inet_ntoa(*(struct in_addr *) &(mapping.fec.prefix)), mapping.fec.len);
        // todo: reject mapping??
        filtered = true;
    }

    if (local || filtered) {
        mapping.hidden = true;
        return;
    }

    if (nh_iface == nullptr) {
        log_error("cannot find a way to reach the neighbor.\n");
        return;
    }

    Ipv4Route *ir = new Ipv4Route();

    ir->gw = nh_address;
    ir->oif = nh_iface->index;
    ir->dst = mapping.fec.prefix;
    ir->dst_len = mapping.fec.len;
    ir->metric = _metric;

    log_debug("adding route: %s/%u.\n", inet_ntoa(*(struct in_addr *) &(ir->dst)), ir->dst_len);
    log_debug("gw: %s oif %d.\n", inet_ntoa(*(struct in_addr *) &(ir->gw)), ir->oif);

    if (mapping.out_label != 3) {
        ir->mpls_encap = true;
        ir->mpls_stack.push_back(mapping.out_label);
        log_debug("out label: %u.\n", mapping.out_label);
    }
    
    _router->addRoute(ir);

    mapping.in_label = getNextLabel();

    MplsRoute *mr = new MplsRoute();

    mr->in_label = mapping.in_label;
    mr->gw = nh_address;
    mr->oif = nh_iface->index;

    if (mapping.out_label != 3) {
        mr->mpls_encap = true;
        mr->mpls_stack.push_back(mapping.out_label);
    }

    log_debug("in label: %u.\n", mapping.in_label);

    _router->addRoute(mr);
}

void Ldpd::refreshMappings() {
    uint64_t self_key = LDP_KEY(_id, _space);

    for (std::map<uint64_t, std::vector<LdpLabelMapping>>::iterator i = _mappings.begin(); i != _mappings.end(); ++i) {
        for (LdpLabelMapping &mapping : i->second) {
            if (self_key == i->first) {
                continue;
            }

            installMapping(i->first, mapping);
        }
    }

    int lo_ifid = -1;

    for (Interface &iface : _ifaces) {
        if (iface.loopback) {
            lo_ifid = iface.index;
        }
    }

    if (lo_ifid < 0) {
        log_error("cannot find loopback interface - don't know how to install label for local router.\n");
        return;
    }

    if (_mappings.count(self_key) == 0) {
        log_error("no local mappings vector? something is not quite right.\n");
        return;
    }

    std::vector<LdpLabelMapping> local_mappings = _mappings[self_key];

    for (const LdpLabelMapping &mapping : local_mappings) {
        if (!shouldInstall(mapping)) {
            continue;
        }

        log_debug("adding route for delivering traffic for label %u locally...\n", mapping.in_label);

        MplsRoute *route = new MplsRoute();

        route->in_label = mapping.in_label;
        route->oif = lo_ifid;

        _router->addRoute(route);
    }

    for (std::vector<LdpLabelMapping>::iterator i = _pending_delete_mappings.begin(); i != _pending_delete_mappings.end(); i = _pending_delete_mappings.erase(i)) {
        if (i->remote) {
            Ipv4Route route = Ipv4Route();
            route.dst = i->fec.prefix;
            route.dst_len = i->fec.len;
        } else {
            MplsRoute route = MplsRoute();
            route.in_label = i->in_label;
            _router->deleteRoute(route.hash());
        }
    }

    for (std::pair<uint64_t, LdpFsm *> session : _fsms) {
        if (session.second->getState() != LdpSessionState::Operational) {
            continue;
        }

        LdpPdu pdu = LdpPdu();

        LdpMessage *mapping_msg = new LdpMessage();
        pdu.addMessage(mapping_msg);

        mapping_msg->setType(LDP_MSGTYPE_LABEL_MAPPING);
        mapping_msg->setId(getNextMessageId());

        uint64_t nei_key = session.first;
        uint64_t local_key = LDP_KEY(_id, _space);

        bool send = false;

        for (const std::pair<uint64_t, std::vector<LdpLabelMapping>> &mappings : _mappings) {
            uint64_t this_key = mappings.first;

            if (this_key == nei_key) {
                continue;
            }

            if (_exported_mappings.count(this_key) == 0) {
                _exported_mappings[this_key] = std::set<LdpLabelMapping>();
            }

            for (const LdpLabelMapping &mapping : mappings.second) {
                if (!mapping.remote && this_key != local_key) {
                    log_error("got non-remote mapping in non-local mapping db?\n");
                    continue;
                }

                if (mapping.hidden || _exported_mappings[this_key].count(mapping) != 0) {
                    continue;
                }

                _exported_mappings[this_key].insert(mapping);

                LdpRawTlv *fec = new LdpRawTlv();
                mapping_msg->addTlv(fec);

                LdpFecTlvValue fec_val = LdpFecTlvValue();

                LdpFecPrefixElement *pel = new LdpFecPrefixElement();

                pel->setPrefix(mapping.fec.prefix);
                pel->setPrefixLength(mapping.fec.len);

                fec_val.addElement(pel);
                
                fec->setValue(&fec_val);

                LdpRawTlv *lbl = new LdpRawTlv();

                LdpGenericLabelTlvValue lbl_val = LdpGenericLabelTlvValue();
                lbl_val.setLabel(mapping.in_label);

                lbl->setValue(&lbl_val);

                mapping_msg->addTlv(lbl);

                send = true;

                if (mapping.remote) {
                    log_debug("sending remote (transit) binding fec %s/%u swap %u with %u.\n", inet_ntoa(*(struct in_addr *) &(mapping.fec.prefix)), mapping.fec.len, mapping.in_label, mapping.out_label);
                } else {
                    log_debug("sending local binding %s/%u lbl %u.\n", inet_ntoa(*(struct in_addr *) &(mapping.fec.prefix)), mapping.fec.len, mapping.in_label);
                }

            }
        }

        if (send) {
            mapping_msg->recalculateLength();
            session.second->send(pdu);
        }
    }
}

void Ldpd::handleNewSession(LdpFsm* of) {
    // send address list, label mapping, etc.

    LdpPdu pdu = LdpPdu();

    LdpMessage *addr_msg = new LdpMessage();
    pdu.addMessage(addr_msg);
    
    addr_msg->setType(LDP_MSGTYPE_ADDRESS);
    addr_msg->setId(getNextMessageId());

    LdpRawTlv *addr_list_tlv = new LdpRawTlv();
    addr_msg->addTlv(addr_list_tlv);

    addr_list_tlv->setType(LDP_TLVTYPE_ADDRESS_LIST);
    
    LdpAddressTlvValue addr_list_val = LdpAddressTlvValue();

    // todo: handle interface/addr changes
    for (const Interface &iface : _ifaces) {
        if (std::find(_ldp_ifaces.begin(), _ldp_ifaces.end(), iface.ifname) != _ldp_ifaces.end()) {
            for (const InterfaceAddress &addr : iface.addresses) { // todo: filter out martian (127/8, etc)?
                addr_list_val.addAddress(addr.address.prefix);
            }
        }
    }

    if (addr_list_val.getAddresses().size() == 0) {
        log_warn("no addresses on any of the interfaces, what?\n");
        return;
    }

    addr_list_tlv->setValue(&addr_list_val);
    addr_msg->recalculateLength();

    // don't need recalc-len for pdu, fsm does it in send()
    of->send(pdu);
}

void Ldpd::setImportPolicy(const RoutePolicy &policy) {
    if (_running) {
        log_error("can't change import/export policy while running.\n");
        return;
    }

    _import = policy;
}

void Ldpd::setExportPolicy(const RoutePolicy &policy) {
    if (_running) {
        log_error("can't change import/export policy while running.\n");
        return;
    }

    _export = policy;
}

void Ldpd::handleRouteChange(void *self, RouteChange change, const Route *route) {
    Ldpd *ldpd = (Ldpd *) self;

    log_debug("todo: see if we need to send/withdraw mapping.\n");
}

void Ldpd::addRouteSource(RoutingProtocol proto) {
    _srcs.insert(proto);
}

uint32_t Ldpd::getNextLabel() const {
    std::set<uint32_t> used = std::set<uint32_t>();

    for (const std::pair<uint64_t, std::vector<LdpLabelMapping>> &mappings : _mappings) {
        for (const LdpLabelMapping &mapping : mappings.second) {
            used.insert(mapping.in_label);
        }
    }

    for (uint32_t i = LDP_MIN_LBL; i <= LDP_MAX_LBL; ++i) {
        if (used.count(i) == 0) {
            return i;
        }
    }

    log_error("we have run out of labels.\n");

    return 0xffffffff;
}

void Ldpd::createLocalMappings() {
    log_debug("creating local mappings...\n");

    uint64_t local_key = LDP_KEY(_id, _space);

    if (_mappings.count(local_key) != 0) {
        log_error("local mapping already exists. updates are handled by the change listeners.\n");
        return;
    }

    _mappings[local_key] = std::vector<LdpLabelMapping>();

    for (std::pair<uint64_t, Route *> r : _router->getFib()) {
        if (r.second->getType() != RouteType::Ipv4) {
            continue;
        }

        Ipv4Route *route = (Ipv4Route *) r.second;
        Prefix pfx = Prefix(route->dst, route->dst_len);

        if (_srcs.count(route->protocol) == 0) {
            log_debug("import reject %s/%u - protocol %u not allowed.\n", inet_ntoa(*(struct in_addr *) &(route->dst)), route->dst_len, route->protocol);
            continue;
        }

        if (_import.apply(pfx) != FilterAction::Accept) {
            log_debug("import reject %s/%u - rejected by filter.\n", inet_ntoa(*(struct in_addr *) &(route->dst)), route->dst_len);
            continue;
        }

        uint32_t label = getNextLabel();

        if (label > LDP_MAX_LBL) {
            return;
        }

        LdpLabelMapping mapping = LdpLabelMapping();

        mapping.remote = false;
        mapping.fec = pfx;
        mapping.in_label = label;

        _mappings[local_key].push_back(mapping);

        log_debug("created binding %s/%u lbl %u.\n", inet_ntoa(*(struct in_addr *) &(pfx.prefix)), pfx.len, label);
    }
}

bool Ldpd::shouldInstall(const LdpLabelMapping &mapping, uint64_t key) {
    if (mapping.hidden) {
        return false;
    }

    if (mapping.remote) {
        if (key == 0) {
            log_warn("mapping is from remote, but key not passed to us.\n");
            return false;
        }

        if (_mappings.count(key) != 0) {
            for (const LdpLabelMapping &m : _mappings[key]) {
                if (mapping == m && m.hidden) { // note: not a real eq check; see mapping op == code.
                    return false;
                }
            }
        }
    }

    for (std::pair<uint64_t, Route*> route : _router->getRoutes()) {
        if (route.second->getType() == RouteType::Mpls) {
            MplsRoute *mpls = (MplsRoute *) route.second;

            if (mpls->in_label == mapping.in_label) {
                return false;
            }
        }

        if (route.second->getType() == RouteType::Ipv4) {
            Ipv4Route *v4 = (Ipv4Route *) route.second;

            if (v4->dst == mapping.fec.prefix || v4->dst_len == mapping.fec.len) {
                return false;
            }
        }
    }

    return true;
}

}