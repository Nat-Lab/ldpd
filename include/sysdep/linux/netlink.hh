#ifndef LDP_NETLINK_H
#define LDP_NETLINK_H
#include "utils/log.hh"
#include "sysdep/linux/rtattr.hh"
#include "abstraction/interface.hh"
#include "abstraction/route.hh"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/lwtunnel.h>

#include <vector>
#include <map>
#include <type_traits>

#define PARSE_OK 0
#define PARSE_SKIP 1

#define PROCESS_NEXT 0
#define PROCESS_END 1
#define PROCESS_ERR 2

namespace ldpd {

enum NetlinkChange {
    Added = 0, Deleted = 1
};

typedef void (*linkchange_handler_t)(void *data, NetlinkChange change, const Interface &iface);
typedef void (*addrchange_handler_t)(void *data, NetlinkChange change, const InterfaceAddress &addr);
typedef void (*ipv4_routechange_handler_t)(void *data, NetlinkChange change, const Ipv4Route &route);
typedef void (*mpls_routechange_handler_t)(void *data, NetlinkChange change, const MplsRoute &route);

class Netlink {
public:
    Netlink();
    ~Netlink();

    int open();
    int close();

    int getInterfaces(std::vector<Interface> &to);

    int getRoutes(std::vector<Ipv4Route> &to);
    int getRoutes(std::vector<MplsRoute> &to);

    template <typename T> int addRoute(const T &route, bool replace = false) {
        return sendRouteMessage(
            (const Route *) &route,
            RTM_NEWROUTE,
            NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | (replace ? NLM_F_REPLACE : NLM_F_EXCL));
    }

    template <typename T> int deleteRoute(const T &route) {
        return sendRouteMessage(
            (const Route *) &route,
            RTM_DELROUTE,
            NLM_F_REQUEST | NLM_F_ACK);
    }

    // note: the Interface object passed in WILL NOT have addresses filled.
    void onLinkChanges(linkchange_handler_t handler, void *data);
    
    void onAddrChanges(addrchange_handler_t handler, void *data);
    void onIpv4RouteChanges(ipv4_routechange_handler_t handler, void *data);
    void onMplsRouteChanges(mpls_routechange_handler_t handler, void *data);

    void tick();

private:
    int sendGeneralQuery(unsigned char af, unsigned short type, unsigned short flags);
    int sendRouteMessage(const Route *route, unsigned short type, unsigned short flags);

    int getReply(unsigned int seq, int (*handler) (void *, const struct nlmsghdr *), void *data);

    void handleChanges(const struct nlmsghdr *msg) const;

    ssize_t sendMessage(const void *msg);

    static int parseNetlinkMessage(Interface &dst, const struct nlmsghdr *src);
    static int parseNetlinkMessage(InterfaceAddress &dst, const struct nlmsghdr *src);
    static int parseNetlinkMessage(Ipv4Route &dst, const struct nlmsghdr *src);
    static int parseNetlinkMessage(MplsRoute &dst, const struct nlmsghdr *src);

    static int procressInterfaceResults(void *ifaces, const struct nlmsghdr *);
    static int procressIpv4RouteResults(void *routes, const struct nlmsghdr *);
    static int procressMplsRouteResults(void *routes, const struct nlmsghdr *);
    static int commonAckHandler(void *unused, const struct nlmsghdr *msg);

    static int buildRtAttr(const Ipv4Route &route, RtAttr &attrs);
    static int buildRtAttr(const MplsRoute &route, RtAttr &attrs);

    pid_t _pid;
    int _fd;
    
    struct sockaddr_nl _local;
    struct sockaddr_nl _kernel;

    std::vector<struct nlmsghdr *> _saved;

    struct iovec _io;
    void *_buffer;
    size_t _bufsz;
    
    unsigned int _seq;

    linkchange_handler_t _lc_handler;
    addrchange_handler_t _ac_handler;
    ipv4_routechange_handler_t _irc_handler;
    mpls_routechange_handler_t _mrc_handler;

    void *_lc_handler_d, *_ac_handler_d, *_irc_handler_d, *_mrc_handler_d;
};

}

#endif // LDP_NETLINK_H