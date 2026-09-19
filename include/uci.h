#pragma once
#include <memory>
#include <string>

// Shared line-oriented protocol. The browser serializes mutating commands;
// stop and isready may be delivered while Asyncify has suspended a search.
class UciSession {
public:
    UciSession();
    ~UciSession();
    bool command(const std::string& line);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
int run_uci();
