#ifndef LDP_RTATTR_H
#define LDP_RTATTR_H
#include "utils/log.hh"
#include "core/serializable.hh"
#include <string.h>
#include <linux/rtnetlink.h>
#include <map>

namespace ldpd {

class RtAttr : Serializable {
public:
    RtAttr();
    ~RtAttr();

    const struct rtattr* getAttribute(unsigned short type) const;
    bool hasAttribute(unsigned short type) const;

    template <typename T> bool getAttributeValue(unsigned short type, T &val) const {
        if (!hasAttribute(type)) {
            return false;
        }
        val = *(T *) RTA_DATA(getAttribute(type));
        return true;
    }

    template <typename T> bool getAttributePointer(unsigned short type, T* &val) const {
        if (!hasAttribute(type)) {
            return false;
        }
        val = (T *) RTA_DATA(getAttribute(type));
        return true;
    }

    template <typename T> void addAttribute(unsigned short type, const T &value) {
        if (hasAttribute(type)) {
            log_warn("ignore add, since attribute with type %u already exists.\n", type);
            return;
        }

        size_t len = RTA_LENGTH(sizeof(T));
        uint8_t *attr_buf = (uint8_t *) malloc(len);

        uint8_t *ptr = attr_buf;

        struct rtattr *attr = (struct rtattr *) ptr;
        ptr += sizeof(struct rtattr);

        attr->rta_len = len;
        attr->rta_type = type;

        memcpy(ptr, &value, sizeof(T));

        _attrs[type] = attr;
    }

    void addNestedAttribute(unsigned short type, const RtAttr &payload);
    void addRawAttribute(unsigned short type, const uint8_t *payload, size_t payload_len);

private:
    std::map<unsigned short, struct rtattr*> _attrs;

// ----------------------------------------------------------------------------

public:
    ssize_t parse(const uint8_t *from, size_t buf_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    size_t length() const;
};

}

#endif // LDP_RTATTR_H