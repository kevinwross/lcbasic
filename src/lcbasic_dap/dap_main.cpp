#include "dap_server.h"

#include <iostream>

// lcbasic-dap: a Debug Adapter Protocol server. Reads DAP messages from
// stdin and writes responses/events to stdout. Configure your editor to
// launch this binary for GW-BASIC debugging.
int main() {
  dap::DapServer server(std::cin, std::cout);
  server.run();
  return 0;
}
