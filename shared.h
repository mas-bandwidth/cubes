// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef SHARED_H
#define SHARED_H

#include "const.h"

struct TimeBase
{
    double time = 0.0;                // frame time. 0.0 is start of process
    double deltaTime = 0.0;           // delta time this frame in seconds.
};

#endif // #ifndef SHARED_H
