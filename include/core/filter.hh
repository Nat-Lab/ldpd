#ifndef LDP_FILTER_H
#define LDP_FILTER_H
#include "abstraction/prefix.hh"
#include <stdint.h>
#include <vector>

namespace ldpd {

enum FilterMatchType {
    Exact, UpTo
};

enum FilterAction {
    Nop, Accept, Reject
};

struct RouteFilter {
    RouteFilter(const Prefix &sourcePrefix, FilterMatchType type, FilterAction action, uint8_t upto = 32);
    FilterAction apply(const Prefix &target) const;

    FilterMatchType type;
    FilterAction action;

    Prefix source;
    uint8_t upto;
};

struct RoutePolicy {
    RoutePolicy(FilterAction defaultAction);
    FilterAction apply(const Prefix &target) const;

    void addFilter(const RouteFilter &filter);

    std::vector<RouteFilter> terms;
    FilterAction defaultAction;
};

}

#endif // LDP_FILTER_H