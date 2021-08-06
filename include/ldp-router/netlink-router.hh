#ifndef LDP_NETLINK_ROUTER_H
#define LDP_NETLINK_ROUTER_H
#include "sysdep/linux/netlink.hh"
#include "abstraction/router.hh"

namespace ldpd {

class NetlinkRouter : Router {
public:
    NetlinkRouter();
    ~NetlinkRouter();

    std::vector<Interface> getInterfaces();

    std::vector<const Route *> getRoutes();
    std::vector<const Route *> getFib();

    uint64_t addRoute(Route *route);
    bool deleteRoute(const Route *selector);

    void onRouteChange(void* data, ldp_routechange_handler_t handler);

    void tick();

private:

    void fullSync();

    void pushRib();
    void fetchFib();

    template <typename T> void handleFibUpdate(NetlinkChange change, const T &route) {
        uint64_t key = route.hash();

        // though I opted for no-auto, this is wayyy to longggg.
        auto range = _fib.equal_range(key);

        for (auto i = range.first; i != range.second; ++i) {
            if (route.matches(i->second)) {
                delete i->second;
                _fib.erase(i);
                return;
            }
        }

        if (change == NetlinkChange::Added) {
            _fib.insert(std::make_pair(key, new T(route)));
        }
    }

    template <typename T> static void onRouteChange(void *self, NetlinkChange change, const T &route) {
        NetlinkRouter *router = (NetlinkRouter *) self;
        router->handleFibUpdate(change, route);
        if (router->_onroutechange != nullptr) {
            router->_onroutechange(router->_routechange_data, (RouteChange) change, &route);
        }
    }

    Netlink _nl;
    std::multimap<uint64_t, Route *> _rib;

    std::vector<Route *> _rib_pending_del;

    std::multimap<uint64_t, Route *> _fib;

    ldp_routechange_handler_t _onroutechange;
    void *_routechange_data;
};

}

#endif // LDP_NETLINK_ROUTER_H