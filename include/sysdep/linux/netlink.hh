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

class RouteAttributes {
public:
    RouteAttributes(const struct rtattr *attr, size_t len);
    ~RouteAttributes();

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

private:
    std::map<unsigned short, struct rtattr*> _attrs;
};

class Netlink {
public:
    Netlink();
    ~Netlink();

    int open();
    int close();

    int getInterfaces(std::vector<Interface> &to);
    int getIpv4Routes(std::vector<Ipv4Route> &to);
    int getMplsRoutes(std::vector<MplsRoute> &to);

    int addIpv4Route(const Ipv4Route &route);
    int addMplsRoute(const MplsRoute &route);

    int deleteIpv4Route(const Ipv4Route &route);
    int deleteMplsRoute(const Ipv4Route &route);

    static int parseInterface(Interface &dst, const struct nlmsghdr *src);
    static int parseIpv4Route(Ipv4Route &dst, const struct nlmsghdr *src);
    static int parseMplsRoute(MplsRoute &dst, const struct nlmsghdr *src);

private:
    int sendGeneralQuery(unsigned char af, unsigned short type, unsigned short flags);
    ssize_t sendMessage(const void *msg);
    int getReply(unsigned int seq, int (*handler) (void *, const struct nlmsghdr *), void *data);

    static int procressInterfaceResults(void *ifaces, const struct nlmsghdr *);
    static int procressIpv4RouteResults(void *routes, const struct nlmsghdr *);
    static int procressMplsRouteResults(void *routes, const struct nlmsghdr *);

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