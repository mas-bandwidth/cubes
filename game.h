// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef GAME_H
#define GAME_H

#include "entity.h"
#include "cubes.h"
#include "physics.h"

struct Input
{
    bool left;
    bool right;
    bool up;
    bool down;
    bool push;
    bool pull;

    Input()
    {
        left = false;
        right = false;
        up = false;
        down = false;
        push = false;
        pull = false;
    }

    bool operator == ( const Input & other )
    {
        return left == other.left    &&
              right == other.right   &&
                 up == other.up      &&
               down == other.down    &&
               push == other.push    &&
               pull == other.pull;
    }
    
    bool operator != ( const Input & other )
    {
        return ! ( (*this) == other );
    }
};

inline void ProcessPlayerInput( EntityManager * entity_manager, PhysicsManager * physics_manager, int player_id, const Input & input, uint64_t tick, double t, float dt )
{
    assert( entity_manager );
    assert( physics_manager );

    const int player_entity_index = ENTITY_PLAYER_BEGIN + player_id;

    assert( entity_manager->GetType( player_entity_index ) == ENTITY_TYPE_CUBE );

    CubeEntity * player_cube = (CubeEntity*) entity_manager->GetEntity( player_entity_index );

    assert( player_cube );

    const float strafe_force = 120.0f;

    float force_x = 0.0f;
    float force_y = 0.0f;
    
    force_x -= strafe_force * (int) input.left;
    force_x += strafe_force * (int) input.right;
    force_y += strafe_force * (int) input.up;
    force_y -= strafe_force * (int) input.down;

    vec3f force( force_x, force_y, 0.0f );

    if ( !input.push )
    {
        if ( dot( player_cube->linear_velocity, force ) < 0.0f )
            force *= 2.0f;
    }

    physics_manager->ApplyForce( player_cube->physics_index, force );

    if ( input.push )
    {
        // todo: driving wobble from tick is bad so I changed to t but needs to be retuned.

        // bobbing force on player cube
        {
            const float wobble_x = sin(t*0.1+1) + sin(t*0.05f+3) + sin(t+10);
            const float wobble_y = sin(t*0.1+2) + sin(t*0.05f+4) + sin(t+11);
            const float wobble_z = sin(t*0.1+3) + sin(t*0.05f+5) + sin(t+12);
            vec3f force = vec3f( wobble_x, wobble_y, wobble_z ) * 2.0f;
            physics_manager->ApplyForce( player_cube->physics_index, force );
        }
        
        // bobbing torque on player cube
        {
            const float wobble_x = sin(t*0.05+10) + sin(t*0.05f+22)  + sin(t*1.1f);
            const float wobble_y = sin(t*0.09+5)  + sin(t*0.045f+16) + sin(t*1.11f);
            const float wobble_z = sin(t*0.05+4)  + sin(t*0.055f+9)  + sin(t*1.12f);
            vec3f torque = vec3f( wobble_x, wobble_y, wobble_z ) * 1.5f;
            physics_manager->ApplyTorque( player_cube->physics_index, torque );
        }
        
        /*
        // calculate velocity tilt for player cube
        math::Vector targetUp(0,0,1);
        {
            math::Vector velocity = force[playerId];
            velocity.z = 0.0f;
            float speed = velocity.length();
            if ( speed > 0.001f )
            {
                math::Vector axis = -force[playerId].cross( math::Vector(0,0,1) );
                axis.normalize();
                float tilt = speed * 0.1f;
                if ( tilt > 0.25f )
                    tilt = 0.25f;
                targetUp = math::Quaternion( tilt, axis ).transform( targetUp );
            }
        }
        
        // stay upright torque on player cube
        {
            math::Vector currentUp = activePlayerObject->orientation.transform( math::Vector(0,0,1) );
            math::Vector axis = targetUp.cross( currentUp );
            float angle = math::acos( targetUp.dot( currentUp ) );
            if ( angle > 0.5f )
                angle = 0.5f;
            math::Vector torque = - 100 * axis * angle;
            simulation->ApplyTorque( activePlayerObject->activeId, torque );
        }

        // todo: this linear damping is not framerate independent
        
        // apply damping to player cube
        activePlayerObject->angularVelocity *= 0.95f;
        activePlayerObject->linearVelocity.x *= 0.96f;
        activePlayerObject->linearVelocity.y *= 0.96f;
        activePlayerObject->linearVelocity.z *= 0.999f;
        */
    }
    
    vec3f push_origin = player_cube->position;
    push_origin += vec3f( 0, 0, -0.1f );

    if ( input.push || input.pull )
    {
        /*
        for ( int i = 0; i < activeObjects.GetCount(); ++i )
        {
            ActiveObject * activeObject = &activeObjects.GetObject( i );

            const int authority = activeObject->authority;
            const float mass = simulation->GetObjectMass( activeObject->activeId );

            if ( push )
            {
                math::Vector difference = activeObject->position - push_origin;
                if ( activeObject == activePlayerObject )
                    difference.z -= 0.7f;
                float distanceSquared = difference.lengthSquared();
                if ( distanceSquared > 0.01f * 0.01f && distanceSquared < 4.0f )
                {
                    float distance = math::sqrt( distanceSquared );
                    math::Vector direction = difference / distance;
                    float magnitude = 1.0f / distanceSquared * 200.0f;
                    if ( magnitude > 500.0f )
                        magnitude = 500.0f;
                    math::Vector force = direction * magnitude;
                    if ( activeObject != activePlayerObject )
                        force *= mass;
                    else if ( pull )
                        force *= 20;

                    if ( authority == MaxPlayers || playerId == authority )
                    {
                        simulation->ApplyForce( activeObject->activeId, force );
                        activeObject->authority = playerId;
                        activeObject->authorityTime = 0;
                    }
                }
            }
            else if ( pull )
            {
                if ( activeObject != activePlayerObject )
                {
                    math::Vector difference = activeObject->position - origin;
                    float distanceSquared = difference.lengthSquared();
                    const float effectiveRadiusSquared = 1.8f * 1.8f;
                    if ( distanceSquared > 0.2f*0.2f && distanceSquared < effectiveRadiusSquared )
                    {
                        float distance = math::sqrt( distanceSquared );
                        math::Vector direction = difference / distance;
                        float magnitude = 1.0f / distanceSquared * 200.0f;
                        if ( magnitude > 2000.0f )
                            magnitude = 2000.0f;
                        math::Vector force = - direction * magnitude;
                        if ( authority == playerId || authority == MaxPlayers )
                        {
                            simulation->ApplyForce( activeObject->activeId, force * mass );
                            activeObject->authority = playerId;
                            activeObject->authorityTime = 0;
                        }
                    }
                }
            }
        }
        */
    }
}

#endif // #ifndef GAME_H
