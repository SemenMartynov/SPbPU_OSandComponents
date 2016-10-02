#include <iostream>
#include <fstream>
#include <string>
#include <regex>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <syslog.h>
#include <signal.h>
#include <stdarg.h>

bool parse(char* ifname, long long* rx_bytes, long long* rx_packets,
           long long* tx_bytes, long long* tx_packets);

int main(int argc, char* argv[]) {
  openlog("netmonitor", LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_DAEMON);
  if (argc < 2) {
    syslog(LOG_ERR, "Usage: %s interface_name", argv[0]);
    return 1;
  }

  int pid = fork();
  switch (pid) {
    case 0:
      umask(0);
      setsid();
      chdir("/");
      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);

      {
        long long rx_bytes = 0;
        long long rx_packets = 0;
        long long tx_bytes = 0;
        long long tx_packets = 0;

        while (true) {
          if (!parse(argv[1], &rx_bytes, &rx_packets, &tx_bytes, &tx_packets)) {
            syslog(LOG_ERR, "Can't find such interface: %s", argv[1]);
            return 1;
          }

          syslog(LOG_INFO,
                   "%s:\n\tReceive %lld  bytes (%lld packets)\n\tTransmit %lld "
                   " bytes (%lld packets)",
                   argv[1], rx_bytes, rx_packets, tx_bytes, tx_packets);

          usleep(2000);
        }
      }

    case -1:
      syslog(LOG_ERR, "Fail: unable to fork");
      closelog();
      return 1;
    default:
      syslog(LOG_NOTICE, "OK: demon with pid %d is created\n", pid);
      break;
  }

  closelog();
  return 0;
}

bool parse(char* ifname, long long* rx_bytes, long long* rx_packets,
           long long* tx_bytes, long long* tx_packets) {
  std::string interface(ifname);
  interface.append(":");
  std::string buff;
  std::ifstream netstat("/proc/net/dev");

  while (std::getline(netstat, buff)) {
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