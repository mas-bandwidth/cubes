#ifndef WORLD_H
#define WORLD_H

#include "platform.h"
#include "entity.h"
#include "cubes.h"
#include <stdio.h>

struct World
{
    uint64_t frame = 0;                                 // current frame (server or client frame depending on context)
    uint64_t tick = 0;                                  // current tick (ticks @ 240HZ)
    double time = 0.0;                                  // current time in seconds
    EntityManager * entity_manager = nullptr;           // indexes entities in this world
    PhysicsManager * physics_manager = nullptr;         // the physics simulation
    CubeManager * cube_manager = nullptr;               // manager for cube entities
};

inline void world_init( World & world )
{
    world.entity_manager = new EntityManager();
    world.physics_manager = new PhysicsManager();
    world.cube_manager = new CubeManager( world.entity_manager, world.physics_manager );
    world.physics_manager->Initialize();
}

inline void world_add_cube( World & world, const vec3f & position, float scale, bool active, int required_index = ENTITY_NULL )
{
    world.cube_manager->CreateCube( position, scale, active, required_index );
}

inline void world_setup_cubes( World & world )
{
    const float PlayerCubeSize = 1.5f;
    const float NonPlayerCubeSize = 0.4f;

    world.physics_manager->AddPlane( vec3f(0,0,1), 0 );

    world_add_cube( world, vectorial::vec3f(0,0,10), PlayerCubeSize, true, ENTITY_PLAYER_BEGIN );

    const int grid_size = 30;
    const float origin = -grid_size / 2.0f + 0.5f;
    const float z = NonPlayerCubeSize / 2.0f;
    
    for ( int y = 0; y < grid_size; ++y )
    {
        for ( int x = 0; x < grid_size; ++x )
        {
            world_add_cube( world, vec3f( x + origin, y + origin, z ), NonPlayerCubeSize, false );
        }
    }
}

inline void world_tick( World & world )
{
    world.cube_manager->UpdateAuthority( world.time, TickDeltaTime );

    world.cube_manager->PrePhysicsUpdate();

    world.physics_manager->Update( world.tick, world.time, TickDeltaTime );

    world.cube_manager->PostPhysicsUpdate();

    world.entity_manager->UpdateAuthority( world.time, TickDeltaTime );

    world.time += TickDeltaTime;
    world.tick++;
}

inline void world_free( World & world )
{
    delete world.entity_manager;
    delete world.physics_manager;
    delete world.cube_manager;
    world = World();
}


#endif // #ifndef WORLD_H
