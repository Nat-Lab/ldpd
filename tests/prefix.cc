#include "abstraction/prefix.hh"
#include <arpa/inet.h>

int main() {
    ldpd::Prefix net;

    net.prefix = inet_addr("10.20.30.0");
    net.len = 24;

    ldpd::Prefix addr;

    addr.prefix = inet_addr("10.20.30.1");
    addr.len = 24;

    return 0;
}