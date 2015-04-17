// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef CUBES_H
#define CUBES_H

#include "entity.h"
#include "physics.h"
#include "shared.h"

struct CubeEntity : public Entity
{
    float scale;
    CubeEntity() : scale( 1.0f ) {}
};

struct CubeManager
{
    EntityManager * entity_manager;
    PhysicsManager * physics_manager;
    bool allocated[MaxCubes];
    int entity_index_to_cube_index[MaxCubes];
    CubeEntity cubes[MaxCubes];

    CubeManager( EntityManager * entity_manager, PhysicsManager * physics_manager )
    {
        assert( entity_manager );
        assert( physics_manager );
        this->entity_manager = entity_manager;
        this->physics_manager = physics_manager;
        memset( allocated, 0, sizeof( allocated ) );
        for ( int i = 0; i < MaxCubes; ++i )
            entity_index_to_cube_index[i] = -1;
    }

    CubeEntity * CreateCube( const vec3f & position, float scale, bool active, int required_entity_index = ENTITY_NULL )
    {
        const int entity_index = entity_manager->Allocate( required_entity_index );

        if ( entity_index == ENTITY_NULL )
            return nullptr;
        
        for ( int i = 0; i < MaxCubes; ++i )
        {
            if ( !allocated[i] )
            {
                allocated[i] = true;
                entity_index_to_cube_index[entity_index] = i;
                cubes[i].entity_index = entity_index;

                PhysicsObjectState initial_state;
                initial_state.active = active;
                initial_state.position = position;

                cubes[i].physics_index = physics_manager->AddObject( entity_index, initial_state, PHYSICS_SHAPE_CUBE, scale );
                cubes[i].position = position;
                cubes[i].scale = scale;
                
                entity_manager->SetEntity( entity_index, &cubes[i], ENTITY_TYPE_CUBE );

                return &cubes[i];
            }
        }

        return nullptr;
    }

    void DestroyCube( CubeEntity * cube_entity )
    {
        assert( cube_entity );
        const int entity_index = cube_entity->entity_index;
        assert( entity_index >= 0 );
        assert( entity_index < MaxEntities );
        const int cube_index = entity_index_to_cube_index[entity_index];
        assert( cube_index >= 0 );
        assert( cube_index < MaxCubes );
        allocated[cube_index] = false;
        entity_index_to_cube_index[entity_index] = -1;
        cubes[cube_index] = CubeEntity();
        entity_manager->Free( entity_index );
    }

    void PrePhysicsUpdate()
    {
        PhysicsObjectState object_state;

        for ( int i = 0; i < MaxCubes; ++i )
        {
            if ( !allocated[i] )
                continue;

            object_state.active = physics_manager->IsActive( cubes[i].physics_index );
            object_state.position = cubes[i].position;
            object_state.orientation = cubes[i].orientation;
            object_state.linear_velocity = cubes[i].linear_velocity;
            object_state.angular_velocity = cubes[i].angular_velocity;

            physics_manager->SetObjectState( cubes[i].physics_index, object_state );
        }
    }

    void PostPhysicsUpdate()
    {
        PhysicsObjectState object_state;

        for ( int i = 0; i < MaxCubes; ++i )
        {
            if ( !allocated[i] )
                continue;

            physics_manager->GetObjectState( cubes[i].physics_index, object_state );

            cubes[i].position = object_state.position;
            cubes[i].orientation = object_state.orientation;
            cubes[i].linear_velocity = object_state.linear_velocity;
            cubes[i].angular_velocity = object_state.angular_velocity;
        }
    }

    void UpdateAuthority( double t, float dt )
    {
        for ( int i = 0; i < MaxCubes; ++i )
        {
            if ( !allocated[i] )
                continue;

            const int authority = entity_manager->GetAuthority( cubes[i].entity_index );
            if ( authority == 0 )
                continue;

            bool active = physics_manager->IsActive( cubes[i].physics_index );
            if ( active )
                continue;

            float & authority_time = entity_manager->authority_time[cubes[i].entity_index];

            authority_time += dt;

            if ( authority_time > AuthorityThreshold )
                entity_manager->SetAuthority( cubes[i].entity_index, 0 );
        }
    }
};

#endif // #ifndef CUBES_H
