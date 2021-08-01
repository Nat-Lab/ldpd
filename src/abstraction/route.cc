#include "abstraction/route.hh"

namespace ldpd {

Route::Route() : mpls_stack() {
    oif = 0;
    gw = 0;
    mpls_encap = false;
}

Ipv4Route::Ipv4Route() : Route() {
    dst = 0;
    dst_len = 0;
    mpls_ttl = 255;
}

RouteType Ipv4Route::getType() const {
    return RouteType::Ipv4;
}

MplsRoute::MplsRoute() : Route() {
    in_label = 0;
}

RouteType MplsRoute::getType() const {
    return RouteType::Mpls;
}

}