// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef ENTITY_H
#define ENTITY_H

#include "const.h"
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "vectorial/vec3f.h"
#include "vectorial/vec4f.h"
#include "vectorial/quat4f.h"
#include "vectorial/mat4f.h"

using namespace vectorial;

enum EntityFlags
{
    ENTITY_FLAG_ALLOCATED = 1
};

enum EntityType
{
    ENTITY_TYPE_WORLD = 0,
    ENTITY_TYPE_CUBE = 1
};

struct Entity
{
    Entity()
    {
        owner = 0;
        entity_index = ENTITY_WORLD;
        physics_index = PHYSICS_NULL;
    }

    int owner;
    int physics_index;
    int entity_index;
    vec3f position;
    quat4f orientation;
    vec3f linear_velocity;
    vec3f angular_velocity;
};

struct EntityManager
{
    int num_entities = 0;
    int entity_alloc_index = ENTITY_PLAYER_END;
    uint8_t flags[MaxEntities];
    uint8_t types[MaxEntities];
    uint8_t sequence[MaxEntities];
    Entity * entities[MaxEntities];

    EntityManager()
    {
        memset( flags, 0, sizeof( flags ) );
        memset( types, 0, sizeof( types ) );
        memset( sequence, 0, sizeof( sequence ) );
        memset( entities, 0, sizeof( entities ) );
    }

    int Allocate( EntityType type, int required_index = ENTITY_NULL )
    {
        assert( num_entities >= 0 );
        assert( num_entities <= MaxEntities );
        if ( num_entities == MaxEntities )
            return ENTITY_NULL;
        if ( required_index != ENTITY_NULL )
        {
            if ( ( flags[required_index] & ENTITY_FLAG_ALLOCATED ) == 0 )
            {
                num_entities++;
                types[required_index] = type;
                return required_index;
            }
            else
                return ENTITY_NULL;
        }
        int allocated_index = ENTITY_NULL;
        for ( int i = 0; i < MaxEntities; ++i )
        {
            allocated_index = entity_alloc_index++;
            if ( ( flags[allocated_index] & ENTITY_FLAG_ALLOCATED ) == 0 )
                break;
        }
        num_entities++;
        assert( allocated_index != ENTITY_NULL );
        assert( num_entities <= MaxEntities );
        return allocated_index;
    }

    void Free( int index )
    {
        assert( index >= 0 );
        assert( index <= MaxEntities );
        assert( flags[index] & ENTITY_FLAG_ALLOCATED );
        assert( num_entities > 0 );
        flags[index] = 0;
        types[index] = ENTITY_TYPE_WORLD;
        entities[index] = nullptr;
        sequence[index]++;
    }

    EntityType GetType( int index ) const
    {
        assert( index >= 0 );
        assert( index < MaxEntities );
        return (EntityType) types[index];
    }

    uint8_t & GetFlag( int index )
    {
        assert( index >= 0 );
        assert( index < MaxEntities );
        return flags[index];
    }

    void SetEntity( int index, Entity * entity, EntityType type )
    {
        assert( index >= 0 );
        assert( index < MaxEntities );
        // todo: set entity at index. make sure allocated etc.
    }

    Entity * GetEntity( int index )
    {
        assert( index >= 0 );
        assert( index < MaxEntities );
        return entities[index];
    }
};

#endif // #ifndef ENTITY_H
