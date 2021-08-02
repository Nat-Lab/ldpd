#include "core/ldpd.hh"
#include "ldp-router/netlink-router.hh"
#include "abstraction/router.hh"
#include <arpa/inet.h>

int main() {
    uint32_t rid = inet_addr("172.16.0.3");

    ldpd::NetlinkRouter rtr = ldpd::NetlinkRouter();
    ldpd::Ldpd ldpd = ldpd::Ldpd(rid, (ldpd::Router *) &rtr);

    ldpd.start();
    ldpd.run();

    return 0;
}