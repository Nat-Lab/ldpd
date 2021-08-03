#include "core/label-mapping.hh"

namespace ldpd {

LdpLabelMapping::LdpLabelMapping() : fec() {
    label = 0;
}

bool LdpLabelMapping::operator==(const LdpLabelMapping &other) {
    return label == other.label && fec == other.fec;
}

}