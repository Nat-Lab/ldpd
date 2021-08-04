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
    signal(SIGPIPE, SIG_IGN);

    ldpd.setTransportAddress(ta);
    ldpd.addInterface("ens33");
    ldpd.addInterface("lo");

    ldpd::RoutePolicy ex = ldpd::RoutePolicy(ldpd::FilterAction::Reject);
    ex.addFilter(ldpd::RouteFilter(ldpd::Prefix(inet_addr("8.8.8.8"), 32), ldpd::FilterMatchType::Exact, ldpd::FilterAction::Accept));

    ldpd.setExportPolicy(ex);

    ldpd::RoutePolicy im = ldpd::RoutePolicy(ldpd::FilterAction::Reject);
    im.addFilter(ldpd::RouteFilter(ldpd::Prefix(inet_addr("1.2.4.8"), 32), ldpd::FilterMatchType::Exact, ldpd::FilterAction::Accept));

    ldpd.addRouteSource(ldpd::RoutingProtocol::Static);

    ldpd.setImportPolicy(im);

    ldpd.start();
    ldpd.run();

    return 0;
}