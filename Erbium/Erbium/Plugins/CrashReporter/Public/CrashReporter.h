#pragma once
#include "../../../../pch.h"

class FCrashReporter
{
public:
    static inline thread_local bool bSEHGuard = false;

    static void Register();
};