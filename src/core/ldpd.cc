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

namespace ldpd {

Ldpd::Ldpd(uint32_t routerId, Router *router) : _fsms(), _tx_handlers(), _fds(), _hellos(), _transports() {
    _running = false;
    _id = routerId;
    _transport = _id;

    _hold = 15;
    _hello = 5;
    _keep = 45;

    _tfd = -1;
    _ufd = -1;

    _last_hello = 0;
    _now = time(NULL);

    _router = router;
}

Ldpd::~Ldpd() {
    stop();
}

int Ldpd::start() {
    if (_running) {
        log_warn("already running.\n");
        return 1;
    }

    bool transport_local = false;
    std::vector<Interface> ifaces = _router->getInterfaces();

    for (const Interface &iface : ifaces) {
        for (const InterfaceAddress &addr : iface.addresses) {
            if (addr.address == _transport) {
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
        log_fatal("socket(): %s\n", strerror(errno));
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
        log_fatal("listen(): %s\n", strerror(errno));
        return 1;
    }

    _ufd = socket(AF_INET, SOCK_DGRAM, 0);

    if (_ufd < 0) {
        log_fatal("socket(): %s\n", strerror(errno));
        return 1;
    }

    if (bind(_ufd, (const sockaddr *) &addr, sizeof(addr)) < 0) {
        log_fatal("bind(): %s.\n", strerror(errno));
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
    close(_ufd);
    close(_tfd);

    for (std::pair<uint64_t, LdpFsm *> fsm : _fsms) {
        shutdownSession(fsm.second);
        delete fsm.second;
    }

    _fsms.clear();

    _running = false;

    return 0;
}

void Ldpd::tick() {
    _now = time(nullptr);

    // todo: other stuff, send hello, keepalive, etc.
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
            log_error("select(): %s\n", strerror(errno));
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

        for (std::pair<int, LdpFsm *> _fd : _fds) {
            if (FD_ISSET(_fd.first, &fds)) {
                handleSession(_fd.first, _fd.second);
            }
        }
    }
}

uint32_t Ldpd::getRouterId() const {
    return _id;
}

uint32_t Ldpd::getTransportAddress() const {
    return _transport;
}

uint16_t Ldpd::getKeepaliveTimer() const {
    return _keep;
}

void Ldpd::setTransportAddress(uint32_t address) {
    _transport = address;
}

void Ldpd::setKeepaliveTimer(uint16_t timer) {
    _keep = timer;
}

tx_handler_t Ldpd::getTransmitHandler(LdpFsm* requester) {
    return nullptr;
}

ssize_t Ldpd::handleMessage(LdpFsm* from, const LdpMessage *msg) {
    return -1;
}

void Ldpd::shutdownSession(LdpFsm* of) {

}

void Ldpd::removeSession(LdpFsm* of) {

}

void Ldpd::handleHello() {
    // TODO: targeted hellos?

    sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));

    socklen_t addrlen = sizeof(remote);

    uint8_t buffer[8192];
    ssize_t len = recvfrom(_ufd, (void *) buffer, sizeof(buffer), 0, (struct sockaddr *) &remote, &addrlen);

    if (len < 0) {
        log_warn("recvfrom(): %s\n", strerror(errno));
        return;
    }

    if (len == 0) {
        log_warn("recvfrom(): got eof\n");
        return;
    }

    LdpPdu pdu = LdpPdu();

    ssize_t res = pdu.parse(buffer, len);

    if (res < 0) {
        log_info("invalid pdu from %s:%u (cannot understand)\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        return;
    }

    const LdpMessage *hello = pdu.getMessage(LDP_MSGTYPE_HELLO);

    if (hello == nullptr) {
        log_info("invalid pdu from %s:%u (no hello body)\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        return;
    }

    uint32_t nei_id = pdu.getRouterId();
    uint16_t nei_ls = pdu.getLabelSpace();

    uint64_t key = LDP_KEY(nei_id, nei_ls);

    if (!_hellos.count(key) || _now - _hellos[key] > _hold) {
        log_info("got a new hello from %s:%u.\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        log_info("their id: %s:%u.\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_ls);
    }

    _hellos[key] = time(NULL);

    const LdpRawTlv *ta_tlv = hello->getTlv(LDP_TLVTYPE_IPV4_TRANSPORT);

    if (ta_tlv == nullptr) {
        return;
    }

    const LdpIpv4TransportAddressTlvValue *ta_tlv_val = (const LdpIpv4TransportAddressTlvValue *) ta_tlv->getParsedValue();

    if (ta_tlv_val == nullptr) {
        log_warn("%s:%u included a transport-address tlv in hello, but we don't understand it.\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_ls);
        return;
    }

    uint32_t ta = ta_tlv_val->getAddress();

    if (_transports.count(key) == 0 || _transports[key] != ta) {
        log_info("learned transport address for %s:%u.\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_ls);
        log_info("transport address: %s.\n", inet_ntoa(*(struct in_addr *) &ta));
        _transports[key] = ta;
    }
}

void Ldpd::handleSession() {
    sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));

    socklen_t addrlen = sizeof(remote);

    int fd = accept(_tfd, (struct sockaddr *) &remote, &addrlen);

    if (fd < 0) {
        log_warn("accept(): %s\n", strerror(errno));
        return;
    }

    if (remote.sin_family != AF_INET) {
        log_warn("accept(): got non-ipv4?\n");
        return;
    }

    log_info("new tcp connection from %s:%u\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));

    uint8_t buffer[8192];
    ssize_t len = read(fd, buffer, sizeof(buffer));

    // FIXME: make sure it has atleast one full pdu?

    if (len < 0) {
        log_warn("read(): %s\n", strerror(errno));
        return;
    }

    if (len == 0) {
        log_warn("read(): got eof\n");
        return;
    }

    LdpFsm *session = new LdpFsm(this);
    
    int offset = 0;

    while (len > 0) {
        ssize_t ret = session->receive(buffer + offset, len);

        if (ret < 0 || session->getState() < LdpSessionState::OpenReceived) {
            delete session;
            return;
        }

        offset += ret;
        len -= ret;
    }

    uint32_t nei_id = session->getNeighborId();
    uint16_t nei_space = session->getNeighborLabelSpace();

    log_info("neigh lsr-id: %s:%u\n", inet_ntoa(*(struct in_addr *) &nei_id), nei_space);

    uint32_t key = LDP_KEY(nei_id, nei_space);

    if (_hellos.count(key) == 0 || _now - _hellos[key] > _hold) {
        log_warn("no hello from them or hold expired. rejecting.\n");

        // TODO: send notify.

        close(fd);
        return;
    }
    
    if (_fds.count(fd) != 0) {
        log_warn("fd %d is used by an active neighbour? assume that one is dead.\n", fd);
        removeSession(_fds[fd]);
    }

    if (_fsms.count(key) != 0) {
        log_warn("a session with a router w/ same lrs-id exists? rejecting.\n");
        close(fd);
        return;
    }

    log_info("looks good, registering them.\n");

    _fsms[key] = session;
    _fds[fd] = session;
}

void Ldpd::handleSession(int fd, LdpFsm *session) {
    uint8_t buffer[8192];
    ssize_t len = read(fd, buffer, sizeof(buffer));

    // FIXME: make sure it has atleast one full pdu?

    if (len < 0) {
        log_warn("read(): %s\n", strerror(errno));
        return;
    }

    if (len == 0) {
        log_warn("read(): got eof\n");
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

}