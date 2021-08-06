#ifndef LDP_LBL_MAPPING_H
#define LDP_LBL_MAPPING_H
#include "abstraction/prefix.hh"
#include <stdint.h>
#include <vector>

namespace ldpd {

struct LdpLabelMapping {
    LdpLabelMapping();

    bool operator==(const LdpLabelMapping &other) const;
    bool operator<(const LdpLabelMapping &other) const;

    bool remote;

    uint32_t in_label;
    uint32_t out_label;
    Prefix fec;
};

}

#endif // LDP_LBL_MAPPING_H