#ifndef LDP_ABSTR_PREFIX_H
#define LDP_ABSTR_PREFIX_H
#include <stdint.h>

namespace ldpd {

struct Prefix {
    uint32_t prefix;
    uint8_t len;
};

} 

#endif // LDP_ABSTR_PREFIX_H