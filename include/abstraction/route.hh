#ifndef LDP_ABSTR_ROUTE_H
#define LDP_ABSTR_ROUTE_H
#include <stdint.h>
#include <vector>

namespace ldpd {

struct Route {
    // src & dst, in network byte order
    uint32_t src, dst;

    // src & dst length (/24, /32, etc.)
    uint8_t src_len, dst_len;

    // gw, in network byte order
    uint32_t gw;

    // true if mpls encap on
    bool mpls_encap;

    // mpls labels stack, labels are in host byte order
    std::vector<uint32_t> mpls_stack;
};

} 

#endif // LDP_ABSTR_ROUTE_H