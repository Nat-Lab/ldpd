#include "utils/log.hh"
#include "core/label-mapping.hh"


namespace ldpd {

LdpLabelMapping::LdpLabelMapping() : fec() {
    in_label = 0;
    out_label = 0;
    remote = false;
    hidden = false;
}

bool LdpLabelMapping::operator==(const LdpLabelMapping &other) const {
    return remote == other.remote && (remote ? (fec == other.fec) : (in_label == other.in_label));
}

bool LdpLabelMapping::operator<(const LdpLabelMapping &other) const {
    uint64_t self_val = (remote ? 0xfffff : 0) + out_label + in_label;
    uint64_t other_val = (other.remote ? 0xfffff : 0) + other.out_label + other.in_label;

    return self_val < other_val;
}

}