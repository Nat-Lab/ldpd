#include "sysdep/linux/netlink.hh"
#include <arpa/inet.h>

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
        printf("interface %d: %s\n", iface.id, iface.ifname.c_str());
    }

    std::vector<ldpd::Ipv4Route> rts = std::vector<ldpd::Ipv4Route>();

    if (nl.getIpv4Routes(rts)) {
        return 1;
    }

    printf("\nbegin ip routes ====\n");

    for (const ldpd::Ipv4Route &rt : rts) {
        printf("route dst: %s/%u\n", inet_ntoa(*(in_addr *) &rt.dst), rt.dst_len);
        printf("route gw: %s\n", inet_ntoa(*(in_addr *) &rt.gw));
        printf("route oif: %d\n", rt.oif);

        if (rt.mpls_encap) {
            for (const uint32_t &lbl : rt.mpls_stack) {
                printf("mpls stack: %u\n", lbl);
            }

            printf("mpls ttl: %u\n", rt.mpls_ttl);
        }

        printf("\n");
    }

    std::vector<ldpd::MplsRoute> mrts = std::vector<ldpd::MplsRoute>();

    if (nl.getMplsRoutes(mrts)) {
        return 1;
    }

    printf("begin mpls routes ====\n");

    for (const ldpd::MplsRoute &rt : mrts) {
        printf("mpls in label: %u\n", rt.in_label);
        printf("gw: %s\n", inet_ntoa(*(in_addr *) &rt.gw));
        printf("oif: %d\n", rt.oif);

        if (rt.mpls_encap) {
            for (const uint32_t &lbl : rt.mpls_stack) {
                printf("out mpls stack: %u\n", lbl);
            }
        }

        printf("\n");
    }

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

    if (nl.addMplsRoute(mr) == 0) {
        printf("add-mpls-route ok.\n");
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
    ir.mpls_ttl = 255;

    if (nl.addIpv4Route(ir) == 0) {
        printf("add-ipv4-route ok.\n");
    }

    nl.close();

    return 0;
}