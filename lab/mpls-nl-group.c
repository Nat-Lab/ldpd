/**
 * The rtnetlink.h don't tell us what group we should use for MPLS routes - it
 * only has macros for IPv4/IPv6/DECnet routes. 
 * 
 * While we sure can use a all-1 (i.e., 0xffffffff) in nl_groups, I wanted to
 * find out what exactily the bit is, so this program.
 */

#include <linux/rtnetlink.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int open_netlink(unsigned int group) {
    struct sockaddr_nl local;

    memset(&local, 0, sizeof(struct sockaddr_nl));

    local.nl_family = AF_NETLINK;
    local.nl_pid = getpid();
    local.nl_groups = group;

    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

    if (fd < 0) {
        printf("socket(): %s\n", strerror(errno));
        return -1;
    }

    if (bind(fd, (struct sockaddr *) &local, sizeof(struct sockaddr_nl))< 0) {
        close(fd);
        printf("bind(): %s\n", strerror(errno));
        return -1;
    }

    return fd;
}

int recv_once(int fd, int retries) {
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

    ssize_t res = -1;

    do {
        res = recvmsg(fd, &rslt_hdr, MSG_DONTWAIT);
        
        if (res < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            break;
        }

        break;
    } while(--retries > 0);

    close(fd);

    if (res < 0) {
        return -1;
    }

    for (struct nlmsghdr *msg = (struct nlmsghdr *) buffer; NLMSG_OK(msg, res); msg = NLMSG_NEXT(msg, res)) {
        if (msg->nlmsg_type == RTM_NEWROUTE) {
            return 1;
        }
    }

    return 0;
}

int main() {
    for(int i = 0; i < 32; ++i) {
        int fd = open_netlink(1 << i);

        if (fd < 0) {
            return 1;
        }

        system("ip -f mpls route replace 114 as 514/1919/810/853 via inet 10.10.10.10 dev ens33");

        if (recv_once(fd, 5) == 1) {
            printf("when bit %d is set, netlink sent us RTM_NEWROUTE when mpls table changes!\n", i);
            continue;
        }
    }

    return 0;
}