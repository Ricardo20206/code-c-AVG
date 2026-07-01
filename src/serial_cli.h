#pragma once

#include "types.h"

class SerialCli {
public:
    void process(const LiveData& live);
};

extern SerialCli g_cli;
