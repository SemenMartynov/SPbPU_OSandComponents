#include "parse.h"

#include <fstream>
#include <string>
#include <regex>

bool parse(char* ifname, long long* rx_bytes, long long* rx_packets,
           long long* tx_bytes, long long* tx_packets) {
    std::string interface(ifname);
    interface.append(":");
    std::string buff;
    std::ifstream netstat("/proc/net/dev");

    while(std::getline(netstat, buff)) {
        size_t shift = buff.find_first_not_of(' ');
        if (buff.compare(shift, interface.length(), interface) == 0) {
            std::regex rx(R"([^[:alpha:]][[:digit:]]+[^[:alpha:]])");
            std::sregex_iterator pos(buff.cbegin(), buff.cend(), rx);

            *rx_bytes = std::stoll(pos->str());
            ++pos;
            *rx_packets = std::stoll(pos->str());
            std::advance(pos, 7);
            *tx_bytes = std::stoll(pos->str());
            ++pos;
            *tx_packets = std::stoll(pos->str());

            return true;
        }
    }
    return false;
}
