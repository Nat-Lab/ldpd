#include "core/label-mapping.hh"

namespace ldpd {

LdpLabelMapping::LdpLabelMapping() : fec() {
    in_label = 0;
    out_label = 0;
    remote = 0;
}

bool LdpLabelMapping::operator==(const LdpLabelMapping &other) const {
    return fec == other.fec && remote == other.remote && (remote ? (out_label == other.out_label) : (in_label == other.in_label));
}

bool LdpLabelMapping::operator<(const LdpLabelMapping &other) const {
    return remote ? (out_label < other.out_label) : (in_label < other.in_label);
}

}