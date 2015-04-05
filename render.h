// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef RENDER_H
#define RENDER_H

#include "core.h"
#include "const.h"
#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "vectorial/vec3f.h"
#include "vectorial/mat4f.h"

using namespace vectorial;

void clear_opengl_error();

void check_opengl_error( const char * message );

uint32_t load_shader( const char * vertex_file_path, const char * fragment_file_path );

void delete_shader( uint32_t shader );

struct RenderCube
{
    float r,g,b,a;
    mat4f transform;
    mat4f inverse_transform;
};

struct RenderCubes
{
    RenderCubes()
    {
        num_cubes = 0;
    }

    int num_cubes;
    RenderCube cube[MaxViewObjects];
};

struct RenderCubeInstance
{
    float r,g,b,a;
    mat4f model;
    mat4f modelView;
    mat4f modelViewProjection;
};

class Render
{
public:

    Render();
    ~Render();

    void ResizeDisplay( int displayWidth, int displayHeight );
    
    void SetLightPosition( const vec3f & position );

    void SetCamera( const vec3f & position, const vec3f & lookAt, const vec3f & up );

    void ClearScreen();

    void BeginScene( float x1, float y1, float x2, float y2 );
 
    void RenderScene( const RenderCubes & cubes );
    
    void RenderShadows( const RenderCubes & cubes );

    void RenderShadowQuad();

    void EndScene();
                
private:

    void Initialize();

    bool initialized;

    int display_width;
    int display_height;

    vec3f camera_position;
    vec3f camera_lookAt;
    vec3f camera_up;

    vec3f light_position;

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

#endif // #ifndef RENDER_H
