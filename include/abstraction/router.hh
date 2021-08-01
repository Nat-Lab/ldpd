#ifndef LDP_ABSTR_ROUTER_H
#define LDP_ABSTR_ROUTER_H
#include "abstraction/interface.hh"
#include "abstraction/route.hh"
#include <vector>
#include <map>

namespace ldpd {

class Router {
public:
    virtual ~Router() {};

    virtual std::vector<Interface> getInterfaces() = 0;

    virtual const std::vector<const Route *> getFibRoutes() = 0;
    virtual const std::map<uint64_t, const Route *> getRibRoutes() = 0;

    virtual uint64_t addRibRoute(const Route *route) = 0;
    virtual bool deleteRibRoute(uint64_t routeIndex) = 0;

    virtual bool syncFib() = 0;
    virtual bool disconnectFib() = 0;
};

}

#endif // LDP_ABSTR_ROUTER_H