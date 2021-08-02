#include "utils/log.hh"
#include "core/ldpd.hh"
#include "ldp-fsm/ldp-fsm.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

namespace ldpd {

Ldpd::Ldpd(uint32_t routerId, Router *router) : _fsms(), _tx_handlers(), _fds() {
    _running = false;
    _id = routerId;
    _transport = _id;

    _hold = 15;
    _hello = 5;
    _keep = 45;

    _tfd = -1;
    _ufd = -1;

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

    for (LdpFsm *fsm : _fsms) {
        shutdownSession(fsm);
        delete fsm;
    }

    _fsms.clear();

    _running = false;

    return 0;
}

void Ldpd::run() {

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

}