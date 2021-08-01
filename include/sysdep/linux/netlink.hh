#ifndef LDP_NETLINK_H
#define LDP_NETLINK_H
#include "utils/log.hh"
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

#define PRASE_OK 0
#define PRASE_SKIP 1

#define PROCESS_NEXT 0
#define PROCESS_END 1
#define PROCESS_ERR 2

namespace ldpd {

class Netlink {
public:
    Netlink();
    ~Netlink();

    int open();
    int close();

    int getInterfaces(std::vector<Interface> &to);
    int getIpv4Routes(std::vector<Ipv4Route> &to);
    int getMplsRoutes(std::vector<MplsRoute> &to);

    int addIpv4Route(const Ipv4Route &route, bool replace = false);
    int addMplsRoute(const MplsRoute &route, bool replace = false);

    int deleteIpv4Route(const Ipv4Route &route);
    int deleteMplsRoute(const Ipv4Route &route);

private:
    int sendGeneralQuery(unsigned char af, unsigned short type, unsigned short flags);
    int getReply(unsigned int seq, int (*handler) (void *, const struct nlmsghdr *), void *data);

    ssize_t sendMessage(const void *msg);

    static int parseInterface(Interface &dst, const struct nlmsghdr *src);
    static int parseIpv4Route(Ipv4Route &dst, const struct nlmsghdr *src);
    static int parseMplsRoute(MplsRoute &dst, const struct nlmsghdr *src);

    static int procressInterfaceResults(void *ifaces, const struct nlmsghdr *);
    static int procressIpv4RouteResults(void *routes, const struct nlmsghdr *);
    static int procressMplsRouteResults(void *routes, const struct nlmsghdr *);
    static int commonAckHandler(void *unused, const struct nlmsghdr *msg);

    pid_t _pid;
    int _fd;
    
    struct sockaddr_nl _local;
    struct sockaddr_nl _kernel;

    std::vector<struct nlmsghdr *> _saved;

    struct iovec _io;
    void *_buffer;
    size_t _bufsz;
    
    unsigned int _seq;
};

}

#endif // LDP_NETLINK_H