// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "physics.h"
#include "assert.h"
#define dSINGLE
#include <ode/ode.h>
#include "entity.h"

const int MaxContacts = 32;

struct PhysicsConfig
{
	float ERP;
	float CFM;
 	int MaxIterations;
	float Gravity;
	float LinearDamping;
	float AngularDamping;
	float Friction;
	float Elasticity;
	float ContactSurfaceLayer;
	float MaximumCorrectingVelocity;
	bool QuickStep;
	float RestTime;
	float LinearRestThresholdSquared;
	float AngularRestThresholdSquared;

	PhysicsConfig()
	{
		ERP = 0.5f;
		CFM = 0.015f;
		MaxIterations = 64;
		MaximumCorrectingVelocity = 2.5f;
		ContactSurfaceLayer = 0.01f;
		Elasticity = 0.0f;
		LinearDamping = 0.001f;
		AngularDamping = 0.001f;
		Friction = 200.0f;
		Gravity = 20.0f;
		QuickStep = true;
		RestTime = 0.1;
		LinearRestThresholdSquared = 0.1f * 0.1f;
		AngularRestThresholdSquared = 0.1f * 0.1f;
	}  
};

struct PhysicsInternal
{
 	PhysicsInternal()
	{
		world = 0;
		space = 0;
		contacts = 0;
	}
	
	~PhysicsInternal()
	{
		if ( contacts )
			dJointGroupDestroy( contacts );
		if ( world )
			dWorldDestroy( world );
		if ( space )
			dSpaceDestroy( space );
			
		contacts = 0;
		world = 0;
		space = 0;
	}
	
	dWorldID world;
	dSpaceID space;
	dJointGroupID contacts;

	struct ObjectData
	{
		dBodyID body;
		dGeomID geom;
		float scale;
		float timeAtRest;

		ObjectData()
		{
			body = 0;
			geom = 0;
			scale = 1.0f;
			timeAtRest = 0.0f;
		}

		bool exists() const
		{
			return body != 0;
		}
	};

	PhysicsConfig config;

	int num_planes;
	int num_objects;
	int entity_ids[MaxPhysicsObjects];
	dGeomID planes[MaxPhysicsPlanes];
	ObjectData objects[MaxPhysicsObjects];

	// todo: better data structure per-this, eg. max interactions per-object (MaxContacts?)
	std::vector< std::vector<uint16_t> > interactions;

    dContact contact[MaxContacts];			

	void UpdateInteractionPairs( dBodyID b1, dBodyID b2 )
	{
		if ( !b1 || !b2 )
			return;

		uint64_t objectId1 = (uint64_t) dBodyGetData( b1 );
		uint64_t objectId2 = (uint64_t) dBodyGetData( b2 );

		interactions[objectId1].push_back( uint16_t( objectId2 ) );
		interactions[objectId2].push_back( uint16_t( objectId1 ) );
	}

	static void NearCallback( void * data, dGeomID o1, dGeomID o2 )
	{
		PhysicsInternal * internal = (PhysicsInternal*) data;

		assert( internal );

	    dBodyID b1 = dGeomGetBody( o1 );
	    dBodyID b2 = dGeomGetBody( o2 );

		const int num_contacts = dCollide( o1, o2, MaxContacts, &internal->contact[0].geom, sizeof(dContact) );

        for ( int i = 0; i < num_contacts; i++ )
        {
            dJointID c = dJointCreateContact( internal->world, internal->contacts, internal->contact+i );
            dJointAttach( c, b1, b2 );
        }

        if ( num_contacts > 0 )
			internal->UpdateInteractionPairs( b1, b2 );
	}
};

// ------------------------------------------

int * PhysicsManager::GetInitCount()
{
	static int initCount = 0;
	return &initCount;
}

PhysicsManager::PhysicsManager()
{
	int * initCount = GetInitCount();
	if ( *initCount == 0 )
		dInitODE();
	(*initCount)++;
	internal = new PhysicsInternal();
}

PhysicsManager::~PhysicsManager()
{
	delete internal;
	internal = nullptr;
	int * initCount = GetInitCount();
	(*initCount)--;
	if ( *initCount == 0 )
		dCloseODE();
}

void PhysicsManager::Initialize()
{
	PhysicsConfig config;
	internal->config = config;

	internal->num_planes = 0;
	internal->num_objects = 0;
	memset( internal->entity_ids, 0, sizeof( internal->entity_ids ) );

	// create simulation

	internal->world = dWorldCreate();
    internal->contacts = dJointGroupCreate( 0 );
    dVector3 center = { 0,0,0 };
    dVector3 extents = { 100,100,100 };
    internal->space = dQuadTreeSpaceCreate( 0, center, extents, 10 );

	// configure world

	dWorldSetERP( internal->world, config.ERP );
	dWorldSetCFM( internal->world, config.CFM );
	dWorldSetQuickStepNumIterations( internal->world, config.MaxIterations );
	dWorldSetGravity( internal->world, 0, 0, -config.Gravity );
	dWorldSetContactSurfaceLayer( internal->world, config.ContactSurfaceLayer );
	dWorldSetContactMaxCorrectingVel( internal->world, config.MaximumCorrectingVelocity );
	dWorldSetLinearDamping( internal->world, config.LinearDamping );
	dWorldSetAngularDamping( internal->world, config.AngularDamping );
	dWorldSetAutoDisableFlag( internal->world, 0 );

	// setup contacts

    for ( int i = 0; i < MaxContacts; i++ ) 
    {
		internal->contact[i].surface.mode = dContactBounce;
		internal->contact[i].surface.mu = config.Friction;
		internal->contact[i].surface.bounce = config.Elasticity;
		internal->contact[i].surface.bounce_vel = 0.001f;
    }
}

void PhysicsManager::Update( uint64_t tick, double t, float dt, bool paused )
{	
	dRandSetSeed( tick );

	internal->interactions.clear();
	
	internal->interactions.resize( MaxPhysicsObjects );

	if ( paused )
		return;

	for ( int i = 0; i < MaxPhysicsObjects; ++i )
	{
		if ( !internal->objects[i].exists() )
			continue;

		if ( !IsActive( i ) )
			continue;

		const dReal * linearVelocity = dBodyGetLinearVel( internal->objects[i].body );
		const dReal * angularVelocity = dBodyGetAngularVel( internal->objects[i].body );

		const float linearVelocityLengthSquared = linearVelocity[0]*linearVelocity[0] + linearVelocity[1]*linearVelocity[1] + linearVelocity[2]*linearVelocity[2];
		const float angularVelocityLengthSquared = angularVelocity[0]*angularVelocity[0] + angularVelocity[1]*angularVelocity[1] + angularVelocity[2]*angularVelocity[2];

		if ( linearVelocityLengthSquared > MaxLinearSpeed * MaxLinearSpeed )
		{
			const float linearSpeed = sqrt( linearVelocityLengthSquared );

			const float scale = MaxLinearSpeed / linearSpeed;

			dReal clampedLinearVelocity[3];

			clampedLinearVelocity[0] = linearVelocity[0] * scale;
			clampedLinearVelocity[1] = linearVelocity[1] * scale;
			clampedLinearVelocity[2] = linearVelocity[2] * scale;

			dBodySetLinearVel( internal->objects[i].body, clampedLinearVelocity[0], clampedLinearVelocity[1], clampedLinearVelocity[2] );

			linearVelocity = &clampedLinearVelocity[0];
		}

		if ( angularVelocityLengthSquared > MaxAngularSpeed * MaxAngularSpeed )
		{
			const float angularSpeed = sqrt( angularVelocityLengthSquared );

			const float scale = MaxAngularSpeed / angularSpeed;

			dReal clampedAngularVelocity[3];

			clampedAngularVelocity[0] = angularVelocity[0] * scale;
			clampedAngularVelocity[1] = angularVelocity[1] * scale;
			clampedAngularVelocity[2] = angularVelocity[2] * scale;

			dBodySetAngularVel( internal->objects[i].body, clampedAngularVelocity[0], clampedAngularVelocity[1], clampedAngularVelocity[2] );

			angularVelocity = &clampedAngularVelocity[0];
		}

		if ( linearVelocityLengthSquared < internal->config.LinearRestThresholdSquared && angularVelocityLengthSquared < internal->config.AngularRestThresholdSquared )
			internal->objects[i].timeAtRest += dt;
		else
			internal->objects[i].timeAtRest = 0.0f;

		if ( internal->objects[i].timeAtRest >= internal->config.RestTime )
			dBodyDisable( internal->objects[i].body );
		else
			dBodyEnable( internal->objects[i].body );
	}

	dJointGroupEmpty( internal->contacts );

	dSpaceCollide( internal->space, internal, PhysicsInternal::NearCallback );

	if ( internal->config.QuickStep )
		dWorldQuickStep( internal->world, dt );
	else
		dWorldStep( internal->world, dt );
}

int PhysicsManager::AddObject( int entity_index, const PhysicsObjectState & object_state, PhysicsShape shape, float scale )
{
	assert( shape == PHYSICS_SHAPE_CUBE );		// no other shape supported yet!

	// find free object slot

	int index = -1;

	for ( int i = 0; i < MaxPhysicsObjects; ++i )
	{
		if ( !internal->objects[i].exists() )
		{
			index = i;
			break;
		}
	}

	assert( index != -1 );

	if ( index == -1 )
		return -1;

	// setup entity id for this physics object

	internal->entity_ids[index] = entity_index;

	// setup object body

	internal->objects[index].body = dBodyCreate( internal->world );

	assert( internal->objects[index].body );

	dMass mass;
	const float density = 1.0f;
	dMassSetBox( &mass, density, scale, scale, scale );
	dBodySetMass( internal->objects[index].body, &mass );
	dBodySetData( internal->objects[index].body, (void*) uint64_t(index) );

	// setup geom and attach to body

	internal->objects[index].scale = scale;
	internal->objects[index].geom = dCreateBox( internal->space, scale, scale, scale );

	dGeomSetBody( internal->objects[index].geom, internal->objects[index].body );

	// set object state

	SetObjectState( index, object_state );

	// success!

	return index;
}

bool PhysicsManager::ObjectExists( int index )
{
	assert( index >= 0 && index < MaxPhysicsObjects );
	return internal->objects[index].exists();
}

float PhysicsManager::GetObjectMass( int index )
{
	assert( index >= 0 && index < MaxPhysicsObjects );
	assert( internal->objects[index].exists() );
	dMass mass;
	dBodyGetMass( internal->objects[index].body, &mass );
	return mass.mass;
}

void PhysicsManager::RemoveObject( int index )
{
	assert( index >= 0 && index < MaxPhysicsObjects );
	assert( internal->objects[index].exists() );

	dBodyDestroy( internal->objects[index].body );
	dGeomDestroy( internal->objects[index].geom );
	internal->objects[index].body = 0;
	internal->objects[index].geom = 0;
}

void PhysicsManager::GetObjectState( int index, PhysicsObjectState & object_state ) const
{
	assert( index >= 0 );
	assert( index < MaxPhysicsObjects );

	assert( internal->objects[index].exists() );

	const dReal * position = dBodyGetPosition( internal->objects[index].body );
	const dReal * orientation = dBodyGetQuaternion( internal->objects[index].body );
	const dReal * linear_velocity = dBodyGetLinearVel( internal->objects[index].body );
	const dReal * angular_velocity = dBodyGetAngularVel( internal->objects[index].body );

	object_state.position = vec3f( position[0], position[1], position[2] );
	object_state.orientation = quat4f( orientation[1], orientation[2], orientation[3], orientation[0] );			// note: orientation[0] is w
	object_state.linear_velocity = vec3f( linear_velocity[0], linear_velocity[1], linear_velocity[2] );
	object_state.angular_velocity = vec3f( angular_velocity[0], angular_velocity[1], angular_velocity[2] );

	object_state.active = dBodyIsEnabled( internal->objects[index].body );
}

void PhysicsManager::SetObjectState( int index, const PhysicsObjectState & object_state )
{
	assert( index >= 0 );
	assert( index < MaxPhysicsObjects );

	assert( internal->objects[index].exists() );

	dQuaternion quaternion;
	quaternion[0] = object_state.orientation.w();
	quaternion[1] = object_state.orientation.x();
	quaternion[2] = object_state.orientation.y();
	quaternion[3] = object_state.orientation.z();

	dBodySetPosition( internal->objects[index].body, object_state.position.x(), object_state.position.y(), object_state.position.z() );
	dBodySetQuaternion( internal->objects[index].body, quaternion );
	dBodySetLinearVel( internal->objects[index].body, object_state.linear_velocity.x(), object_state.linear_velocity.y(), object_state.linear_velocity.z() );
	dBodySetAngularVel( internal->objects[index].body, object_state.angular_velocity.x(), object_state.angular_velocity.y(), object_state.angular_velocity.z() );

	// todo: under some circumstances if this object is currently disabled but the set object state is enabled, we want to enable it

	if ( !object_state.active )
	{
		internal->objects[index].timeAtRest = internal->config.RestTime;
		dBodyDisable( internal->objects[index].body );
	}
}

bool PhysicsManager::IsActive( int index ) const
{
	assert( index >= 0 );
	assert( index < MaxPhysicsObjects );
	assert( internal->objects[index].exists() );
	return dBodyIsEnabled( internal->objects[index].body );
}

void PhysicsManager::ApplyForce( int index, const vec3f & force )
{
	assert( index >= 0 );
	assert( index < MaxPhysicsObjects );
	assert( internal->objects[index].exists() );
	if ( length_squared( force ) > 0.000001f )
	{
		internal->objects[index].timeAtRest = 0.0f;
		dBodyEnable( internal->objects[index].body );
		dBodyAddForce( internal->objects[index].body, force.x(), force.y(), force.z() );
	}
}

void PhysicsManager::ApplyTorque( int index, const vec3f & torque )
{
	assert( index >= 0 );
	assert( index < MaxPhysicsObjects );
	assert( internal->objects[index].exists() );
	if ( length_squared( torque ) > 0.000001f )
	{
		internal->objects[index].timeAtRest = 0.0f;
		dBodyEnable( internal->objects[index].body );
		dBodyAddTorque( internal->objects[index].body, torque.x(), torque.y(), torque.z() );
	}
}

void PhysicsManager::AddPlane( const vec3f & normal, float d )
{
	assert( internal->num_planes < MaxPhysicsPlanes );
	internal->planes[internal->num_planes++] = dCreatePlane( internal->space, normal.x(), normal.y(), normal.z(), d );
}

void PhysicsManager::Reset()
{
	for ( int i = 0; i < MaxPhysicsObjects; ++i )
	{
		if ( internal->objects[i].exists() )
			RemoveObject( i );
	}

	for ( int i = 0; i < internal->num_planes; ++i )
		dGeomDestroy( internal->planes[i] );

	internal->num_planes = 0;
	internal->num_objects = 0;
	memset( internal->entity_ids, 0, sizeof( internal->entity_ids ) );
}

void PhysicsManager::WalkInteractions( EntityManager * entity_manager )
{
	for ( int player_id = ENTITY_PLAYER_BEGIN; player_id < ENTITY_PLAYER_END; ++player_id )
	{
		std::vector<bool> interacting( MaxPhysicsObjects, false );
		std::vector<bool> ignores( MaxPhysicsObjects, false );
		std::vector<int> queue( MaxPhysicsObjects );

		int head = 0;
		int tail = 0;

		// add all physics objects that are active and have non-default authority to the list

		for ( int i = 0; i < MaxPhysicsObjects; ++i )
		{
			if ( !internal->objects[i].exists() )
				continue;

			const int entity_index = internal->entity_ids[i];

			const int authority = entity_manager->GetAuthority( entity_index );

			if ( authority == player_id )
			{
				assert( head < MaxPhysicsObjects );
				queue[head++] = i;
			}
			else if ( authority == 0 )
			{
				ignores[i] = true;
			}
		}

		// recurse into object interactions while ignoring objects

		while ( head != tail )
		{
			const std::vector<uint16_t> & object_interactions = internal->interactions[ queue[tail] ];
			for ( int i = 0; i < (int) object_interactions.size(); ++i )
			{
				const int physics_id = object_interactions[i];
				assert( physics_id >= 0 );
				assert( physics_id < (int) interacting.size() );
				if ( !ignores[physics_id] && !interacting[physics_id] )
				{
					assert( head < MaxPhysicsObjects );
					queue[head++] = physics_id;
				}
				interacting[physics_id] = true;
			}
			tail++;
		}

		// pass over the interacting objects and set their authority
		
		for ( int i = 0; i < MaxPhysicsObjects; ++i )
		{
			if ( interacting[i] )
			{
				int entity_index = internal->entity_ids[i];
				assert( entity_index > ENTITY_WORLD );
				assert( entity_index < MaxEntities );
				int authority = entity_manager->GetAuthority( entity_index );
				if ( ( authority == 0 || authority == player_id ) && IsActive( i ) )
					entity_manager->SetAuthority( entity_index, player_id );
			}
		}
	}
}
