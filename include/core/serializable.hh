#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H
#include <stdint.h>
#include <unistd.h>

#define WRITE_S(dst_buf, dst_buf_sz_var, src_obj, err_ret) {\
    ssize_t ret = src_obj->write(dst_buf, dst_buf_sz_var);\
    if (ret < 0) { return err_ret; }\
    dst_buf += ret; dst_buf_sz_var -= ret;\
}

#define PARSE_S(src_buf, src_buf_sz_var, dst_obj, err_ret, free_on_err) {\
    ssize_t ret = dst_obj->parse(src_buf, src_buf_sz_var);\
    if (ret < 0) { if (free_on_err) { delete dst_obj; }; return err_ret; }\
    src_buf += ret; src_buf_sz_var -= ret;\
}


namespace ldpd {

class Serializable {
public:
    virtual ~Serializable() {};

    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;
    virtual size_t length() const = 0;
};

}

#endif // SERIALIZABLE_H