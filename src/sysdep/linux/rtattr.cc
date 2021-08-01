#include "utils/log.hh"
#include "sysdep/linux/rtattr.hh"

namespace ldpd {

RtAttr::RtAttr() : _attrs() {

}

RtAttr::~RtAttr() {
    for (std::pair<const unsigned short, struct rtattr*> attr : _attrs) {
        free(attr.second);
    }
}

const struct rtattr* RtAttr::getAttribute(unsigned short type) const {
    if (_attrs.count(type) == 0) {
        return nullptr;
    }

    return _attrs.at(type);
}

bool RtAttr::hasAttribute(unsigned short type) const {
    return _attrs.count(type);
}

void RtAttr::addNestedAttribute(unsigned short type, const RtAttr &payload) {
    size_t len = payload.length();

    uint8_t *buf = (uint8_t *) malloc(len);

    payload.write(buf, len);

    addRawAttribute(type, buf, len);

    free(buf);
}

void RtAttr::addRawAttribute(unsigned short type, const uint8_t *payload, size_t payload_len) {
    size_t len = RTA_LENGTH(payload_len);
    uint8_t *attr_buf = (uint8_t *) malloc(len);

    uint8_t *ptr = attr_buf;

    struct rtattr *attr = (struct rtattr *) ptr;
    ptr += sizeof(struct rtattr);

    attr->rta_len = len;
    attr->rta_type = type;

    memcpy(ptr, payload, payload_len);
}

ssize_t RtAttr::parse(const uint8_t *from, size_t len) {
    const uint8_t *ptr = from;

    struct rtattr *attr = (struct rtattr *) ptr;

    for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
        struct rtattr *local_copy = (struct rtattr *) malloc(attr->rta_len);
        memcpy(local_copy, attr, attr->rta_len);
        _attrs[attr->rta_type] = local_copy;
    }

    return ptr - from;
}

ssize_t RtAttr::write(uint8_t *to, size_t buf_sz) const {
    uint8_t *ptr = to;

    if (buf_sz < length()) {
        log_error("cannot write: buffer too small\n");
        return -1;
    }

    for (std::pair<const unsigned short, const struct rtattr*> attr : _attrs) {
        memcpy(ptr, attr.second, attr.second->rta_len);
        ptr += attr.second->rta_len;
    }

    return ptr - to;
}

size_t RtAttr::length() const {
    size_t len = 0;
    for (std::pair<const unsigned short, const struct rtattr*> attr : _attrs) {
        len += attr.second->rta_len;
    }

    return len;
}

}