#include "core/ldpd.hh"
#include "ldp-router/netlink-router.hh"
#include "abstraction/router.hh"

#include <arpa/inet.h>
#include <signal.h>

ldpd::Ldpd *d = nullptr;

void handle_sig(__attribute__((unused)) int sig) {
    if (d != nullptr) {
        d->stop();
    }
}

int main() {
    uint32_t rid = inet_addr("10.16.0.3");
    uint32_t ta = inet_addr("172.16.0.3");

    ldpd::NetlinkRouter rtr = ldpd::NetlinkRouter();
    ldpd::Ldpd ldpd = ldpd::Ldpd(rid, 0, (ldpd::Router *) &rtr);

    d = &ldpd;

    signal(SIGINT, handle_sig);

    ldpd.setTransportAddress(ta);
    ldpd.start();
    ldpd.run();

    return 0;
}