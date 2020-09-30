#pragma once

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>
#include <thread>
#include "../SDK/SDK.h"
#include "../interfaces.h"

namespace EnsiBot {

    void Init();
    void CreateMove(CUserCmd* pCmd);
};

