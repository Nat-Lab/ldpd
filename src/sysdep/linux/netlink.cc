#include "sysdep/linux/netlink.hh"
#include <errno.h>

namespace ldpd {

Netlink::Netlink() {
    _fd = -1;

    memset(&_local, 0, sizeof(struct sockaddr_nl));

    _pid = getpid();
    _local.nl_family = AF_NETLINK;
    _local.nl_pid = _pid;

    _seq = 0;
}

Netlink::~Netlink() {
    close();
}

/**
 * @brief open rtnl socket.
 * 
 * @return int status. 0 on success, 1 on error.
 */
int Netlink::open() {
    if (_fd > 0) {
        log_warn("rtnl socket already opened.\n");
        return 0;
    }

    _fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

    if (_fd < 0) {
        log_fatal("socket(): %s\n", strerror(errno));
        return 1;
    }

    if (bind(_fd, (struct sockaddr *) &_local, sizeof(struct sockaddr_nl))< 0) {
        ::close(_fd);
        _fd = -1;
        log_fatal("bind(): %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

int Netlink::close() {
    if (_fd > 0) {
        ::close(_fd);
        _fd = -1;
    }

    return 0;
}

/**
 * @brief get interfaces. will block.
 * 
 * @param to location to store retrieved interfaces.
 * @return int status. 0 on success, 1 on error.
 */
int Netlink::getInterfaces(std::vector<Interface> &to) {
    unsigned int this_seq = ++_seq;

    if (sendRequest(this_seq, RTM_GETLINK, NLM_F_REQUEST | NLM_F_DUMP) < 0) {
        return 1;
    }

    bool ends = false;

    uint8_t buffer[8192];

    do {
        ssize_t res = getReply(buffer, sizeof(buffer));

        if (res < 0) {
            log_error("recvmsg(): %s\n", strerror(errno));
            return 1;
        }

        if (res == 0) {
            continue;
        }

        for (struct nlmsghdr *msg = (struct nlmsghdr *) buffer; NLMSG_OK(msg, res); msg = NLMSG_NEXT(msg, res)) {
            if (msg->nlmsg_seq != this_seq) {
                log_warn("ignored nl reply from kernel with seq %u\n", msg->nlmsg_seq);
                continue;
            }

            switch(msg->nlmsg_type) {
                case NLMSG_DONE: {
                    ends = true;
                    break;
                }
                case RTM_NEWLINK: {
                    Interface iface = Interface();
                    parseInterface(iface, msg);
                    
                    to.push_back(iface);

                    break;
                }
                default: {
                    log_warn("ignored unknown nlmsg type %u\n", msg->nlmsg_type);
                    break;
                }
            }
        }

    } while(!ends);

    return 0;
}

/**
 * @brief send a request to rtnl socket.
 * 
 * @param seq seq to id this request.
 * @param type type.
 * @param flags flags.
 * @return ssize_t ret val of sendmsg.
 */
ssize_t Netlink::sendRequest(unsigned int seq, unsigned short type, unsigned short flags) const {
    struct sockaddr_nl kernel;
    struct msghdr nl_msg;
    struct iovec io;

    nl_request_t request;

    memset(&kernel, 0, sizeof(struct sockaddr_nl));
    memset(&nl_msg, 0, sizeof(struct msghdr));
    memset(&request, 0, sizeof(nl_request_t));

    kernel.nl_family = AF_NETLINK;
    
    request.header.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    request.header.nlmsg_type = type;
    request.header.nlmsg_flags = flags;
    request.header.nlmsg_pid = _pid;
    request.header.nlmsg_seq = seq;

    // will deal with v4 only for now.
    request.message.rtgen_family = AF_INET;

    io.iov_base = &request;
    io.iov_len = sizeof(nl_request);

    nl_msg.msg_iov = &io;
    nl_msg.msg_iovlen = 1;
    nl_msg.msg_name = &kernel;
    nl_msg.msg_namelen = sizeof(struct sockaddr_nl);

    return sendmsg(_fd, (const struct msghdr *) &nl_msg, 0);
}

ssize_t Netlink::getReply(uint8_t *buffer, size_t buf_sz) const {
    struct sockaddr_nl kernel;

    memset(&kernel, 0, sizeof(struct sockaddr_nl));

    kernel.nl_family = AF_NETLINK;

    struct msghdr rslt_hdr;
    struct iovec rslt_io;

    memset(&rslt_hdr, 0, sizeof(struct msghdr));
    memset(&rslt_io, 0, sizeof(struct iovec));

    rslt_io.iov_base = buffer;
    rslt_io.iov_len = buf_sz;

    rslt_hdr.msg_iov = &rslt_io;
    rslt_hdr.msg_iovlen = 1;
    rslt_hdr.msg_name = &kernel;
    rslt_hdr.msg_namelen = sizeof(struct sockaddr_nl);

    return recvmsg(_fd, &rslt_hdr, 0);
}

/**
 * @brief parse an interface from nlmsg.
 * 
 * @param dst place to store the interface.
 * @param src source nlmsg.
 * @return int status. 0 on success, 1 on error.
 */
int Netlink::parseInterface(Interface &dst, const struct nlmsghdr *src) {
    if (src->nlmsg_type != RTM_NEWLINK) {
        log_error("bad nlmsg type %u, want %u.\n", src->nlmsg_type, RTM_NEWLINK);
        return 1;
    }

    const struct ifinfomsg *iface = (const struct ifinfomsg *) NLMSG_DATA(src);

    dst.id = iface->ifi_index;

    size_t sz = src->nlmsg_len - sizeof(struct nlmsghdr);

    for (const struct rtattr *attr = IFLA_RTA(iface); RTA_OK(attr, sz); attr = RTA_NEXT(attr, sz)) {
        switch (attr->rta_type) {
            case IFLA_IFNAME: {
                dst.ifname = std::string((const char *) RTA_DATA(attr));
                break;
            }
        }
    }

    return 0;
}

}