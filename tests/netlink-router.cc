#include "ldp-router/netlink-router.hh"
#include <arpa/inet.h>

uint64_t hash = 0;

ldpd::Route *make_route() {
    /*ldpd::Ipv4Route *ir = new ldpd::Ipv4Route();

    ir->dst = inet_addr("114.51.4.0");
    ir->dst_len = 24;
    ir->gw = inet_addr("172.16.0.254");
    ir->mpls_encap = true;
    ir->mpls_stack = std::vector<uint32_t>();
    ir->mpls_stack.push_back(1919);
    ir->mpls_stack.push_back(810);
    ir->mpls_stack.push_back(853);*/

    ldpd::MplsRoute *mr = new ldpd::MplsRoute();

    mr->gw = inet_addr("10.10.10.10");
    mr->in_label = 114;
    mr->oif = 2;
    mr->mpls_encap = true;
    mr->mpls_stack = std::vector<uint32_t>();
    mr->mpls_stack.push_back(514);
    mr->mpls_stack.push_back(1919);
    mr->mpls_stack.push_back(810);
    mr->mpls_stack.push_back(853);

    return mr;
}

void dump_route(std::map<uint64_t, ldpd::Route *> routes) {
    for (std::pair<uint64_t, ldpd::Route *> pair : routes) {
        ldpd::Route *r = pair.second;

        if (r->getType() == ldpd::RouteType::Mpls) {
            ldpd::MplsRoute *route = (ldpd::MplsRoute *) r;

            printf("%u via %s oif %d", route->in_label, inet_ntoa(*(in_addr *) &route->gw), route->oif);
            if (route->mpls_encap) {
                printf(", out stack: ");
                for (const uint32_t &lbl : route->mpls_stack) {
                    printf("%u/", lbl);
                }
                printf("S");
            }

            printf("\n");
        }

        if (r->getType() == ldpd::RouteType::Ipv4) {
            ldpd::Ipv4Route *route = (ldpd::Ipv4Route *) r;

            printf("%s/%u via ", inet_ntoa(*(in_addr *) &route->dst), route->dst_len);
            printf("%s oif %d", inet_ntoa(*(in_addr *) &route->gw), route->oif);

            if (route->mpls_encap) {
                printf(", mpls: ");
                for (const uint32_t &lbl : route->mpls_stack) {
                    printf("%u/", lbl);
                }
                printf("S, ttl: %u", route->mpls_ttl);
            }
            printf("\n");
        }
    }
}

void dump_info(ldpd::NetlinkRouter &router) {
    printf("fib ==========\n");
    dump_route(router.getFib());

    printf("rib ==========\n");
    dump_route(router.getRoutes());
}

void add_route(ldpd::NetlinkRouter &router) {
    hash = router.addRoute(make_route());
}

void del_route(ldpd::NetlinkRouter &router) {
    if (hash != 0) {
        router.deleteRoute(hash);
        hash = 0;
    }
}

int main() {
    ldpd::NetlinkRouter router = ldpd::NetlinkRouter();

    int count = 0;
    while(true) {
        printf("iter %d ================================================================================\n", count);

        dump_info(router);

        if (count <= 4) {
            if (count % 2 == 0) {
                add_route(router);
            } else {
                del_route(router);
            }
        }

        router.tick();

        dump_info(router);

        count++;

        sleep(1);
    }

    return 0;
}