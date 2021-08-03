#ifndef LDP_LBL_MAPPING_H
#define LDP_LBL_MAPPING_H
#include "abstraction/prefix.hh"
#include <stdint.h>
#include <vector>

namespace ldpd {

struct LdpLabelMapping {
    LdpLabelMapping();
    LdpLabelMapping(uint32_t label, Prefix pfx);

    bool operator==(const LdpLabelMapping &other);

    uint32_t label;
    Prefix fec;
};

}

#endif // LDP_LBL_MAPPING_H