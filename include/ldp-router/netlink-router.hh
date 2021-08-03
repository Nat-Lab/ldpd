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

    std::map<uint64_t, Route *> getRoutes();
    std::map<uint64_t, Route *> getFib();

    uint64_t addRoute(Route *route);
    bool deleteRoute(uint64_t routeIndex);

    void tick();

private:

    void fullSync();

    void pushRib();
    void fetchFib();

    template <typename T> void handleFibUpdate(NetlinkChange change, const T &route) {
        uint64_t key = route.hash();

        if (_fib.count(key) != 0) {
            delete _fib[key];
            _fib.erase(key);
        }

        if (change == NetlinkChange::Added) {
            _fib[key] = new T(route);
        }
    }

    template <typename T> static void onRouteChange(void *self, NetlinkChange change, const T &route) {
        NetlinkRouter *router = (NetlinkRouter *) self;
        router->handleFibUpdate(change, route);
    }

    Netlink _nl;
    std::map<uint64_t, Route *> _rib;

    std::vector<Route *> _rib_pending_del;

    std::map<uint64_t, Route *> _fib;
};

}

#endif // LDP_NETLINK_ROUTER_H