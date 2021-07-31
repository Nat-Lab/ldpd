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

namespace ldpd {

class Netlink {
public:
    Netlink();
    ~Netlink();

    int open();
    int close();

    int getInterfaces(std::vector<Interface> &to);
    int getRoutes(std::vector<Route> &to);

    static int parseInterface(Interface &dst, const struct nlmsghdr *src);
    static int parseRoute(Route &dst, const struct nlmsghdr *src);

private:
    ssize_t sendQuery(unsigned int seq, unsigned short type, unsigned short flags) const;
    int getReply(unsigned int seq, int (*handler) (void *, const struct nlmsghdr *), void *data) const;

    static int procressInterfaceResults(void *ifaces, const struct nlmsghdr *);

    pid_t _pid;
    int _fd;
    struct sockaddr_nl _local;
    
    unsigned int _seq;
};

extern "C" {

    typedef struct nl_request {
        struct nlmsghdr header;
        struct rtgenmsg message;
    } nl_request_t;

}

}