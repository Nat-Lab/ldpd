#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H
#include <stdint.h>
#include <unistd.h>

namespace ldpd {

class Serializable {
public:
    ssize_t print(uint8_t *to, size_t buf_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz) const;

    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;
    virtual ssize_t length() const = 0;
};

}

#endif // SERIALIZABLE_H