#include "utils/log.hh"
#include "abstraction/prefix.hh"
#include <arpa/inet.h>

namespace ldpd {

Prefix::Prefix() {
    prefix = 0;
    len = 0;
}

Prefix::Prefix(uint32_t prefix, uint8_t len) {
    this->prefix = prefix;
    this->len = len;
}

bool Prefix::operator==(const Prefix &other) const {
    return prefix == other.prefix && len == other.len;
}

bool Prefix::includes(const Prefix &other) const {
    uint32_t this_start = ntohl(lowerBound());
    uint32_t this_end = ntohl(upperBound());
    uint32_t other_h = ntohl(other.prefix);

    return this_start <= other_h && other_h <= this_end;
}

bool Prefix::isInSameNetwork(const Prefix &other) const {
    uint32_t this_masked = prefix & htonl((uint32_t) ~((1 << (32 - len)) - 1));
    uint32_t other_masked = other.prefix & htonl((uint32_t) ~((1 << (32 - other.len)) - 1));

    return this_masked == other_masked && len == other.len;
}

uint32_t Prefix::lowerBound() const {
    return prefix & htonl((uint32_t) ~((1 << (32 - len)) - 1));
}

uint32_t Prefix::upperBound() const {
    uint32_t this_mask = htonl((uint32_t) ~((1 << (32 - len)) - 1));
    return (prefix & this_mask) | ~this_mask;
}

uint32_t Prefix::getNetwork() const {
    return lowerBound();
}

uint32_t Prefix::maskWith(uint8_t len) const {
    return prefix & htonl((uint32_t) ~((1 << (32 - len)) - 1));
}

}