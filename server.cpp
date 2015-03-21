// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "protocol.h"
#include "shared.h"
#include "platform.h"
#include <stdio.h>

void server_frame( uint64_t frame, uint64_t tick, double real_time, double frame_time, double jitter )
{
    printf( "%llu: %f [%f]\n", frame, real_time, jitter );
}

int server_main( int argc, char ** argv )
{
    printf( "server\n" );

    uint64_t frame = 0;
    uint64_t tick = 0;

    const double start_time = platform_time();

    double previous_frame_time = start_time;
    double next_frame_time = previous_frame_time + ServerFrameDeltaTime;

    double average_jitter = 0.0;

    for ( int i = 0; i < 100; ++i )
    {
        const double time_to_sleep = max( 0.0, next_frame_time - platform_time() - average_jitter / 2 );

        platform_sleep( time_to_sleep );

        double real_time = platform_time();

        double frame_time = next_frame_time;

        double jitter = real_time - frame_time - average_jitter / 2;

        average_jitter += ( jitter - average_jitter ) * 0.9;

        server_frame( frame, tick, real_time, frame_time, jitter );

        const double end_of_frame_time = platform_time();

        int num_frames_advanced = 0;
        while ( next_frame_time < end_of_frame_time + ServerFrameSafety * ServerFrameDeltaTime )
        {
            next_frame_time += ServerFrameDeltaTime;
            num_frames_advanced++;
        }

        const int num_dropped_frames = max( 0, num_frames_advanced - 1 );

        if ( num_dropped_frames > 0 )
        {
            printf( "*** dropped frame %llu *** (%d)\n", frame, num_dropped_frames );
        }

        previous_frame_time = next_frame_time - ServerFrameDeltaTime;

        tick += TicksPerServerFrame;

        frame++;
    }

    return 0;
}

int main( int argc, char ** argv )
{
    return server_main( argc, argv ); 
}
