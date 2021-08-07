#include "utils/inet-ntop.hh"

namespace ldpd {

InetNtop::InetNtop(uint32_t address) {
    inet_ntop(AF_INET, &address, str, INET_ADDRSTRLEN);
}

}