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

    _seq = 0;

    _buffer = nullptr;
    _bufsz = 0;
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

    return 0;
}

/**
 * @brief get routes. will block.
 * 
 * @param to location to store retrieved routes.
 * @return int status. 0 on success, 1 on error.
 */
int Netlink::getIpv4Routes(std::vector<Ipv4Route> &to) {
    int seq = sendGeneralQuery(AF_INET, RTM_GETROUTE, NLM_F_REQUEST | NLM_F_DUMP);

    if (seq < 0) {
        return 1;
    }

    if (getReply((unsigned int) seq, Netlink::procressIpv4RouteResults, &to) != 0) {
        return 1;
    }

    return 0;
}

int Netlink::getMplsRoutes(std::vector<MplsRoute> &to) {
    int seq = sendGeneralQuery(AF_MPLS, RTM_GETROUTE, NLM_F_REQUEST | NLM_F_DUMP);

    if (seq < 0) {
        return 1;
    }

    if (getReply((unsigned int) seq, Netlink::procressMplsRouteResults, &to) != 0) {
        return 1;
    }

    return 0;
}

int Netlink::addMplsRoute(const MplsRoute &route, bool replace) {
    unsigned int seq = ++_seq;

    uint8_t buffer[8192];
    uint8_t *ptr = buffer;

    memset(buffer, 0, 8192);

    struct nlmsghdr *msghdr = (struct nlmsghdr *) ptr;
    ptr += sizeof(struct nlmsghdr);

    uint8_t *msg_start = ptr;

    struct rtmsg *rtmsg = (struct rtmsg *) ptr;
    ptr += sizeof(struct rtmsg);

    rtmsg->rtm_family = AF_MPLS;
    rtmsg->rtm_table = RT_TABLE_MAIN;
    rtmsg->rtm_protocol = RTPROT_STATIC;
    rtmsg->rtm_scope = RT_SCOPE_UNIVERSE;
    rtmsg->rtm_type = RTN_UNICAST;
    rtmsg->rtm_dst_len = 20;

    // add oif attr
    struct rtattr *oif_attr_hdr = (struct rtattr *) ptr;
    ptr += sizeof(struct rtattr);

    oif_attr_hdr->rta_type = RTA_OIF;
    oif_attr_hdr->rta_len = RTA_LENGTH(sizeof(int));

    memcpy(ptr, &(route.oif), sizeof(int));
    ptr += sizeof(int);

    // add dst attr
    struct rtattr *dst_attr_hdr = (struct rtattr *) ptr;
    ptr += sizeof(struct rtattr);

    dst_attr_hdr->rta_type = RTA_DST;
    dst_attr_hdr->rta_len = RTA_LENGTH(sizeof(uint32_t));

    uint32_t lbl_val = htonl(route.in_label << 12 | 0x100);

    memcpy(ptr, &lbl_val, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if (route.mpls_encap) {
        uint8_t *backup = ptr;

        // add out label stack
        struct rtattr *new_dst_attr_hdr = (struct rtattr *) ptr;
        ptr += sizeof(struct rtattr);

        new_dst_attr_hdr->rta_type = RTA_NEWDST;
        new_dst_attr_hdr->rta_len = RTA_LENGTH(route.mpls_stack.size() * sizeof(uint32_t));

        uint32_t *last_lbl = nullptr;

        for (const uint32_t &label : route.mpls_stack) {
            uint32_t lbl_val = htonl(label << 12);
            memcpy(ptr, &lbl_val, sizeof(uint32_t));
            last_lbl = (uint32_t *) ptr;
            ptr += sizeof(uint32_t);
        }

        if (last_lbl == nullptr) {
            log_warn("no stack set but mpls_encap = true. do not do this.\n");
            ptr = backup;
        } else {
            *last_lbl = *last_lbl | htonl(0x100);
        }
    }

    // add via addr
    struct rtattr *via_attr_hdr = (struct rtattr *) ptr;
    ptr += sizeof(struct rtattr);

    via_attr_hdr->rta_type = RTA_VIA;
    via_attr_hdr->rta_len = RTA_LENGTH(sizeof(struct rtvia) + sizeof(uint32_t));

    struct rtvia *via = (struct rtvia *) ptr;
    ptr += sizeof(struct rtvia);

    via->rtvia_family = AF_INET;

    memcpy(ptr, &(route.gw), sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    size_t msglen = ptr - msg_start;

    msghdr->nlmsg_len = NLMSG_LENGTH(msglen);
    msghdr->nlmsg_pid = _pid;
    msghdr->nlmsg_seq = seq;
    msghdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | (replace ? NLM_F_REPLACE : NLM_F_EXCL);
    msghdr->nlmsg_type = RTM_NEWROUTE;

    if (sendMessage(msghdr) < 0) {
        log_error("sendMessage(): %s\n", strerror(errno));
        return -1;
    }

    return getReply((unsigned int) seq, Netlink::commonAckHandler, (char *) __FUNCTION__);
}

int Netlink::addIpv4Route(const Ipv4Route &route, bool replace) {
    unsigned int seq = ++_seq;

    uint8_t buffer[8192];
    uint8_t *ptr = buffer;

    memset(buffer, 0, 8192);

    struct nlmsghdr *msghdr = (struct nlmsghdr *) ptr;
    ptr += sizeof(struct nlmsghdr);

    uint8_t *msg_start = ptr;

    struct rtmsg *rtmsg = (struct rtmsg *) ptr;
    ptr += sizeof(struct rtmsg);

    rtmsg->rtm_family = AF_INET;
    rtmsg->rtm_table = RT_TABLE_MAIN;
    rtmsg->rtm_protocol = RTPROT_STATIC;
    rtmsg->rtm_scope = RT_SCOPE_UNIVERSE;
    rtmsg->rtm_type = RTN_UNICAST;
    rtmsg->rtm_dst_len = route.dst_len;

    // add oif attr
    struct rtattr *oif_attr_hdr = (struct rtattr *) ptr;
    ptr += sizeof(struct rtattr);

    oif_attr_hdr->rta_type = RTA_OIF;
    oif_attr_hdr->rta_len = RTA_LENGTH(sizeof(int));

    memcpy(ptr, &(route.oif), sizeof(int));
    ptr += sizeof(int);

    // add dst attr
    struct rtattr *dst_attr_hdr = (struct rtattr *) ptr;
    ptr += sizeof(struct rtattr);

    dst_attr_hdr->rta_type = RTA_DST;
    dst_attr_hdr->rta_len = RTA_LENGTH(sizeof(uint32_t));

    memcpy(ptr, &(route.dst), sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // add gw attr
    struct rtattr *gw_attr_hdr = (struct rtattr *) ptr;
    ptr += sizeof(struct rtattr);

    gw_attr_hdr->rta_type = RTA_GATEWAY;
    gw_attr_hdr->rta_len = RTA_LENGTH(sizeof(uint32_t));

    memcpy(ptr, &(route.gw), sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if (route.mpls_encap) {
        uint8_t *backup = ptr;

        // add rta_encap attr
        struct rtattr *encap_attr_hdr = (struct rtattr *) ptr;
        ptr += sizeof(struct rtattr);

        encap_attr_hdr->rta_type = RTA_ENCAP;

        {
            uint8_t *nested_attr_start = ptr;

            if (route.mpls_ttl != 255) {
                // nested attr: mpls_ttl
                struct rtattr *nested_mpls_ttl_attr_hdr = (struct rtattr *) ptr;
                ptr += sizeof(struct rtattr);

                nested_mpls_ttl_attr_hdr->rta_type = MPLS_IPTUNNEL_TTL;
                nested_mpls_ttl_attr_hdr->rta_len = RTA_LENGTH(sizeof(uint8_t));
                
                memcpy(ptr, &(route.mpls_ttl), sizeof(uint8_t));
                ptr += sizeof(uint8_t);
            }

            // nested attr: mpls_tunnel_dst
            struct rtattr *nested_mpls_dst_attr_hdr = (struct rtattr *) ptr;
            ptr += sizeof(struct rtattr);

            nested_mpls_dst_attr_hdr->rta_type = MPLS_IPTUNNEL_DST;
            nested_mpls_dst_attr_hdr->rta_len = RTA_LENGTH(sizeof(uint32_t) * route.mpls_stack.size());

            uint32_t *last_lbl = nullptr;

            for (const uint32_t &label : route.mpls_stack) {
                uint32_t lbl_val = htonl(label << 12);
                memcpy(ptr, &lbl_val, sizeof(uint32_t));
                last_lbl = (uint32_t *) ptr;
                ptr += sizeof(uint32_t);
            }

            if (last_lbl == nullptr) {
                log_warn("no stack set but mpls_encap = true. do not do this.\n");
                ptr = backup;
                goto send;
            } else {
                *last_lbl = *last_lbl | htonl(0x100);
            }

            encap_attr_hdr->rta_len = RTA_LENGTH(ptr - nested_attr_start);
        }

        // add rta_encap_type attr
        struct rtattr *encap_type_attr_hdr = (struct rtattr *) ptr;
        ptr += sizeof(struct rtattr);

        encap_type_attr_hdr->rta_type = RTA_ENCAP_TYPE;
        encap_type_attr_hdr->rta_len = RTA_LENGTH(sizeof(short));

        short type = LWTUNNEL_ENCAP_MPLS;

        memcpy(ptr, &(type), sizeof(short));
        ptr += sizeof(short);
        
    }

send:

    size_t msglen = ptr - msg_start;

    msghdr->nlmsg_len = NLMSG_LENGTH(msglen);
    msghdr->nlmsg_pid = _pid;
    msghdr->nlmsg_seq = seq;
    msghdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | (replace ? NLM_F_REPLACE : NLM_F_EXCL);
    msghdr->nlmsg_type = RTM_NEWROUTE;

    if (sendMessage(msghdr) < 0) {
        log_error("sendMessage(): %s\n", strerror(errno));
        return -1;
    }

    return getReply((unsigned int) seq, Netlink::commonAckHandler, (char *) __FUNCTION__);

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

    RtAttr attrs = RtAttr();
    attrs.parse((uint8_t *) IFLA_RTA(iface), RTM_PAYLOAD(src));

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

    RtAttr attrs = RtAttr();
    attrs.parse((uint8_t *) RTM_RTA(rt), RTM_PAYLOAD(src));

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

    RtAttr mpls_info = RtAttr();
    mpls_info.parse((uint8_t *) encap_attr_val, encap_attr_val->rta_len);

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

    RtAttr attrs = RtAttr();
    attrs.parse((uint8_t *) RTM_RTA(rt), RTM_PAYLOAD(src));

    if (!attrs.getAttributeValue(RTA_DST, dst.in_label)) { log_warn("ignored a route w/ no rta_dst.\n"); return PRASE_SKIP; }
    if (!attrs.getAttributeValue(RTA_OIF, dst.oif)) { log_warn("ignored a route w/ no rta_oif.\n"); return PRASE_SKIP; }

    dst.in_label = ntohl(dst.in_label) >> 12;

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

}