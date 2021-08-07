#ifndef LDP_INET_NTOP_H
#define LDP_INET_NTOP_H
#include <stdint.h>
#include <arpa/inet.h>

namespace ldpd {

class InetNtop {
public:
    InetNtop(uint32_t address);

    char str[INET_ADDRSTRLEN];
};

}

#endif // LDP_INET_NTOP_H