// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "protocol.h"
#include "shared.h"
#include "platform.h"
#include "entity.h"
#include "cubes.h"
#include <stdio.h>

struct Server
{
    uint64_t frame = 0;
    uint64_t tick = 0;
    EntityManager * entity_manager = nullptr;
    CubeManager * cube_manager = nullptr;
};

void server_frame( uint64_t frame, uint64_t tick, double real_time, double frame_time, double jitter )
{
    printf( "%llu: %f [%+.2fms]\n", frame, real_time, jitter * 1000 );
}

void server_add_cube( Server & server, const vec3f & position, float size, int required_index = ENTITY_NULL )
{
    CubeEntity * cube_entity = server.cube_manager->CreateCube( required_index );
    if ( !cube_entity )
    {
        printf( "null cube entity?!\n" );
        return;
    }
    printf( "created cube: %d\n", cube_entity->index );
    cube_entity->position = position;
    cube_entity->size = size;
}

void server_init_world( Server & server )
{
    const int CubeSteps = 30;
    const float PlayerCubeSize = 1.5f;
    const float NonPlayerCubeSize = 0.4f;

    server_add_cube( server, vectorial::vec3f(0,0,10), PlayerCubeSize, ENTITY_PLAYER_BEGIN );

    const float origin = -CubeSteps / 2.0f;
    const float z = NonPlayerCubeSize / 2.0f;
    const int count = CubeSteps;
    for ( int y = 0; y < count; ++y )
        for ( int x = 0; x < count; ++x )
            server_add_cube( server, vec3f(x+origin+0.5f,y+origin+0.5f,z), NonPlayerCubeSize );
}

int server_main( int argc, char ** argv )
{
    printf( "server\n" );

    Server server;

    server.entity_manager = new EntityManager();
    server.cube_manager = new CubeManager( *server.entity_manager );

    server_init_world( server );

    const double start_time = platform_time();

    double previous_frame_time = start_time;
    double next_frame_time = previous_frame_time + ServerFrameDeltaTime;

    for ( int i = 0; i < 10; ++i )
    {
        const double time_to_sleep = max( 0.0, next_frame_time - platform_time() - AverageSleepJitter );

        platform_sleep( time_to_sleep );

        const double real_time = platform_time();

        const double frame_time = next_frame_time;

        const double jitter = real_time - frame_time;

        server_frame( server.frame, server.tick, real_time, frame_time, jitter );

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
            printf( "*** dropped frame %llu *** (%d)\n", server.frame, num_dropped_frames );
        }

        previous_frame_time = next_frame_time - ServerFrameDeltaTime;

        server.tick += TicksPerServerFrame;

        server.frame++;
    }

    delete server.entity_manager;
    delete server.cube_manager;

    return 0;
}

int main( int argc, char ** argv )
{
    return server_main( argc, argv ); 
}
