// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef PHYSICS_H
#define PHYSICS_H

#include "const.h"
#include "vectorial/vec3f.h"
#include "vectorial/quat4f.h"
#include <vector>						// todo: remove this!

using namespace vectorial;

enum PhysicsShape
{
	PHYSICS_SHAPE_CUBE
};

struct PhysicsObjectState
{
	bool enabled;
	float scale;
	vec3f position;
	quat4f orientation;
	vec3f linear_velocity;
	vec3f angular_velocity;
};

class Physics
{	
	static int * GetInitCount();

public:

	Physics();
	~Physics();

	void Initialize();

	void Update( float delta_time, bool paused = false );

	int AddObject( const PhysicsObjectState & object_state, PhysicsShape shape );

	bool ObjectExists( int id );

	float GetObjectMass( int id );

	void RemoveObject( int id );
	
	void GetObjectState( int id, PhysicsObjectState & object_state );

	void SetObjectState( int id, const PhysicsObjectState & object_state );

	const std::vector<uint16_t> & GetObjectInteractions( int id ) const;

	int GetNumInteractionPairs() const;

	void ApplyForce( int id, const vec3f & force );

	void ApplyTorque( int id, const vec3f & torque );

	void AddPlane( const vec3f & normal, float d );

	void Reset();

private:

	struct PhysicsInternal * internal;
};

#endif
