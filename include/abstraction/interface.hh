#ifndef LDP_ABSTR_INTERFACE_H
#define LDP_ABSTR_INTERFACE_H
#include "abstraction/prefix.hh"
#include <string>
#include <vector>

namespace ldpd {

struct InterfaceAddress {
    InterfaceAddress() : address() {};
    int ifindex;

    Prefix address;
};

struct Interface {
    Interface() : ifname("unset"), addresses() {
        index = 0;
        up = false;
        loopback = false;
        ptp = false;
        running = false;
        noarp = false;
    }

    int index;

    bool up;
    bool loopback;
    bool ptp;
    bool running;
    bool noarp;

    std::string ifname;

    std::vector<InterfaceAddress> addresses;
};


} 

#endif // LDP_ABSTR_INTERFACE_H