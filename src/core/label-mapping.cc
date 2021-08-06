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
    if (remote != other.remote) {
        log_warn("comparing between remote and local binding is invalid.\n");
        return false;
    }

    return fec == other.fec && (remote ? (out_label == other.out_label) : (in_label == other.in_label));
}

bool LdpLabelMapping::operator<(const LdpLabelMapping &other) const {
    return remote ? (out_label < other.out_label) : (in_label < other.in_label);
}

}