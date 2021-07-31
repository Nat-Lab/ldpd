#ifndef LDP_ABSTR_INTERFACE_H
#define LDP_ABSTR_INTERFACE_H
#include <string>

namespace ldpd {

struct Interface {
    int id;

    std::string ifname;
};

} 

#endif // LDP_ABSTR_INTERFACE_H