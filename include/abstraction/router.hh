#ifndef LDP_ABSTR_ROUTER_H
#define LDP_ABSTR_ROUTER_H
#include "abstraction/interface.hh"
#include "abstraction/route.hh"
#include <vector>
#include <map>

namespace ldpd {

enum RouteChange {
    AddedOrChanged = 0, Removed = 1
};

typedef void (*ldp_routechange_handler_t)(void *data, RouteChange change, const Route *route);

class Router {
public:
    virtual ~Router() {};

    virtual std::vector<Interface> getInterfaces() = 0;
    virtual std::map<uint64_t, Route *> getFib() = 0;
    virtual std::map<uint64_t, Route *> getRoutes() = 0;

    virtual uint64_t addRoute(Route *route) = 0;
    virtual bool deleteRoute(uint64_t routeIndex) = 0;

    virtual void onRouteChange(void *data, ldp_routechange_handler_t handler) = 0;

    virtual void tick() = 0;
};

}

#endif // LDP_ABSTR_ROUTER_H