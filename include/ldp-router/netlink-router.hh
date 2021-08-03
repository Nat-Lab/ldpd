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

    const std::vector<const Route *> getFibRoutes();
    const std::map<uint64_t, const Route *> getRibRoutes();

    uint64_t addRibRoute(const Route *route);
    bool deleteRibRoute(uint64_t routeIndex);

    bool syncFib();
    bool disconnectFib();

private:
    void clearCachedFibEntries();

    Netlink _nl;
    std::vector<Route *> _rib;
    std::vector<Route *> _fib;
};

}

#endif // LDP_NETLINK_ROUTER_H