#include "utils/log.hh"
#include "abstraction/interface.hh"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <vector>

namespace ldpd {

class Netlink {
public:
    Netlink();
    ~Netlink();

    int open();
    int close();

    int getInterfaces(std::vector<Interface> &to);

    static int parseInterface(Interface &dst, const struct nlmsghdr *src);

private:
    ssize_t sendRequest(unsigned int seq, unsigned short type, unsigned short flags) const;
    ssize_t getReply(uint8_t *buffer, size_t buf_sz) const;

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