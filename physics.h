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
	bool enabled = true;
	float scale = 1.0f;
	vec3f position;
	quat4f orientation;
	vec3f linear_velocity;
	vec3f angular_velocity;
};

class PhysicsManager
{	
	static int * GetInitCount();

public:

	PhysicsManager();
	~PhysicsManager();

	void Initialize();

	void Update( uint64_t tick, double time, float delta_time, bool paused = false );

	int AddObject( const PhysicsObjectState & object_state, PhysicsShape shape );

	bool ObjectExists( int index );

	float GetObjectMass( int index );

	void RemoveObject( int index );
	
	void GetObjectState( int index, PhysicsObjectState & object_state );

	void SetObjectState( int index, const PhysicsObjectState & object_state );

	const std::vector<uint16_t> & GetObjectInteractions( int index ) const;

	int GetNumInteractionPairs() const;

	void ApplyForce( int index, const vec3f & force );

	void ApplyTorque( int index, const vec3f & torque );

	void AddPlane( const vec3f & normal, float d );

	void Reset();

private:

	struct PhysicsInternal * internal;
};

#endif
