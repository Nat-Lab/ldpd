#include "abstraction/route.hh"

namespace ldpd {

Route::Route() : mpls_stack() {
    oif = -1;
    gw = 0;
    mpls_encap = false;
    metric = 0;
    protocol = RoutingProtocol::Ldp;
}

bool Route::matches(const Route *other) const {
    if (getType() != other->getType()) {
        return false;
    }

    if (protocol != other->protocol) {
        return false;
    }

    if (oif > 0 && oif != other->oif) {
        return false;
    }

    if (mpls_encap && mpls_stack != other->mpls_stack) {
        return false;
    }

    return true;
}

Ipv4Route::Ipv4Route() : Route() {
    dst = 0;
    dst_len = 0;
    mpls_ttl = 255;
}

RouteType Ipv4Route::getType() const {
    return RouteType::Ipv4;
}

uint64_t Ipv4Route::hash() const {
    return ((uint64_t) dst << 32) + ((uint64_t) dst_len << 24); 
}

bool Ipv4Route::matches(const Route *o) const {
    if (!Route::matches(o)) {
        return false;
    }

    const Ipv4Route *other = (Ipv4Route *) o;

    return dst == other->dst && dst_len == other->dst_len;
}

MplsRoute::MplsRoute() : Route() {
    in_label = 0xffffffff;
}

uint64_t MplsRoute::hash() const {
    return in_label;
}

bool MplsRoute::matches(const Route *o) const {
    if (!Route::matches(o)) {
        return false;
    }

    const MplsRoute *other = (MplsRoute *) o;

    return in_label == other->in_label;
}

RouteType MplsRoute::getType() const {
    return RouteType::Mpls;
}

}