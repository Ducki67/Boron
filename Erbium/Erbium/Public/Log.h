#pragma once
#include <cstdio>
#include "Configuration.h"

#ifdef BORON_LOGS
#define BORON_LOG(...) printf(__VA_ARGS__)
#else
#define BORON_LOG(...) ((void)0)
#endif
#define BORON_LOG_ON(...) printf(__VA_ARGS__)
