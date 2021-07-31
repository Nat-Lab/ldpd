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

#define PARSE_OK 0
#define PARSE_SKIP 1

#define PROCESS_NEXT 0
#define PROCESS_END 1
#define PROCESS_ERR 2

namespace ldpd {

extern "C" {

    typedef struct nl_request {
        struct nlmsghdr header;
        struct rtgenmsg message;
    } nl_request_t;

}

class Netlink {
public:
    Netlink();
    ~Netlink();

    int open();
    int close();

    int getInterfaces(std::vector<Interface> &to);
    int getIpv4Routes(std::vector<Ipv4Route> &to);

    static int parseInterface(Interface &dst, const struct nlmsghdr *src);
    static int parseIpv4Route(Ipv4Route &dst, const struct nlmsghdr *src);

private:
    ssize_t sendQuery(unsigned int seq, unsigned short type, unsigned short flags) const;
    int getReply(unsigned int seq, int (*handler) (void *, const struct nlmsghdr *), void *data) const;

    static int procressInterfaceResults(void *ifaces, const struct nlmsghdr *);
    static int procressIpv4RouteResults(void *routes, const struct nlmsghdr *);

    pid_t _pid;
    int _fd;
    struct sockaddr_nl _local;
    std::vector<struct nlmsghdr *> _saved;
    
    unsigned int _seq;
};

}