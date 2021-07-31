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

    for (const ldpd::Interface &iface : ifaces) {
        printf("interface %d: %s\n", iface.id, iface.ifname.c_str());
    }

    std::vector<ldpd::Route> rts = std::vector<ldpd::Route>();

    if (nl.getRoutes(rts)) {
        return 1;
    }

    for (const ldpd::Route &rt : rts) {
        printf("route src: %s/%u\n", inet_ntoa(*(in_addr *) &rt.src), rt.src_len);
        printf("route dst: %s/%u\n", inet_ntoa(*(in_addr *) &rt.dst), rt.dst_len);
        printf("route gw: %s\n", inet_ntoa(*(in_addr *) &rt.gw));
    }

    nl.close();

    return 0;
}