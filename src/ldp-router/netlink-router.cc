#include "ldp-router/netlink-router.hh"

namespace ldpd {

NetlinkRouter::NetlinkRouter() : _nl() {
    _nl.open();
}

NetlinkRouter::~NetlinkRouter() {
    _nl.close();
}

std::vector<Interface> NetlinkRouter::getInterfaces() {
    std::vector<Interface> ifaces = std::vector<Interface>();
    _nl.getInterfaces(ifaces);

    return ifaces;
}

const std::vector<const Route *> NetlinkRouter::getFibRoutes() {
    return std::vector<const Route *>();
}

const std::map<uint64_t, const Route *> NetlinkRouter::getRibRoutes() {
    return std::map<uint64_t, const Route *>();
}

uint64_t NetlinkRouter::addRibRoute(const Route *route) {
    return 0;
}

bool NetlinkRouter::deleteRibRoute(uint64_t routeIndex) {
    return false;
}

bool NetlinkRouter::syncFib() {
    return false;
}

bool NetlinkRouter::disconnectFib() {
    return false;
}

}