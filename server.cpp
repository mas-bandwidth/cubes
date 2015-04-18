// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "protocol.h"
#include "shared.h"
#include "world.h"
#include "game.h"
#include <stdio.h>

void server_tick( World & world )
{
    printf( "%llu-%llu: %f [%+.4f]\n", world.frame, world.tick, world.time, TickDeltaTime );

    world_tick( world );
}

void server_frame( World & world, double real_time, double frame_time, double jitter )
{
    //printf( "%llu: %f [%+.2fms]\n", frame, real_time, jitter * 1000 );
    
    for ( int i = 0; i < TicksPerServerFrame; ++i )
    {
        server_tick( world );
    }
}

int server_main( int argc, char ** argv )
{
    printf( "server\n" );

    World world;
    world_init( world );
    world_setup_cubes( world );

    const double start_time = platform_time();

    double previous_frame_time = start_time;
    double next_frame_time = previous_frame_time + ServerFrameDeltaTime;

    while ( true )
    {
        const double time_to_sleep = max( 0.0, next_frame_time - platform_time() - AverageSleepJitter );

        platform_sleep( time_to_sleep );

        const double real_time = platform_time();

        const double frame_time = next_frame_time;

        const double jitter = real_time - frame_time;

        server_frame( world, real_time, frame_time, jitter );

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
            printf( "*** dropped frame %llu *** (%d)\n", world.frame, num_dropped_frames );
        }

        previous_frame_time = next_frame_time - ServerFrameDeltaTime;

        world.frame++;
    }

    world_free( world );

    return 0;
}

int main( int argc, char ** argv )
{
    return server_main( argc, argv ); 
}
