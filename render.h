// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef RENDER_H
#define RENDER_H

#include "core.h"
#include "const.h"
#include "vectorial/vec3f.h"
#include "vectorial/mat4f.h"

using namespace vectorial;

struct RenderCube
{
    float r,g,b,a;
    mat4f transform;
    mat4f inverse_transform;
};

struct RenderState
{
    RenderState()
    {
        num_cubes = 0;
    }

    int num_cubes;
    RenderCube cube[MaxCubes];
};

void render_get_state( const struct World & world, RenderState & render_state );

struct RenderCubeInstance
{
    float r,g,b,a;
    mat4f model;
    mat4f model_view;
    mat4f model_view_projection;
};

class Render
{
public:

    Render();
    ~Render();

    void ResizeDisplay( int display_width, int display_height );
    
    void SetLightPosition( const vec3f & position );

    void SetCamera( const vec3f & position, const vec3f & lookat, const vec3f & up );

    void ClearScreen();

    void BeginScene( float x1, float y1, float x2, float y2 );
 
    void RenderScene( const RenderState & render_state );
    
    void RenderShadows( const RenderState & render_state );

    void RenderShadowQuad();

    void EndScene();
                
private:

    void Initialize();

    bool initialized;

    int display_width;
    int display_height;

    vec3f camera_position;
    vec3f camera_lookat;
    vec3f camera_up;

    vec3f light_position;

    uint32_t shadow_shader;
    uint32_t cubes_shader;
    uint32_t debug_shader;

    uint32_t cubes_vao;
    uint32_t cubes_vbo;
    uint32_t cubes_ibo;
    uint32_t cubes_instance_buffer;

    uint32_t shadow_vao;
    uint32_t shadow_vbo;

    uint32_t mask_vao;
    uint32_t mask_vbo;

    vec3f shadow_vertices[MaxCubeShadowVertices];

    RenderCubeInstance instance_data[MaxCubes];
};

struct Camera
{
    vec3f position;
    vec3f lookat;
    vec3f up;

    Camera()
    {
        position = vec3f::zero();
        lookat = vec3f::zero();
        up = vec3f(0,0,1);
    }

    void EaseIn( const vec3f & new_lookat, const vec3f & new_position )
    {
        const float epsilon = 0.00001f;

        if ( length_squared( new_lookat - lookat ) > epsilon )
            lookat += ( new_lookat - lookat ) * 0.15f;

        if ( length_squared( new_position - position ) > epsilon )
            position += ( new_position - position ) * 0.15f;
        
        up = cross( position - lookat, vec3f(1,0,0) );
    }

    void Snap( const vec3f & new_lookat, const vec3f & new_position )
    {
        lookat = new_lookat;
        position = new_position;
        up = cross( position - lookat, vec3f(1,0,0) );
    }
};

void clear_opengl_error();

void check_opengl_error( const char * message );

uint32_t load_shader( const char * vertex_file_path, const char * fragment_file_path );

void delete_shader( uint32_t shader );

#endif // #ifndef RENDER_H
