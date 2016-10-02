#include <iostream>
#include <fstream>
#include <string>
#include <regex>

bool parse(char* ifname, long long* rx_bytes, long long* rx_packets,
           long long* tx_bytes, long long* tx_packets);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " interface_name" << std::endl;
        return 1;
    }

    long long rx_bytes = 0;
    long long rx_packets = 0;
    long long tx_bytes = 0;
    long long tx_packets = 0;

    if (!parse(argv[1], &rx_bytes, &rx_packets, &tx_bytes, &tx_packets)) {
        std::cerr << "Can't find such interface: " << argv[1] << std::endl;
        return 1;
    }

    std::cout << argv[1] << ":" << std::endl;
    std::cout << "\tReceive " << rx_bytes << " bytes (" << rx_packets << " packets)" << std::endl;
    std::cout << "\tTransmit " << tx_bytes << " bytes (" << tx_packets << " packets)" << std::endl;

    return 0;
}

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
