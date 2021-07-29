#ifndef VALUE_OP_H
#define VALUE_OP_H
#include "log.hh"

#include <stdint.h>
#include <unistd.h>
#include <string.h>

namespace ldpd {



/**
 * @brief read value from buffer.
 * 
 * read value from buffer pointer and advance the buffer pointer.
 * 
 * @tparam T type of value.
 * @param src buffer to read from.
 * @param src_sz size of the source buffer.
 * @param dst pointer to variable to write to.
 * @return ssize_t bytes written.
 */
template <typename T> ssize_t getValue(const uint8_t **src, size_t src_sz, T &dst) {
    auto *buf = *buffer;
    size_t sz = sizeof(T);
    
    if (sz > src_sz) {
        log_fatal("unexpected end of buffer when reading value: souce buffer size: %zu, variable size %zu\n", src_sz, sz);
        return -1;
    }

    memcpy(&dst, buf, sz);
    *buffer = buf + sz;
    
    return sz;
}

/**
 * @brief write value to buffer.
 * 
 * write the value to buffer pointer and advance the buffer pointer.
 * 
 * @tparam T type of value.
 * @param dst pointer to pointer to buffer.
 * @param dst_sz size of the buffer.
 * @param value value to write.
 * @return size_t bytes written.
 */
template <typename T> size_t putValue(uint8_t **dst, size_t dst_sz, const T &value) {
    uint8_t *buf = *buffer;
    size_t sz = sizeof(T);

    if (sz > dst_sz) {
        log_fatal("unexpected end of buffer when writing value: souce buffer size: %zu, variable size %zu\n", src_sz, sz);
        return -1;
    }

    memcpy(buf, &value, sz);
    *buffer = buf + sz;
    return sz;
}

#define GETVAL_S(buf_ptr, buf_sz_var, dst_var_type, dst_var, post_processing) {\
    ssize_t tmp = getValue<dst_var_type>(&buf_ptr, buf_sz_var, dst_var);\
    dst_var = post_processing(dst_var);\
    if (tmp < 0) { return -1; };\
    buf_sz_var -= tmp;\
}

#define PUTVAL_S(buf_ptr, buf_sz_var, src_var_type, src_var, pre_processing) {\
    ssize_t tmp = putValue<src_var_type>(&buf_ptr, buf_sz_var, pre_processing(src_var))\
    if (tmp < 0) { return -1; };\
    buf_sz_var -= tmp;\
}

}

#endif