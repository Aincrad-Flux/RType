#include <raylib.h>
#include <string>
#include <vector>
#include <cstring>
#include "../include/client/ui/App.hpp"

static void parseArgs(int argc, char** argv, std::string& host, std::string& port, std::string& name) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto eq = [](const char* x, const char* y){ return std::strcmp(x,y) == 0; };
        if (eq(a, "-h") || eq(a, "--host")) {
            if (i + 1 < argc) host = argv[++i];
        } else if (eq(a, "-p") || eq(a, "--port")) {
            if (i + 1 < argc) port = argv[++i];
        } else if (eq(a, "-n") || eq(a, "--name")) {
            if (i + 1 < argc) name = argv[++i];
        }
    }
}

int main(int argc, char** argv) {
    std::string host, port, name;
    parseArgs(argc, argv, host, port, name);
    client::ui::App app;
    if (!host.empty() && !port.empty()) {
        if (name.empty()) name = "Player"; // default name if not provided
        app.setAutoConnect(host, port, name);
    }
    app.run();
    return 0;
}