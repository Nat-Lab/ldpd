#ifndef LDP_ABSTR_INTERFACE_H
#define LDP_ABSTR_INTERFACE_H
#include "abstraction/prefix.hh"
#include <string>
#include <vector>

namespace ldpd {

struct Interface {
    int id;

    std::string ifname;

    std::vector<Prefix> addresses;
};

} 

#endif // LDP_ABSTR_INTERFACE_H