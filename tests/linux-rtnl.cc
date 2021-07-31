#include "sysdep/linux/netlink.hh"

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

    nl.close();

    return 0;
}