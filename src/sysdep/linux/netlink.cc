#include "sysdep/linux/netlink.hh"
#include "sysdep/linux/rtattr.hh"
#include <errno.h>
#include <arpa/inet.h>
#include <linux/mpls_iptunnel.h>

namespace ldpd {

Netlink::Netlink() : _saved() {
    _fd = -1;

    memset(&_local, 0, sizeof(struct sockaddr_nl));
    memset(&_kernel, 0, sizeof(struct sockaddr_nl));

    _kernel.nl_family = AF_NETLINK;

    _pid = getpid();
    _local.nl_family = AF_NETLINK;
    _local.nl_pid = _pid;

    // 0x4000000 is group for mpls-route. see "lab/mpls-nl-group.c"
    _local.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_ROUTE | RTMGRP_IPV4_IFADDR | 0x4000000;

    _seq = 0;

    _buffer = nullptr;
    _bufsz = 0;

    _lc_handler = nullptr;
    _ac_handler = nullptr;
    _irc_handler = nullptr;
    _mrc_handler = nullptr;
}

Netlink::~Netlink() {
    for (struct nlmsghdr *msg : _saved) {
        free(msg);
    }

    if (_buffer != nullptr) {
        free(_buffer);
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
    int seq = sendGeneralQuery(AF_INET, RTM_GETLINK, NLM_F_REQUEST | NLM_F_DUMP);

    if (seq < 0) {
        return 1;
    }

    if (getReply((unsigned int) seq, Netlink::procressInterfaceResults, &to) != 0) {
        return 1;
    }

    seq = sendGeneralQuery(AF_INET, RTM_GETADDR, NLM_F_REQUEST | NLM_F_DUMP);

    if (getReply((unsigned int) seq, Netlink::procressInterfaceResults, &to) != 0) {
        return 1;
    }

    return 0;
}

int Netlink::getRoutes(std::vector<Ipv4Route> &to) {
    int seq = sendGeneralQuery(AF_INET, RTM_GETROUTE, NLM_F_REQUEST | NLM_F_DUMP);

    if (seq < 0) {
        return 1;
    }

    if (getReply((unsigned int) seq, Netlink::procressIpv4RouteResults, &to) != 0) {
        return 1;
    }

    return 0;
}

int Netlink::getRoutes(std::vector<MplsRoute> &to) {
    int seq = sendGeneralQuery(AF_MPLS, RTM_GETROUTE, NLM_F_REQUEST | NLM_F_DUMP);

    if (seq < 0) {
        return 1;
    }

    if (getReply((unsigned int) seq, Netlink::procressMplsRouteResults, &to) != 0) {
        return 1;
    }

    return 0;
}

int Netlink::sendRouteMessage(const Route *route, unsigned short type, unsigned short flags) {
    unsigned int seq = ++_seq;

    uint8_t buffer[8192];
    uint8_t *ptr = buffer;

    memset(buffer, 0, 8192);

    struct nlmsghdr *msghdr = (struct nlmsghdr *) ptr;
    ptr += sizeof(struct nlmsghdr);

    struct rtmsg *rtmsg = (struct rtmsg *) ptr;
    ptr += sizeof(struct rtmsg);

    rtmsg->rtm_table = RT_TABLE_MAIN;
    rtmsg->rtm_protocol = RTPROT_STATIC;
    rtmsg->rtm_scope = RT_SCOPE_UNIVERSE;
    rtmsg->rtm_type = RTN_UNICAST;

    RtAttr attrs = RtAttr();

    if (route->getType() == RouteType::Ipv4) {
        const Ipv4Route *r = (const Ipv4Route *) route;
        rtmsg->rtm_dst_len = r->dst_len;
        rtmsg->rtm_family = AF_INET;

        if (buildRtAttr(*r, attrs) < 0) {
            return 1;
        }
    } else if (route->getType() == RouteType::Mpls) {
        const MplsRoute *r = (const MplsRoute *) route;
        rtmsg->rtm_dst_len = 20;
        rtmsg->rtm_family = AF_MPLS;

        if (buildRtAttr(*r, attrs) < 0) {
            return 1;
        }
    } else {
        log_error("unknow route type %d.\n", route->getType());
        return 1;
    }

    size_t buffer_left = sizeof(buffer) - sizeof(struct nlmsghdr) - sizeof(struct rtmsg);
    
    ssize_t attrs_len = attrs.write(ptr, buffer_left);

    if (attrs_len < 0) {
        return 1;
    }

    size_t msglen = (size_t) attrs_len + sizeof(struct rtmsg);

    msghdr->nlmsg_len = NLMSG_LENGTH(msglen);
    msghdr->nlmsg_pid = _pid;
    msghdr->nlmsg_seq = seq;
    msghdr->nlmsg_flags = flags;
    msghdr->nlmsg_type = type;

    if (sendMessage(msghdr) < 0) {
        log_error("sendMessage(): %s\n", strerror(errno));
        return 1;
    }

    return getReply((unsigned int) seq, Netlink::commonAckHandler, (void *) __FUNCTION__);
}

int Netlink::sendGeneralQuery(unsigned char af, unsigned short type, unsigned short flags) {
    unsigned int seq = ++_seq;

    uint8_t buffer[NLMSG_LENGTH(sizeof(struct rtgenmsg))];
    memset(buffer, 0, sizeof(buffer));

    uint8_t *ptr = buffer;

    struct nlmsghdr *msghdr = (struct nlmsghdr *) ptr;
    ptr += sizeof(struct nlmsghdr);

    struct rtgenmsg *msg = (struct rtgenmsg *) ptr;
    msg->rtgen_family = af;

    msghdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    msghdr->nlmsg_type = type;
    msghdr->nlmsg_flags = flags;
    msghdr->nlmsg_pid = _pid;
    msghdr->nlmsg_seq = seq;

    if (sendMessage(msghdr) < 0) {
        log_error("sendMessage(): %s\n", strerror(errno));
        return -1;
    }

    return seq;
}

ssize_t Netlink::sendMessage(const void *msg) {
    // not thread safe, may need mutex?
    struct msghdr msghdr;

    memset(&msghdr, 0, sizeof(struct msghdr));

    struct nlmsghdr *nl_msghdr = (struct nlmsghdr *) msg;

    if (nl_msghdr->nlmsg_len > _bufsz) {
        if (_buffer != nullptr) {
            free(_buffer);
        }

        _buffer = malloc(nl_msghdr->nlmsg_len);
    }

    memcpy(_buffer, msg, nl_msghdr->nlmsg_len);

    _io.iov_base = _buffer;
    _io.iov_len = nl_msghdr->nlmsg_len;

    msghdr.msg_iov = &_io;
    msghdr.msg_iovlen = 1;
    msghdr.msg_name = &_kernel; 
    msghdr.msg_namelen = sizeof(struct sockaddr_nl);

    return sendmsg(_fd, (const struct msghdr *) &msghdr, 0);
}

int Netlink::getReply(unsigned int seq, int (*handler) (void *, const struct nlmsghdr *), void *data) {
    uint8_t buffer[8192];
    
    memset(buffer, 0, sizeof(buffer));

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

            int rslt = handler(data, msg);

            if (rslt == PROCESS_END) {
                end = true;
            }

            if (rslt == PROCESS_NEXT) {
                continue;
            }

            if (rslt == PROCESS_ERR) {
                return 1;
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
            
            if (parseNetlinkMessage(iface, msg) == PARSE_OK) {
                to->push_back(iface);
            }

            break;
        }
        case RTM_NEWADDR: {
            InterfaceAddress addr = InterfaceAddress();

            if (parseNetlinkMessage(addr, msg) == PARSE_OK) {
                bool pushed = false;

                for (Interface &iface : *to) {
                    if (iface.index == addr.ifindex) {
                        pushed = true;
                        iface.addresses.push_back(addr);
                    }
                }

                if (!pushed) {
                    log_warn("got address for unknow interface %d.\n", addr.ifindex);
                }
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

int Netlink::procressIpv4RouteResults(void *routes, const struct nlmsghdr *msg) {
    std::vector<Ipv4Route> *to = (std::vector<Ipv4Route> *) routes;

    switch(msg->nlmsg_type) {
        case NLMSG_DONE: {
            return PROCESS_END;
        }
        case RTM_NEWROUTE: {
            Ipv4Route r = Ipv4Route();
            if (parseNetlinkMessage(r, msg) == PARSE_OK) {
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
            if (parseNetlinkMessage(r, msg) == PARSE_OK) {
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

int Netlink::parseNetlinkMessage(InterfaceAddress &dst, const struct nlmsghdr *src) {
    if (src->nlmsg_type != RTM_NEWADDR && src->nlmsg_type != RTM_DELADDR) {
        log_error("bad nlmsg type %u.\n", src->nlmsg_type);
        return PARSE_SKIP;
    }

    const struct ifaddrmsg *addr = (const struct ifaddrmsg *) NLMSG_DATA(src);

    if (addr->ifa_family != AF_INET) {
        log_warn("ignored a non-ipv6 interface address.\n");
        return PARSE_SKIP;
    }

    RtAttr attrs = RtAttr();
    attrs.parse((uint8_t *) IFA_RTA(addr), RTM_PAYLOAD(src));

    dst.ifindex = addr->ifa_index;
    dst.address.len = addr->ifa_prefixlen;

    if (!attrs.getAttributeValue(IFA_ADDRESS, dst.address.prefix)) {
        log_error("address has no ifa_address.\n");
        return PARSE_SKIP;
    }

    return PARSE_OK;
}

int Netlink::parseNetlinkMessage(Interface &dst, const struct nlmsghdr *src) {
    if (src->nlmsg_type != RTM_NEWLINK && src->nlmsg_type != RTM_DELLINK) {
        log_error("bad nlmsg type %u.\n", src->nlmsg_type);
        return PARSE_SKIP;
    }

    const struct ifinfomsg *iface = (const struct ifinfomsg *) NLMSG_DATA(src);

    dst.index = iface->ifi_index;

    RtAttr attrs = RtAttr();
    attrs.parse((uint8_t *) IFLA_RTA(iface), RTM_PAYLOAD(src));

    if (attrs.hasAttribute(IFLA_IFNAME)) {
        dst.ifname = std::string((const char*) RTA_DATA(attrs.getAttribute(IFLA_IFNAME)));
    } else {
        log_error("no ifla_ifname attribute.\n");
        return PARSE_SKIP;
    }

    return PARSE_OK;
}

int Netlink::parseNetlinkMessage(Ipv4Route &dst, const struct nlmsghdr *src) {
    if (src->nlmsg_type != RTM_NEWROUTE && src->nlmsg_type != RTM_DELROUTE) {
        log_error("bad nlmsg type %u.\n", src->nlmsg_type);
        return PARSE_SKIP;
    }

    const struct rtmsg *rt = (const struct rtmsg *) NLMSG_DATA(src);

    if (rt->rtm_type != RTN_UNICAST || rt->rtm_table != RT_TABLE_MAIN) {
        return PARSE_SKIP;
    }


    dst.dst_len = rt->rtm_dst_len;
    dst.mpls_encap = false;
    dst.mpls_stack = std::vector<uint32_t>();
    dst.mpls_ttl = 255;

    RtAttr attrs = RtAttr();
    attrs.parse((uint8_t *) RTM_RTA(rt), RTM_PAYLOAD(src));

    if (!attrs.getAttributeValue(RTA_DST, dst.dst)) { log_warn("ignored a route w/ no rta_dst.\n"); return PARSE_SKIP; }
    if (!attrs.getAttributeValue(RTA_OIF, dst.oif)) { log_warn("ignored a route w/ no rta_oif.\n"); return PARSE_SKIP; }
    if (!attrs.getAttributeValue(RTA_GATEWAY, dst.gw)) { log_warn("ignored a route w/ no rta_gw.\n"); return PARSE_SKIP; }

    short enctype;

    if (!attrs.getAttributeValue(RTA_ENCAP_TYPE, enctype)) {
        return PARSE_OK;
    }

    if (enctype != LWTUNNEL_ENCAP_MPLS) {
        log_warn("unsupported enc %u, skipping route.\n", enctype);
        return PARSE_SKIP;
    }

    const struct rtattr* encap_attr_val = nullptr;

    if (!attrs.getAttributePointer(RTA_ENCAP, encap_attr_val)) {
        log_error("rta_encap_type == mpls, but no rta_encap found.\n");
        return PARSE_SKIP;
    }

    dst.mpls_encap = true;

    RtAttr mpls_info = RtAttr();
    mpls_info.parse((uint8_t *) encap_attr_val, encap_attr_val->rta_len);

    const uint32_t *labels;

    if (!mpls_info.getAttributePointer(MPLS_IPTUNNEL_DST, labels)) {
        log_error("mpls_tun_dst not found in mpls encap rattr.\n");
        return PARSE_SKIP;
    }

    size_t labels_arr_len = RTA_PAYLOAD(mpls_info.getAttribute(MPLS_IPTUNNEL_DST));

    if (labels_arr_len % sizeof(uint32_t) != 0) {
        log_error("mpls lbl arr %% sizeof(uint32_t) != 0, what?\n");
        return PARSE_SKIP;
    }

    for (size_t i = 0; i < labels_arr_len/sizeof(uint32_t); ++i) {
        dst.mpls_stack.push_back(ntohl(labels[i]) >> 12);
    }

    mpls_info.getAttributeValue(MPLS_IPTUNNEL_TTL, dst.mpls_ttl);

    return PARSE_OK;
}

int Netlink::parseNetlinkMessage(MplsRoute &dst, const struct nlmsghdr *src) {
    if (src->nlmsg_type != RTM_NEWROUTE && src->nlmsg_type != RTM_DELROUTE) {
        log_error("bad nlmsg type %u.\n", src->nlmsg_type);
        return PARSE_SKIP;
    }

    const struct rtmsg *rt = (const struct rtmsg *) NLMSG_DATA(src);

    if (rt->rtm_type != RTN_UNICAST || rt->rtm_table != RT_TABLE_MAIN) {
        return PARSE_SKIP;
    }

    dst.mpls_encap = false;
    dst.mpls_stack = std::vector<uint32_t>();

    RtAttr attrs = RtAttr();
    attrs.parse((uint8_t *) RTM_RTA(rt), RTM_PAYLOAD(src));

    if (!attrs.getAttributeValue(RTA_DST, dst.in_label)) { log_warn("ignored a route w/ no rta_dst.\n"); return PARSE_SKIP; }
    if (!attrs.getAttributeValue(RTA_OIF, dst.oif)) { log_warn("ignored a route w/ no rta_oif.\n"); return PARSE_SKIP; }

    dst.in_label = ntohl(dst.in_label) >> 12;

    const struct rtvia *via;

    if (!attrs.getAttributePointer(RTA_VIA, via)) { log_warn("ignored a route w/ no rta_via.\n"); return PARSE_SKIP; }

    if (via->rtvia_family != AF_INET) {
        log_error("unsupported af: %u\n", via->rtvia_family);
        return PARSE_SKIP;
    }

    dst.gw = *(uint32_t *) via->rtvia_addr;   

    const uint32_t *labels;

    if (!attrs.getAttributePointer(RTA_NEWDST, labels)) {
        return PARSE_OK;
    }

    dst.mpls_encap = true;

    size_t labels_arr_len = RTA_PAYLOAD(attrs.getAttribute(RTA_NEWDST));

    if (labels_arr_len % sizeof(uint32_t) != 0) {
        log_error("mpls lbl arr %% sizeof(uint32_t) != 0, what?\n");
        return PARSE_SKIP;
    }

    for (size_t i = 0; i < labels_arr_len/sizeof(uint32_t); ++i) {
        dst.mpls_stack.push_back(ntohl(labels[i]) >> 12);
    }

    return PARSE_OK;

}

int Netlink::commonAckHandler(void *caller, const struct nlmsghdr *msg) {
    if (msg->nlmsg_type == NLMSG_DONE) {
        return PROCESS_END;
    }

    if (msg->nlmsg_type == NLMSG_ERROR) {
        const nlmsgerr *err = (const struct nlmsgerr *) NLMSG_DATA(msg);

        if (err->error == 0) {
            return PROCESS_END;
        }

        log_error("rtnl reported error: %s (requested by %s)\n", strerror(-err->error), (const char *) caller);

        return PROCESS_ERR;
    }

    return PROCESS_NEXT;
}

int Netlink::buildRtAttr(const Ipv4Route &route, RtAttr &attrs) {
    attrs.addAttribute(RTA_OIF, route.oif);
    attrs.addAttribute(RTA_DST, route.dst);
    attrs.addAttribute(RTA_GATEWAY, route.gw);
    attrs.addAttribute(RTA_PRIORITY, route.metric);

    if (route.mpls_encap && route.mpls_stack.size() > 0) {
        short type = LWTUNNEL_ENCAP_MPLS;
        attrs.addAttribute(RTA_ENCAP_TYPE, type);

        RtAttr nested = RtAttr();

        if (route.mpls_ttl == 0) {
            log_error("bad mpls ttl: cannot be 0.\n");
            return 1;
        }

        if (route.mpls_ttl != 255) {
            nested.addAttribute(MPLS_IPTUNNEL_TTL, route.mpls_ttl);
        }

        size_t stack_val_sz = sizeof(uint32_t) * route.mpls_stack.size();

        uint32_t *stack_buf = (uint32_t *) malloc(stack_val_sz);

        int idx = 0;
        for (const uint32_t &label : route.mpls_stack) {
            stack_buf[idx++] = htonl(label << 12);
        }

        stack_buf[idx - 1] |= htonl(0x100);

        nested.addRawAttribute(MPLS_IPTUNNEL_DST, (uint8_t *) stack_buf, stack_val_sz);

        free(stack_buf);

        attrs.addNestedAttribute(RTA_ENCAP, nested);
    }

    return 0;
}

int Netlink::buildRtAttr(const MplsRoute &route, RtAttr &attrs) {
    if (route.mpls_encap && route.mpls_stack.size() > 0) {
        size_t stack_val_sz = sizeof(uint32_t) * route.mpls_stack.size();

        uint32_t *stack_buf = (uint32_t *) malloc(stack_val_sz);

        int idx = 0;
        for (const uint32_t &label : route.mpls_stack) {
            stack_buf[idx++] = htonl(label << 12);
        }

        stack_buf[idx - 1] |= htonl(0x100);

        attrs.addRawAttribute(RTA_NEWDST, (uint8_t *) stack_buf, stack_val_sz);

        free(stack_buf);
    }

    attrs.addAttribute(RTA_OIF, route.oif);
    attrs.addAttribute(RTA_PRIORITY, route.metric);

    uint32_t lbl_val = htonl(route.in_label << 12 | 0x100);
    attrs.addAttribute(RTA_DST, lbl_val);

    size_t via_val_sz = sizeof(struct rtvia) + sizeof(uint32_t);
    uint8_t via_buf[sizeof(struct rtvia) + sizeof(uint32_t)];

    struct rtvia *via = (struct rtvia *) via_buf;
    via->rtvia_family = AF_INET;
    memcpy(via_buf + sizeof(struct rtvia), &(route.gw), sizeof(uint32_t));

    attrs.addRawAttribute(RTA_VIA, via_buf, via_val_sz);

    return 0;
}

void Netlink::tick() {
    uint8_t buffer[8192];
    
    memset(buffer, 0, sizeof(buffer));

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

    ssize_t res = recvmsg(_fd, &rslt_hdr, MSG_DONTWAIT);

    if (res < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            return;
        }

        log_error("recvmsg(): %s (%zu)\n", strerror(errno), res);
        return;
    }

    if (res == 0) {
        return;
    }

    for (struct nlmsghdr *msg = (struct nlmsghdr *) buffer; NLMSG_OK(msg, res); msg = NLMSG_NEXT(msg, res)) {
        if (msg->nlmsg_type == RTM_NEWROUTE || msg->nlmsg_type == RTM_DELROUTE) {
            const struct rtmsg *rt = (const struct rtmsg *) NLMSG_DATA(msg);
            if (rt->rtm_family == AF_INET) {
                log_debug("route-change: ipv4 route %s.\n", msg->nlmsg_type == RTM_NEWROUTE ? "added/replaced" : "deleted");

                if (_irc_handler == nullptr) {
                    continue;
                }

                Ipv4Route r = Ipv4Route();
                if (parseNetlinkMessage(r, msg) == PARSE_OK) {
                    _irc_handler(_irc_handler_d, msg->nlmsg_type == RTM_NEWROUTE ? NetlinkChange::Added : NetlinkChange::Deleted, r);
                }
            }

            if (rt->rtm_family == AF_MPLS) {
                log_debug("route-change: mpls route %s.\n", msg->nlmsg_type == RTM_NEWROUTE ? "added/replaced" : "deleted");

                if (_irc_handler == nullptr) {
                    continue;
                }

                MplsRoute r = MplsRoute();
                if (parseNetlinkMessage(r, msg) == PARSE_OK) {
                    _mrc_handler(_mrc_handler_d, msg->nlmsg_type == RTM_NEWROUTE ? NetlinkChange::Added : NetlinkChange::Deleted, r);
                }
            }

            continue;
        }

        if (msg->nlmsg_type == RTM_NEWLINK || msg->nlmsg_type == RTM_DELLINK) {
            log_debug("link-change: link %s.\n", msg->nlmsg_type == RTM_NEWLINK ? "added/changed" : "deleted");

            if (_lc_handler == nullptr) {
                continue;
            }

            Interface iface = Interface();

            if (parseNetlinkMessage(iface, msg) == PARSE_OK) {
                _lc_handler(_lc_handler_d, msg->nlmsg_type == RTM_NEWLINK ? NetlinkChange::Added : NetlinkChange::Deleted, iface);
            }

            continue;
        }

        if (msg->nlmsg_type == RTM_NEWADDR || msg->nlmsg_type == RTM_DELADDR) {
            log_debug("address-change: address %s.\n", msg->nlmsg_type == RTM_NEWADDR ? "added/changed" : "deleted");

            if (_lc_handler == nullptr) {
                continue;
            }

            InterfaceAddress addr = InterfaceAddress();

            if (parseNetlinkMessage(addr, msg) == PARSE_OK) {
                _ac_handler(_ac_handler_d, msg->nlmsg_type == RTM_NEWADDR ? NetlinkChange::Added : NetlinkChange::Deleted, addr);
            }

            continue;
        }

        log_debug("unhandled netlink message: type %u.\n", msg->nlmsg_type);
    }
}

void Netlink::onLinkChanges(linkchange_handler_t handler, void *data) {
    _lc_handler = handler;
    _lc_handler_d = data;
}

void Netlink::onAddrChanges(addrchange_handler_t handler, void *data) {
    _ac_handler = handler;
    _ac_handler_d = data;
}

void Netlink::onIpv4RouteChanges(ipv4_routechange_handler_t handler, void *data) {
    _irc_handler = handler;
    _irc_handler_d = data;
}

void Netlink::onMplsRouteChanges(mpls_routechange_handler_t handler, void *data) {
    _mrc_handler = handler;
    _mrc_handler_d = data;
}

}