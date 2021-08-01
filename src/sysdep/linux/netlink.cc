#include "sysdep/linux/netlink.hh"
#include <errno.h>
#include <arpa/inet.h>
#include <linux/mpls_iptunnel.h>

namespace ldpd {

RouteAttributes::RouteAttributes(const struct rtattr *attr, size_t len) : _attrs() {
    for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
        struct rtattr *local_copy = (struct rtattr *) malloc(attr->rta_len);
        memcpy(local_copy, attr, attr->rta_len);
        _attrs[attr->rta_type] = local_copy;
    }
}

RouteAttributes::~RouteAttributes() {
    for (std::pair<const unsigned short, struct rtattr*> attr : _attrs) {
        free(attr.second);
    }
}

const struct rtattr* RouteAttributes::getAttribute(unsigned short type) const {
    if (_attrs.count(type) == 0) {
        return nullptr;
    }

    return _attrs.at(type);
}

bool RouteAttributes::hasAttribute(unsigned short type) const {
    return _attrs.count(type);
}

Netlink::Netlink() : _saved() {
    _fd = -1;

    memset(&_local, 0, sizeof(struct sockaddr_nl));

    _pid = getpid();
    _local.nl_family = AF_NETLINK;
    _local.nl_pid = _pid;

    _seq = 0;
}

Netlink::~Netlink() {
    for (struct nlmsghdr *msg : _saved) {
        free(msg);
    }

    _saved.clear();

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

    if (sendQuery(this_seq, AF_INET, RTM_GETLINK, NLM_F_REQUEST | NLM_F_DUMP) < 0) {
        return 1;
    }

    if (getReply(this_seq, Netlink::procressInterfaceResults, &to) != 0) {
        return 1;
    }

    return 0;
}

/**
 * @brief get routes. will block.
 * 
 * @param to location to store retrieved routes.
 * @return int status. 0 on success, 1 on error.
 */
int Netlink::getIpv4Routes(std::vector<Ipv4Route> &to) {
    unsigned int this_seq = ++_seq;

    if (sendQuery(this_seq, AF_INET, RTM_GETROUTE, NLM_F_REQUEST | NLM_F_DUMP) < 0) {
        return 1;
    }

    if (getReply(this_seq, Netlink::procressIpv4RouteResults, &to) != 0) {
        return 1;
    }

    return 0;
}

int Netlink::getMplsRoutes(std::vector<MplsRoute> &to) {
    unsigned int this_seq = ++_seq;

    if (sendQuery(this_seq, AF_MPLS, RTM_GETROUTE, NLM_F_REQUEST | NLM_F_DUMP) < 0) {
        return 1;
    }

    if (getReply(this_seq, Netlink::procressMplsRouteResults, &to) != 0) {
        return 1;
    }

    return 0;
}

/**
 * @brief send a query of given type to rtnl socket.
 * 
 * @param seq seq to id this request.
 * @param type type.
 * @param flags flags.
 * @return ssize_t ret val of sendmsg.
 */
ssize_t Netlink::sendQuery(unsigned int seq, unsigned char af, unsigned short type, unsigned short flags) const {
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
    request.message.rtgen_family = af;

    io.iov_base = &request;
    io.iov_len = sizeof(nl_request);

    nl_msg.msg_iov = &io;
    nl_msg.msg_iovlen = 1;
    nl_msg.msg_name = &kernel;
    nl_msg.msg_namelen = sizeof(struct sockaddr_nl);

    return sendmsg(_fd, (const struct msghdr *) &nl_msg, 0);
}

int Netlink::getReply(unsigned int seq, int (*handler) (void *, const struct nlmsghdr *), void *data) const {
    uint8_t buffer[8192];

    struct sockaddr_nl kernel;

    memset(&kernel, 0, sizeof(struct sockaddr_nl));

    kernel.nl_family = AF_NETLINK;

    struct msghdr rslt_hdr;
    struct iovec rslt_io;

    memset(&rslt_hdr, 0, sizeof(struct msghdr));
    memset(&rslt_io, 0, sizeof(struct iovec));

    rslt_io.iov_base = buffer;
    rslt_io.iov_len = sizeof(buffer);

    rslt_hdr.msg_iov = &rslt_io;
    rslt_hdr.msg_iovlen = 1;
    rslt_hdr.msg_name = &kernel;
    rslt_hdr.msg_namelen = sizeof(struct sockaddr_nl);

    bool end = false;

    do {
        ssize_t res = recvmsg(_fd, &rslt_hdr, 0);

        if (res < 0) {
            log_error("recvmsg(): %s\n", strerror(errno));
            return 1;
        }

        if (res == 0) {
            continue;
        }

        // todo: inspect saved msgs

        for (struct nlmsghdr *msg = (struct nlmsghdr *) buffer; NLMSG_OK(msg, res); msg = NLMSG_NEXT(msg, res)) {
            if (msg->nlmsg_seq != seq) {
                log_info("reply from kernel with seq %u != %u (us): saving for the others\n", msg->nlmsg_seq, seq);
                struct nlmsghdr *saved = (struct nlmsghdr *) malloc(msg->nlmsg_len);
                memcpy(saved, msg, msg->nlmsg_len);
                continue;
            }

            if (handler(data, msg) == PROCESS_END) {
                end = true;
            }
        }

    } while (!end);

    return 0;
}

int Netlink::procressInterfaceResults(void *ifaces, const struct nlmsghdr *msg) {
    std::vector<Interface> *to = (std::vector<Interface> *) ifaces;

    switch(msg->nlmsg_type) {
        case NLMSG_DONE: {
            return PROCESS_END;
        }
        case RTM_NEWLINK: {
            Interface iface = Interface();
            if (parseInterface(iface, msg) == PRASE_OK) {
                to->push_back(iface);
            }

            break;
        }
        default: {
            log_warn("ignored unknown nlmsg type %u\n", msg->nlmsg_type);
            break;
        }
    }

    return PROCESS_NEXT;
}

int Netlink::procressIpv4RouteResults(void *routes, const struct nlmsghdr *msg) {
    std::vector<Ipv4Route> *to = (std::vector<Ipv4Route> *) routes;

    switch(msg->nlmsg_type) {
        case NLMSG_DONE: {
            return PROCESS_END;
        }
        case RTM_NEWROUTE: {
            Ipv4Route r = Ipv4Route();
            if (parseIpv4Route(r, msg) == PRASE_OK) {
                to->push_back(r);
            }
            
            break;
        };
        default: {
            log_warn("ignored unknown nlmsg type %u\n", msg->nlmsg_type);
            break;
        }
    }

    return PROCESS_NEXT;
}

int Netlink::procressMplsRouteResults(void *routes, const struct nlmsghdr *msg) {
    std::vector<MplsRoute> *to = (std::vector<MplsRoute> *) routes;

    switch(msg->nlmsg_type) {
        case NLMSG_DONE: {
            return PROCESS_END;
        }
        case RTM_NEWROUTE: {
            MplsRoute r = MplsRoute();
            if (parseMplsRoute(r, msg) == PRASE_OK) {
                to->push_back(r);
            }
            
            break;
        };
        default: {
            log_warn("ignored unknown nlmsg type %u\n", msg->nlmsg_type);
            break;
        }
    }

    return PROCESS_NEXT;
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
        return PRASE_SKIP;
    }

    const struct ifinfomsg *iface = (const struct ifinfomsg *) NLMSG_DATA(src);

    dst.id = iface->ifi_index;

    RouteAttributes attrs = RouteAttributes(IFLA_RTA(iface), RTM_PAYLOAD(src));

    if (attrs.hasAttribute(IFLA_IFNAME)) {
        dst.ifname = std::string((const char*) RTA_DATA(attrs.getAttribute(IFLA_IFNAME)));
    } else {
        log_error("no ifla_ifname attribute.\n");
        return PRASE_SKIP;
    }

    return PRASE_OK;
}

int Netlink::parseIpv4Route(Ipv4Route &dst, const struct nlmsghdr *src) {
    if (src->nlmsg_type != RTM_NEWROUTE) {
        log_error("bad nlmsg type %u, want %u.\n", src->nlmsg_type, RTM_NEWROUTE);
        return PRASE_SKIP;
    }

    const struct rtmsg *rt = (const struct rtmsg *) NLMSG_DATA(src);

    if (rt->rtm_type != RTN_UNICAST || rt->rtm_table != RT_TABLE_MAIN) {
        return PRASE_SKIP;
    }


    dst.dst_len = rt->rtm_dst_len;
    dst.mpls_encap = false;
    dst.mpls_stack = std::vector<uint32_t>();
    dst.mpls_ttl = 255;

    RouteAttributes attrs = RouteAttributes(RTM_RTA(rt), RTM_PAYLOAD(src));

    if (!attrs.getAttributeValue(RTA_DST, dst.dst)) { log_warn("ignored a route w/ no rta_dst.\n"); return PRASE_SKIP; }
    if (!attrs.getAttributeValue(RTA_OIF, dst.oif)) { log_warn("ignored a route w/ no rta_oif.\n"); return PRASE_SKIP; }
    if (!attrs.getAttributeValue(RTA_GATEWAY, dst.gw)) { log_warn("ignored a route w/ no rta_gw.\n"); return PRASE_SKIP; }

    short enctype;

    if (!attrs.getAttributeValue(RTA_ENCAP_TYPE, enctype)) {
        return PRASE_OK;
    }

    if (enctype != LWTUNNEL_ENCAP_MPLS) {
        log_warn("unsupported enc %u, skipping route.\n", enctype);
        return PRASE_SKIP;
    }

    const struct rtattr* encap_attr_val = nullptr;

    if (!attrs.getAttributePointer(RTA_ENCAP, encap_attr_val)) {
        log_error("rta_encap_type == mpls, but no rta_encap found.\n");
        return PRASE_SKIP;
    }

    dst.mpls_encap = true;

    RouteAttributes mpls_info = RouteAttributes(encap_attr_val, encap_attr_val->rta_len);

    const uint32_t *labels;

    if (!mpls_info.getAttributePointer(MPLS_IPTUNNEL_DST, labels)) {
        log_error("mpls_tun_dst not found in mpls encap rattr.\n");
        return PRASE_SKIP;
    }

    size_t lables_arr_len = RTA_PAYLOAD(mpls_info.getAttribute(MPLS_IPTUNNEL_DST));

    if (lables_arr_len % sizeof(uint32_t) != 0) {
        log_error("mpls lbl arr %% sizeof(uint32_t) != 0, what?\n");
        return PRASE_SKIP;
    }

    for (size_t i = 0; i < lables_arr_len/sizeof(uint32_t); ++i) {
        dst.mpls_stack.push_back(ntohl(labels[i]) >> 12);
    }

    mpls_info.getAttributeValue(MPLS_IPTUNNEL_TTL, dst.mpls_ttl);

    return PRASE_OK;
}

int Netlink::parseMplsRoute(MplsRoute &dst, const struct nlmsghdr *src) {
    if (src->nlmsg_type != RTM_NEWROUTE) {
        log_error("bad nlmsg type %u, want %u.\n", src->nlmsg_type, RTM_NEWROUTE);
        return PRASE_SKIP;
    }

    const struct rtmsg *rt = (const struct rtmsg *) NLMSG_DATA(src);

    if (rt->rtm_type != RTN_UNICAST || rt->rtm_table != RT_TABLE_MAIN) {
        return PRASE_SKIP;
    }

    dst.mpls_encap = false;
    dst.mpls_stack = std::vector<uint32_t>();

    RouteAttributes attrs = RouteAttributes(RTM_RTA(rt), RTM_PAYLOAD(src));

    if (!attrs.getAttributeValue(RTA_DST, dst.in_label)) { log_warn("ignored a route w/ no rta_dst.\n"); return PRASE_SKIP; }

    const struct rtvia *via;

    if (!attrs.getAttributePointer(RTA_VIA, via)) { log_warn("ignored a route w/ no rta_via.\n"); return PRASE_SKIP; }

    if (via->rtvia_family != AF_INET) {
        log_error("unsupported af: %u\n", via->rtvia_family);
        return PRASE_SKIP;
    }

    dst.gw = *(uint32_t *) via->rtvia_addr;   

    const uint32_t *labels;

    if (!attrs.getAttributePointer(RTA_NEWDST, labels)) {
        return PRASE_OK;
    }

    dst.mpls_encap = true;

    size_t lables_arr_len = RTA_PAYLOAD(attrs.getAttribute(RTA_NEWDST));

    if (lables_arr_len % sizeof(uint32_t) != 0) {
        log_error("mpls lbl arr %% sizeof(uint32_t) != 0, what?\n");
        return PRASE_SKIP;
    }

    for (size_t i = 0; i < lables_arr_len/sizeof(uint32_t); ++i) {
        dst.mpls_stack.push_back(ntohl(labels[i]) >> 12);
    }

    return PRASE_OK;

}

}