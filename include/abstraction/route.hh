#ifndef LDP_ABSTR_ROUTE_H
#define LDP_ABSTR_ROUTE_H
#include <stdint.h>
#include <vector>

namespace ldpd {

enum RouteType {
    Ipv4, Mpls
};

enum RoutingProtocol {
    Undefined = 0, Static = 3, Zebra = 11, Bird = 12, Dhcp = 16, Babel = 42,
    Bgp = 186, Isis = 187, Ospf = 188, Rip = 189, Eigrp = 192, Ldp = 193
};

struct Route {
    Route();
    virtual ~Route() {};

    // protocol
    RoutingProtocol protocol;

    // out iface id.
    int oif;

    // gw, in network byte order
    uint32_t gw;

    // metric
    int metric;

    // true if mpls encap on (if false for MplsRoute type - means everything poped)
    bool mpls_encap;

    // mpls labels stack, if mpls_encap. labels are in host byte order
    std::vector<uint32_t> mpls_stack;

    virtual RouteType getType() const = 0;
    virtual uint64_t hash() const = 0;
    virtual bool matches(const Route *other) const; 

};

struct Ipv4Route : public Route {
    Ipv4Route();

    // dst net, in network byte order
    uint32_t dst;

    // dst length (/24, /32, etc.)
    uint8_t dst_len;

    // mpls ttl, if mpls_encap.
    uint8_t mpls_ttl;

    virtual RouteType getType() const;
    virtual uint64_t hash() const;
    virtual bool matches(const Route *other) const;
};

struct MplsRoute : public Route {
    MplsRoute();

    // in label
    uint32_t in_label;

    virtual RouteType getType() const;
    virtual uint64_t hash() const;
    virtual bool matches(const Route *other) const;
};

} 

#endif // LDP_ABSTR_ROUTE_H