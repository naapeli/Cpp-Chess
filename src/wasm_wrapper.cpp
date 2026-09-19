#include "uci.h"
#include <emscripten.h>

extern "C" {
// One engine instance per Web Worker. Input/output are ordinary UCI lines.
EMSCRIPTEN_KEEPALIVE void uci_command(const char* line) {
    static UciSession session;
    if (line) session.command(line);
}
}
