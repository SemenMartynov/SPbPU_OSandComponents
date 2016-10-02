#include <iostream>
#include "parse.h"

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
