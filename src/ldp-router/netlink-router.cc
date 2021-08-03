#include "ldp-router/netlink-router.hh"

namespace ldpd {

NetlinkRouter::NetlinkRouter() : _nl(), _rib(), _rib_pending_del(), _fib() {
    _nl.open();
    fullSync();
    _nl.onIpv4RouteChanges(&NetlinkRouter::onRouteChange, this);
    _nl.onMplsRouteChanges(&NetlinkRouter::onRouteChange, this);
}

NetlinkRouter::~NetlinkRouter() {
    for (std::pair<uint64_t, Route *> r : _rib) {
        Route *route = r.second;

        if (route->getType() == RouteType::Mpls) {
            MplsRoute *r = (MplsRoute *) route;
            _nl.addRoute(*r);
        }

        if (route->getType() == RouteType::Ipv4) {
            Ipv4Route *r = (Ipv4Route *) route;
            _nl.addRoute(*r);
        }
    }

    _nl.close();
}

std::vector<Interface> NetlinkRouter::getInterfaces() {
    std::vector<Interface> ifaces = std::vector<Interface>();
    _nl.getInterfaces(ifaces);

    return ifaces;
}

std::map<uint64_t, Route *> NetlinkRouter::getFib() {
    return _fib;
}

std::map<uint64_t, Route *> NetlinkRouter::getRoutes() {
    return _rib;
}

uint64_t NetlinkRouter::addRoute(Route *route) {
    uint64_t key = route->hash();
    deleteRoute(key);
    _rib[key] = route;
    return key;
}

bool NetlinkRouter::deleteRoute(uint64_t routeIndex) {
    if (_rib.count(routeIndex) == 1) {
        _rib_pending_del.push_back(_rib[routeIndex]);
        _rib.erase(routeIndex);
        return true;
    }

    return false;
}

void NetlinkRouter::fullSync() {
    fetchFib();
    pushRib();
}

void NetlinkRouter::fetchFib() {
    std::vector<Ipv4Route> v4 = std::vector<Ipv4Route>();
    std::vector<MplsRoute> m = std::vector<MplsRoute>();

    if (_nl.getRoutes(v4) != 0) {
        log_error("cannot load ipv4 routes from fib.\n");
    }

    if (_nl.getRoutes(m) != 0) {
        log_error("cannot load mpls routes from fib.\n");
    }

    for (Ipv4Route &route : v4) {
        _fib[route.hash()] = new Ipv4Route(route);
    }

    for (MplsRoute &route : m) {
        _fib[route.hash()] = new MplsRoute(route);
    }
}

void NetlinkRouter::pushRib() {
    for (std::vector<Route *>::iterator i = _rib_pending_del.begin(); i != _rib_pending_del.end(); i = _rib_pending_del.erase(i)) {
        Route *route = *i;

        if (route->getType() == RouteType::Mpls) {
            MplsRoute *r = (MplsRoute *) route;
            _nl.deleteRoute(*r);
        }

        if (route->getType() == RouteType::Ipv4) {
            Ipv4Route *r = (Ipv4Route *) route;
            _nl.deleteRoute(*r);
        }

        delete route;
    }

    for (std::pair<uint64_t, Route *> r : _rib) {
        if (_fib.count(r.first) != 0) {
            continue;
        }

        Route *route = r.second;

        if (route->getType() == RouteType::Mpls) {
            MplsRoute *r = (MplsRoute *) route;
            _nl.addRoute(*r);
        }

        if (route->getType() == RouteType::Ipv4) {
            Ipv4Route *r = (Ipv4Route *) route;
            _nl.addRoute(*r);
        }
    }
}

void NetlinkRouter::tick() {
    // todo: rate-limit this?
    pushRib();
}

}