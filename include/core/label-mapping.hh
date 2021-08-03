#ifndef LDP_LBL_MAPPING_H
#define LDP_LBL_MAPPING_H
#include "abstraction/prefix.hh"
#include <stdint.h>
#include <vector>

namespace ldpd {

struct LdpLabelMapping {
    LdpLabelMapping() : fec() {};

    uint32_t label;
    std::vector<Prefix> fec;
};

}

#endif // LDP_LBL_MAPPING_H