#include "usbguard.hpp"
#include "usbguard-list-devices.hpp"

#include <IPCClient.hpp>
#include <iostream>

namespace usbguard
{
  static const char *options_short = "hab";

  static const struct ::option options_long[] = {
    { "help", no_argument, nullptr, 'h' },
    { "blocked", no_argument, nullptr, 'b' },
    { "allowed", no_argument, nullptr, 'a' },
    { nullptr, 0, nullptr, 0 }
  };

  static void showHelp(std::ostream& stream)
  {
    stream << " Usage: " << usbguard_arg0 << " list-devices [OPTIONS]" << std::endl;
    stream << std::endl;
    stream << " Options:" << std::endl;
    stream << "  -a, --allowed  List allowed devices." << std::endl;
    stream << "  -b, --blocked  List blocked devices." << std::endl;
    stream << "  -h, --help     Show this help." << std::endl;
    stream << std::endl;
  }

  int usbguard_list_devices(int argc, char *argv[])
  {
    bool list_blocked = false;
    bool list_allowed = false;
    int opt = 0;

    while ((opt = getopt_long(argc, argv, options_short, options_long, nullptr)) != -1) {
      switch(opt) {
        case 'h':
          showHelp(std::cout);
          return EXIT_SUCCESS;
        case 'a':
          list_allowed = true;
          break;
        case 'b':
          list_blocked = true;
          break;
        case '?':
          showHelp(std::cerr);
        default:
          return EXIT_FAILURE;
      }
    }

    const bool list_everything = (list_blocked == list_allowed);
    std::string query = "match";

    if (!list_everything) {
      if (list_allowed) {
        query = "allow";
      }
      else {
        query = "block";
      }
    }

    usbguard::IPCClient ipc(/*connected=*/true);

    for (auto device_rule : ipc.listDevices(query)) {
      std::cout << device_rule.getID() << ": " << device_rule.toString() << std::endl;
    }

    return EXIT_SUCCESS;
  }
} /* namespace usbguard */
