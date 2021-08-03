#include "core/label-mapping.hh"

namespace ldpd {

LdpLabelMapping::LdpLabelMapping() : fec() {
    label = 0;
}

LdpLabelMapping::LdpLabelMapping(uint32_t label, Prefix pfx) : fec(pfx) {
    this->label = label;
}

bool LdpLabelMapping::operator==(const LdpLabelMapping &other) {
    return label == other.label && fec == other.fec;
}

}