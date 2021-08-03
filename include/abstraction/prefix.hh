#ifndef LDP_ABSTR_PREFIX_H
#define LDP_ABSTR_PREFIX_H
#include <stdint.h>

namespace ldpd {

struct Prefix {
    Prefix();

    bool operator==(const Prefix &other) const;
    bool isInSameNetwork(const Prefix &other) const;
    bool includes(const Prefix &other) const;

    uint32_t prefix;
    uint8_t len;
};

} 

#endif // LDP_ABSTR_PREFIX_H