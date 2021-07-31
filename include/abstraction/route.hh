#ifndef LDP_ABSTR_ROUTE_H
#define LDP_ABSTR_ROUTE_H
#include <stdint.h>
#include <vector>

namespace ldpd {

enum RouteType {
    Generic, Connected
};

struct Route {
    // in/out iface id.
    int iif, oif;

    // gw, in network byte order
    uint32_t gw;

    // true if mpls encap on (if false for MplsRoute type - means everything poped)
    bool mpls_encap;

    // mpls labels stack, if mpls_encap. labels are in host byte order
    std::vector<uint32_t> mpls_stack;

};

struct Ipv4Route : public Route {
    // src & dst, in network byte order
    uint32_t src, dst;

    // src & dst length (/24, /32, etc.)
    uint8_t src_len, dst_len;

    // mpls ttl, if mpls_encap.
    uint8_t mpls_ttl;
};

struct MplsRoute : public Route {
    // in label
    uint32_t in_label;
};

} 

#endif // LDP_ABSTR_ROUTE_H