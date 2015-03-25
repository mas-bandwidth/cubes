// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "physics.h"
#include "assert.h"
#define dSINGLE
#include <ode/ode.h>

const int MaxContacts = 16;

// todo: stuff to look into dBodySetMovedCallback, dBodySetGyroscopicMode

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
		CFM = 0.01f;
		MaxIterations = 64;
		MaximumCorrectingVelocity = 5.0f;
		ContactSurfaceLayer = 0.01f;
		Elasticity = 0.0f;
		LinearDamping = 0.01f;
		AngularDamping = 0.01f;
		Friction = 200.0f;
		Gravity = 20.0f;
		QuickStep = true;
		RestTime = 0.1f;
		LinearRestThresholdSquared = 0.25f * 0.25f;
		AngularRestThresholdSquared = 0.25f * 0.25f;
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
	std::vector<dGeomID> planes;
	std::vector<ObjectData> objects;
	std::vector< std::vector<uint16_t> > interactions;

    dContact contact[MaxContacts];			

	void UpdateInteractionPairs( dBodyID b1, dBodyID b2 )
	{
		if ( !b1 || !b2 )
			return;

		uint64_t objectId1 = reinterpret_cast<uint64_t>( dBodyGetData( b1 ) );
		uint64_t objectId2 = reinterpret_cast<uint64_t>( dBodyGetData( b2 ) );

		interactions[objectId1].push_back( objectId2 );
		interactions[objectId2].push_back( objectId1 );
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

int * Physics::GetInitCount()
{
	static int initCount = 0;
	return &initCount;
}

Physics::Physics()
{
	int * initCount = GetInitCount();
	if ( *initCount == 0 )
		dInitODE();
	(*initCount)++;
	internal = new PhysicsInternal();
}

Physics::~Physics()
{
	delete internal;
	internal = nullptr;
	int * initCount = GetInitCount();
	(*initCount)--;
	if ( *initCount == 0 )
		dCloseODE();
}

void Physics::Initialize()
{
	PhysicsConfig config;
	internal->config = config;

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

	// setup contacts

    for ( int i = 0; i < MaxContacts; i++ ) 
    {
		internal->contact[i].surface.mode = dContactBounce;
		internal->contact[i].surface.mu = config.Friction;
		internal->contact[i].surface.bounce = config.Elasticity;
		internal->contact[i].surface.bounce_vel = 0.001f;
    }

	internal->objects.resize( 1024 );
}

void Physics::Update( float delta_time, bool paused )
{		
	internal->interactions.clear();
	
	internal->interactions.resize( internal->objects.size() );

	if ( paused )
		return;

	// IMPORTANT: do this *first* before updating simulation then at rest calculations
	// will work properly with rough quantization (quantized state is fed in prior to update)
	for ( int i = 0; i < (int) internal->objects.size(); ++i )
	{
		if ( internal->objects[i].exists() )
		{
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
				internal->objects[i].timeAtRest += delta_time;
			else
				internal->objects[i].timeAtRest = 0.0f;

			if ( internal->objects[i].timeAtRest >= internal->config.RestTime )
				dBodyDisable( internal->objects[i].body );
			else
				dBodyEnable( internal->objects[i].body );
		}
	}

	dJointGroupEmpty( internal->contacts );

	dSpaceCollide( internal->space, internal, PhysicsInternal::NearCallback );

	if ( internal->config.QuickStep )
		dWorldQuickStep( internal->world, delta_time );
	else
		dWorldStep( internal->world, delta_time );
}

int Physics::AddObject( const PhysicsObjectState & object_state, PhysicsShape shape )
{
	assert( shape == PHYSICS_SHAPE_CUBE );		// no other shape supported yet!

	// find free object slot

	uint64_t id = -1;
	for ( int i = 0; i < (int) internal->objects.size(); ++i )
	{
		if ( !internal->objects[i].exists() )
		{
			id = i;
			break;
		}
	}
	if ( id == -1 )
	{
		id = internal->objects.size();
		internal->objects.resize( id + 1 );
	}

	// setup object body

	internal->objects[id].body = dBodyCreate( internal->world );

	assert( internal->objects[id].body );

	dMass mass;
	const float density = 1.0f;
	dMassSetBox( &mass, density, object_state.scale, object_state.scale, object_state.scale );
	dBodySetMass( internal->objects[id].body, &mass );
	dBodySetData( internal->objects[id].body, (void*) id );

	// setup geom and attach to body

	internal->objects[id].scale = object_state.scale;
	internal->objects[id].geom = dCreateBox( internal->space, object_state.scale, object_state.scale, object_state.scale );

	dGeomSetBody( internal->objects[id].geom, internal->objects[id].body );	

	// set object state

	SetObjectState( id, object_state );

	// success!

	return id;
}

bool Physics::ObjectExists( int id )
{
	assert( id >= 0 && id < (int) internal->objects.size() );
	return internal->objects[id].exists();
}

float Physics::GetObjectMass( int id )
{
	assert( id >= 0 && id < (int) internal->objects.size() );
	assert( internal->objects[id].exists() );
	dMass mass;
	dBodyGetMass( internal->objects[id].body, &mass );
	return mass.mass;
}

void Physics::RemoveObject( int id )
{
	assert( id >= 0 && id < (int) internal->objects.size() );
	assert( internal->objects[id].exists() );

	dBodyDestroy( internal->objects[id].body );
	dGeomDestroy( internal->objects[id].geom );
	internal->objects[id].body = 0;
	internal->objects[id].geom = 0;
}

void Physics::GetObjectState( int id, PhysicsObjectState & object_state )
{
	assert( id >= 0 );
	assert( id < (int) internal->objects.size() );

	assert( internal->objects[id].exists() );

	const dReal * position = dBodyGetPosition( internal->objects[id].body );
	const dReal * orientation = dBodyGetQuaternion( internal->objects[id].body );
	const dReal * linear_velocity = dBodyGetLinearVel( internal->objects[id].body );
	const dReal * angular_velocity = dBodyGetAngularVel( internal->objects[id].body );

	object_state.position = vec3f( position[0], position[1], position[2] );
	object_state.orientation = quat4f( orientation[1], orientation[2], orientation[3], orientation[0] );			// note: orientation[0] is w
	object_state.linear_velocity = vec3f( linear_velocity[0], linear_velocity[1], linear_velocity[2] );
	object_state.angular_velocity = vec3f( angular_velocity[0], angular_velocity[1], angular_velocity[2] );

	object_state.enabled = internal->objects[id].timeAtRest < internal->config.RestTime;
}

void Physics::SetObjectState( int id, const PhysicsObjectState & object_state )
{
	assert( id >= 0 );
	assert( id < (int) internal->objects.size() );

	assert( internal->objects[id].exists() );

	dQuaternion quaternion;
	quaternion[0] = object_state.orientation.w();
	quaternion[1] = object_state.orientation.x();
	quaternion[2] = object_state.orientation.y();
	quaternion[3] = object_state.orientation.z();

	dBodySetPosition( internal->objects[id].body, object_state.position.x(), object_state.position.y(), object_state.position.z() );
	dBodySetQuaternion( internal->objects[id].body, quaternion );
	dBodySetLinearVel( internal->objects[id].body, object_state.linear_velocity.x(), object_state.linear_velocity.y(), object_state.linear_velocity.z() );
	dBodySetAngularVel( internal->objects[id].body, object_state.angular_velocity.x(), object_state.angular_velocity.y(), object_state.angular_velocity.z() );

	if ( object_state.enabled )
	{
		internal->objects[id].timeAtRest = 0.0f;
		dBodyEnable( internal->objects[id].body );
	}
	else
	{
		internal->objects[id].timeAtRest = internal->config.RestTime;
		dBodyDisable( internal->objects[id].body );
	}
}

const std::vector<uint16_t> & Physics::GetObjectInteractions( int id ) const
{
	assert( id >= 0 );
	assert( id < (int) internal->interactions.size() );
	return internal->interactions[id];
}

void Physics::ApplyForce( int id, const vec3f & force )
{
	assert( id >= 0 );
	assert( id < (int) internal->objects.size() );
	assert( internal->objects[id].exists() );
	if ( length_squared( force ) > 0.000001f )
	{
		internal->objects[id].timeAtRest = 0.0f;
		dBodyEnable( internal->objects[id].body );
		dBodyAddForce( internal->objects[id].body, force.x(), force.y(), force.z() );
	}
}

void Physics::ApplyTorque( int id, const vec3f & torque )
{
	assert( id >= 0 );
	assert( id < (int) internal->objects.size() );
	assert( internal->objects[id].exists() );
	if ( length_squared( torque ) > 0.000001f )
	{
		internal->objects[id].timeAtRest = 0.0f;
		dBodyEnable( internal->objects[id].body );
		dBodyAddTorque( internal->objects[id].body, torque.x(), torque.y(), torque.z() );
	}
}

void Physics::AddPlane( const vec3f & normal, float d )
{
	internal->planes.push_back( dCreatePlane( internal->space, normal.x(), normal.y(), normal.z(), d ) );
}

void Physics::Reset()
{
	for ( int i = 0; i < (int) internal->objects.size(); ++i )
	{
		if ( internal->objects[i].exists() )
			RemoveObject( i );
	}

	for ( int i = 0; i < (int) internal->planes.size(); ++i )
		dGeomDestroy( internal->planes[i] );

	internal->planes.clear();
}
