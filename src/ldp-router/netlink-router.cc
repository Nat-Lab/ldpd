#include "ldp-router/netlink-router.hh"
#include <arpa/inet.h>

namespace ldpd {

NetlinkRouter::NetlinkRouter() : _nl(), _rib(), _rib_pending_del(), _fib() {
    log_debug("opening netlink services...\n");
    
    if (_nl.open() < 0) {
        return;
    }

    fullSync();
    
    _nl.onIpv4RouteChanges(&NetlinkRouter::onRouteChange, this);
    _nl.onMplsRouteChanges(&NetlinkRouter::onRouteChange, this);

    _onroutechange = nullptr;
}

NetlinkRouter::~NetlinkRouter() {
    for (std::pair<uint64_t, Route *> r : _rib) {
        Route *route = r.second;

        if (route->getType() == RouteType::Mpls) {
            MplsRoute *r = (MplsRoute *) route;
            log_debug("(mpls) deleting route to %u from fib...\n", r->in_label);
            _nl.deleteRoute(*r);
        }

        if (route->getType() == RouteType::Ipv4) {
            Ipv4Route *r = (Ipv4Route *) route;
            log_debug("(ipv4) deleting route to %s/%u from fib...\n", inet_ntoa(*(struct in_addr *) &(r->dst)), r->dst_len);
            _nl.deleteRoute(*r);
        }

        delete route;
    }

    for (std::pair<uint64_t, Route *> r : _fib) {
        delete r.second;
    }

    _nl.close();
}

std::vector<Interface> NetlinkRouter::getInterfaces() {
    std::vector<Interface> ifaces = std::vector<Interface>();
    _nl.getInterfaces(ifaces);

    return ifaces;
}

std::vector<const Route *> NetlinkRouter::getFib() {
    std::vector<const Route *> rslt = std::vector<const Route *>();

    for (std::pair<uint64_t, Route *> r : _fib) {
        rslt.push_back(r.second);
    }

    return rslt;
}

std::vector<const Route *>  NetlinkRouter::getRoutes() {
    std::vector<const Route *> rslt = std::vector<const Route *>();

    for (std::pair<uint64_t, Route *> r : _rib) {
        rslt.push_back(r.second);
    }

    return rslt;
}

uint64_t NetlinkRouter::addRoute(Route *route) {
    uint64_t key = route->hash();
    deleteRoute(route);
    _rib.insert(std::make_pair(key, route));
    return key;
}

bool NetlinkRouter::deleteRoute(const Route *selector) {
    uint64_t key = selector->hash();

    // though I opted for no-auto, this is wayyy to longggg.
    auto range = _rib.equal_range(key);

    for (auto i = range.first; i != range.second; ++i) {
        if (selector->matches(i->second)) {
            _rib_pending_del.push_back(i->second);
            _rib.erase(i);
            return true;
        }
    }

    return false;
}

void NetlinkRouter::fullSync() {
    fetchFib();
    pushRib();
}

void NetlinkRouter::fetchFib() {
    log_debug("performing full fib sync...\n");

    std::vector<Ipv4Route> v4 = std::vector<Ipv4Route>();
    std::vector<MplsRoute> m = std::vector<MplsRoute>();

    if (_nl.getRoutes(v4) != 0) {
        log_error("cannot load ipv4 routes from fib.\n");
    }

    if (_nl.getRoutes(m) != 0) {
        log_error("cannot load mpls routes from fib.\n");
    }

    for (Ipv4Route &route : v4) {
        _fib.insert(std::make_pair(route.hash(), new Ipv4Route(route)));
    }

    for (MplsRoute &route : m) {
        _fib.insert(std::make_pair(route.hash(), new MplsRoute(route)));
    }
}

void NetlinkRouter::pushRib() {
    for (std::vector<Route *>::iterator i = _rib_pending_del.begin(); i != _rib_pending_del.end();) {
        Route *route = *i;

        if (route->getType() == RouteType::Mpls) {
            MplsRoute *r = (MplsRoute *) route;
            log_debug("(mpls) deleting route to %u from fib...\n", r->in_label);
            if (_nl.deleteRoute(*r) == 0) {
                handleFibUpdate(NetlinkChange::Deleted, *r);
                delete route;
                i = _rib_pending_del.erase(i);
                continue;
            }
        }

        if (route->getType() == RouteType::Ipv4) {
            Ipv4Route *r = (Ipv4Route *) route;
            log_debug("(ipv4) deleting route to %s/%u from fib...\n", inet_ntoa(*(struct in_addr *) &(r->dst)), r->dst_len);
            if (_nl.deleteRoute(*r) == 0) {
                handleFibUpdate(NetlinkChange::Deleted, *r);
                delete route;
                i = _rib_pending_del.erase(i);
                continue;
            }
        }

        log_error("failed to delete route - will retry.\n");
        ++i;
    }

    for (std::pair<uint64_t, Route *> r : _rib) {
        if (_fib.count(r.first) != 0) {
            continue;
        }

        Route *route = r.second;

        if (route->getType() == RouteType::Mpls) {
            MplsRoute *r = (MplsRoute *) route;
            log_debug("(mpls) adding route to %u to fib...\n", r->in_label);
            if (_nl.addRoute(*r, true) == 0) {
                handleFibUpdate(NetlinkChange::Added, *r);
                continue;
            }
        }

        if (route->getType() == RouteType::Ipv4) {
            Ipv4Route *r = (Ipv4Route *) route;
            log_debug("(ipv4) adding route to %s/%u to fib...\n", inet_ntoa(*(struct in_addr *) &(r->dst)), r->dst_len);
            if (_nl.addRoute(*r, true) == 0) {
                handleFibUpdate(NetlinkChange::Added, *r);
                continue;
            }
        }

        log_error("failed to add route - will retry.\n");
    }
}

void NetlinkRouter::onRouteChange(void* data, ldp_routechange_handler_t handler) {
    _onroutechange = handler;
    _routechange_data = data;
}

void NetlinkRouter::tick() {
    _nl.tick();

    // todo: rate-limit this?
    pushRib();
}

}