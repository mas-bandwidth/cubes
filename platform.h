// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef PLATFORM_H
#define PLATFORM_H

#include <assert.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <unistd.h>

inline void platform_sleep( double time )
{
    usleep( (int) ( time * 1000000 ) );
}

inline double platform_time()
{
    static uint64_t start = 0;
    static mach_timebase_info_data_t timebase_info;

    if ( start == 0 )
    {
        mach_timebase_info( &timebase_info );
        start = mach_absolute_time();
        return 0.0;
    }

    uint64_t current = mach_absolute_time();

    assert( current > start );

    return ( double( current - start ) * double( timebase_info.numer ) / double( timebase_info.denom ) ) / 1000000000.0;
}

#endif // #ifndef SHARED_H
