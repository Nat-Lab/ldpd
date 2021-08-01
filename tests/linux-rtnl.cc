#include "sysdep/linux/netlink.hh"
#include <arpa/inet.h>

int dump_v4_route(ldpd::Netlink &nl) {
    std::vector<ldpd::Ipv4Route> rts = std::vector<ldpd::Ipv4Route>();

    if (nl.getRoutes(rts)) {
        return 1;
    }

    for (const ldpd::Ipv4Route &rt : rts) {
        printf("%s/%u via ", inet_ntoa(*(in_addr *) &rt.dst), rt.dst_len);
        printf("%s oif %d", inet_ntoa(*(in_addr *) &rt.gw), rt.oif);

        if (rt.mpls_encap) {
            printf(", mpls: ");
            for (const uint32_t &lbl : rt.mpls_stack) {
                printf("%u/", lbl);
            }
            printf("S, ttl: %u", rt.mpls_ttl);
        }
        printf("\n");
    }

    return 0;
}

int dump_mpls_route(ldpd::Netlink &nl) {
    std::vector<ldpd::MplsRoute> mrts = std::vector<ldpd::MplsRoute>();

    if (nl.getRoutes(mrts)) {
        return 1;
    }

    for (const ldpd::MplsRoute &rt : mrts) {
        printf("%u via %s oif %d", rt.in_label, inet_ntoa(*(in_addr *) &rt.gw), rt.oif);
        if (rt.mpls_encap) {
            printf(", out stack: ");
            for (const uint32_t &lbl : rt.mpls_stack) {
                printf("%u/", lbl);
            }
            printf("S");
        }

        printf("\n");
    }

    return 0;
}

int main() {
    ldpd::Netlink nl = ldpd::Netlink();

    if (nl.open() != 0) {
        return 1;
    }

    std::vector<ldpd::Interface> ifaces = std::vector<ldpd::Interface>();

    if (nl.getInterfaces(ifaces)) {
        return 1;
    }

    printf("begin ifaces ====\n");

    for (const ldpd::Interface &iface : ifaces) {
        printf("%d: %s ", iface.index, iface.ifname.c_str());
        for (const ldpd::InterfaceAddress &addr : iface.addresses) {
            printf("%s/%d ", inet_ntoa(*(in_addr *) &(addr.address)), addr.len);
        }
        printf("\n");
    }

    printf("begin ip routes ====\n");
    dump_v4_route(nl);

    printf("begin mpls routes ====\n");
    dump_mpls_route(nl);

    printf("addroute test ====\n");

    ldpd::MplsRoute mr = ldpd::MplsRoute();

    mr.gw = inet_addr("10.10.10.10");
    mr.in_label = 114;
    mr.oif = 2;
    mr.mpls_encap = true;
    mr.mpls_stack = std::vector<uint32_t>();
    mr.mpls_stack.push_back(514);
    mr.mpls_stack.push_back(1919);
    mr.mpls_stack.push_back(810);
    mr.mpls_stack.push_back(853);

    if (nl.addRoute(mr) == 0) {
        printf("add-mpls-route ok, rtable now: \n");
        dump_mpls_route(nl);
    }

    ldpd::Ipv4Route ir = ldpd::Ipv4Route();

    ir.dst = inet_addr("114.51.4.0");
    ir.dst_len = 24;
    ir.gw = inet_addr("172.16.0.254");
    ir.mpls_encap = true;
    ir.mpls_stack = std::vector<uint32_t>();
    ir.mpls_stack.push_back(1919);
    ir.mpls_stack.push_back(810);
    ir.mpls_stack.push_back(853);

    if (nl.addRoute(ir) == 0) {
        printf("add-ipv4-route ok, rtablw now:\n");
        dump_v4_route(nl);
    }

    printf("delroute test ====\n");

    if (nl.deleteRoute(mr) == 0) {
        printf("del-mpls-route ok, rtable now: \n");
        dump_mpls_route(nl);
    }

    if (nl.deleteRoute(ir) == 0) {
        printf("del-ipv4-route ok, rtable now: \n");
        dump_v4_route(nl);
    }

    nl.close();

    return 0;
}