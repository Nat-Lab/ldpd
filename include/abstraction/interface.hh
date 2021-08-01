#ifndef LDP_ABSTR_INTERFACE_H
#define LDP_ABSTR_INTERFACE_H
#include "abstraction/prefix.hh"
#include <string>
#include <vector>

namespace ldpd {

struct InterfaceAddress {
    int ifindex;

    uint32_t address;
    uint8_t len;
};

struct Interface {
    int index;

    std::string ifname;

    std::vector<InterfaceAddress> addresses;
};


} 

#endif // LDP_ABSTR_INTERFACE_H