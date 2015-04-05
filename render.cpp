#include "render.h"
#include <stdio.h>
#include <assert.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

struct CubeVertex
{
    float x,y,z;
    float nx,ny,nz;
};

CubeVertex cube_vertices[] = 
{
    { +1, -1, +1, 0, 0, +1 },
    { -1, -1, +1, 0, 0, +1 },
    { -1, +1, +1, 0, 0, +1 },
    { +1, +1, +1, 0, 0, +1 },

    { -1, -1, -1, 0, 0, -1 },
    { +1, -1, -1, 0, 0, -1 },
    { +1, +1, -1, 0, 0, -1 },
    { -1, +1, -1, 0, 0, -1 },
 
    { +1, +1, -1, +1, 0, 0 },
    { +1, -1, -1, +1, 0, 0 },
    { +1, -1, +1, +1, 0, 0 },
    { +1, +1, +1, +1, 0, 0 },
    
    { -1, -1, -1, -1, 0, 0 },
    { -1, +1, -1, -1, 0, 0 },
    { -1, +1, +1, -1, 0, 0 },
    { -1, -1, +1, -1, 0, 0 },

    { -1, +1, -1, 0, +1, 0 },
    { +1, +1, -1, 0, +1, 0 },
    { +1, +1, +1, 0, +1, 0 },
    { -1, +1, +1, 0, +1, 0 },

    { +1, -1, -1, 0, -1, 0 },
    { -1, -1, -1, 0, -1, 0 },
    { -1, -1, +1, 0, -1, 0 },
    { +1, -1, +1, 0, -1, 0 },
};

uint16_t cube_indices[] = 
{
    0, 1, 2,
    0, 2, 3,

    4, 5, 6,
    4, 6, 7,

    8, 9, 10,
    8, 10, 11,

    12, 13, 14,
    12, 14, 15,

    16, 17, 18,
    16, 18, 19,

    20, 21, 22,
    20, 22, 23,
};

struct DebugVertex
{
    float x,y,z;
    float r,g,b,a;
};

Render::Render()
{
    initialized = false;    
    display_width = 0;
    display_height = 0;
    cubes_vao = 0;
    cubes_vbo = 0;
    cubes_ibo = 0;
    cubes_instance_buffer = 0;
    shadow_vao = 0;
    shadow_vbo = 0;
    mask_vao = 0;
    mask_vbo = 0;
    shadow_shader = 0;
    cubes_shader = 0;
    debug_shader = 0;
}

Render::~Render()
{
    glDeleteVertexArrays( 1, &cubes_vao );
    glDeleteBuffers( 1, &cubes_vbo );
    glDeleteBuffers( 1, &cubes_ibo );
    glDeleteBuffers( 1, &cubes_instance_buffer );

    cubes_vao = 0;
    cubes_vbo = 0;
    cubes_ibo = 0;
    cubes_instance_buffer = 0;

    glDeleteVertexArrays( 1, &shadow_vao );
    glDeleteBuffers( 1, &shadow_vbo );

    shadow_vao = 0;
    shadow_vbo = 0;

    glDeleteVertexArrays( 1, &mask_vao );
    glDeleteBuffers( 1, &mask_vbo );

    mask_vao = 0;
    mask_vbo = 0;
}

void Render::Initialize()
{
    assert( !initialized );

    shadow_shader = load_shader( "shaders/shadow.vert", "shaders/shadow.frag" );
    cubes_shader = load_shader( "shaders/cubes.vert", "shaders/cubes.frag" );
    debug_shader = load_shader( "shaders/debug.vert", "shaders/debug.frag" );

    assert( shadow_shader );
    assert( cubes_shader );
    assert( debug_shader );

    if ( !shadow_shader || !cubes_shader || !debug_shader )
        return;

    // setup cubes draw call
    {
        glUseProgram( cubes_shader );

        const int position_location = glGetAttribLocation( cubes_shader, "VertexPosition" );
        const int normal_location = glGetAttribLocation( cubes_shader, "VertexNormal" );
        const int color_location = glGetAttribLocation( cubes_shader, "VertexColor" );
        const int model_location = glGetAttribLocation( cubes_shader, "Model" );
        const int model_view_location = glGetAttribLocation( cubes_shader, "ModelView" );
        const int model_view_projection_location = glGetAttribLocation( cubes_shader, "ModelViewProjection" );

        glGenBuffers( 1, &cubes_vbo );
        glBindBuffer( GL_ARRAY_BUFFER, cubes_vbo );
        glBufferData( GL_ARRAY_BUFFER, sizeof( cube_vertices ), cube_vertices, GL_STATIC_DRAW );
        glBindBuffer( GL_ARRAY_BUFFER, 0 );
     
        glGenBuffers( 1, &cubes_ibo );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, cubes_ibo );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( cube_indices ), cube_indices, GL_STATIC_DRAW );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

        glGenBuffers( 1, &cubes_instance_buffer );

        glGenVertexArrays( 1, &cubes_vao );

        glBindVertexArray( cubes_vao );

        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, cubes_ibo );

        glBindBuffer( GL_ARRAY_BUFFER, cubes_vbo );

        if ( position_location >= 0 )
        {
            glEnableVertexAttribArray( position_location );
            glVertexAttribPointer( position_location, 3, GL_FLOAT, GL_FALSE, sizeof(CubeVertex), (GLubyte*)0 );
        }

        if ( normal_location >= 0 )
        {
            glEnableVertexAttribArray( normal_location );
            glVertexAttribPointer( normal_location, 3, GL_FLOAT, GL_FALSE, sizeof(CubeVertex), (GLubyte*)(3*4) );
        }

        glBindBuffer( GL_ARRAY_BUFFER, cubes_instance_buffer );

        if ( color_location >= 0 )
        {
            glEnableVertexAttribArray( color_location );
            glVertexAttribPointer( color_location, 4, GL_FLOAT, GL_FALSE, sizeof( RenderCubeInstance ), (void*) ( offsetof( RenderCubeInstance, r ) ) );
            glVertexAttribDivisor( color_location, 1 );
        }

        for ( unsigned int i = 0; i < 4 ; ++i )
        {
            if ( model_location >= 0 )
            {
                glEnableVertexAttribArray( model_location + i );
                glVertexAttribPointer( model_location + i, 4, GL_FLOAT, GL_FALSE, sizeof( RenderCubeInstance ), (void*) ( offsetof( RenderCubeInstance, model ) + ( sizeof(float) * 4 * i ) ) );
                glVertexAttribDivisor( model_location + i, 1 );
            }

            if ( model_view_location >= 0 )
            {
                glEnableVertexAttribArray( model_view_location + i );
                glVertexAttribPointer( model_view_location + i, 4, GL_FLOAT, GL_FALSE, sizeof( RenderCubeInstance ), (void*) ( offsetof( RenderCubeInstance, modelView ) + ( sizeof(float) * 4 * i ) ) );
                glVertexAttribDivisor( model_view_location + i, 1 );
            }

            if ( model_view_projection_location >= 0 )
            {
                glEnableVertexAttribArray( model_view_projection_location + i );
                glVertexAttribPointer( model_view_projection_location + i, 4, GL_FLOAT, GL_FALSE, sizeof( RenderCubeInstance ), (void*) ( offsetof( RenderCubeInstance, modelViewProjection ) + ( sizeof(float) * 4 * i ) ) );
                glVertexAttribDivisor( model_view_projection_location + i, 1 );
            }
        }

        glBindBuffer( GL_ARRAY_BUFFER, 0 );

        glBindVertexArray( 0 );

        glUseProgram( 0 );
    }

    // setup shadow draw call
    {
        glUseProgram( shadow_shader );

        const int position_location = glGetAttribLocation( shadow_shader, "VertexPosition" );

        glGenBuffers( 1, &shadow_vbo );
     
        glGenVertexArrays( 1, &shadow_vao );

        glBindVertexArray( shadow_vao );

        glBindBuffer( GL_ARRAY_BUFFER, shadow_vbo );

        if ( position_location >= 0 )
        {
            glEnableVertexAttribArray( position_location );
            glVertexAttribPointer( position_location, 3, GL_FLOAT, GL_FALSE, sizeof(vectorial::vec3f), (GLubyte*)0 );
        }

        glBindBuffer( GL_ARRAY_BUFFER, 0 );

        glBindVertexArray( 0 );

        glUseProgram( 0 );
    }

    // setup mask draw call
    {
        glUseProgram( debug_shader );

        const int position_location = glGetAttribLocation( debug_shader, "VertexPosition" );
        const int color_location = glGetAttribLocation( debug_shader, "VertexColor" );

        const float shadow_alpha = 0.25f;

        DebugVertex mask_vertices[6] =
        {
            { 0, 0, 0, 0, 0, 0, shadow_alpha },
            { 1, 0, 0, 0, 0, 0, shadow_alpha },
            { 1, 1, 0, 0, 0, 0, shadow_alpha },
            { 0, 0, 0, 0, 0, 0, shadow_alpha },
            { 1, 1, 0, 0, 0, 0, shadow_alpha },
            { 0, 1, 0, 0, 0, 0, shadow_alpha },
        };

        glGenBuffers( 1, &mask_vbo );
        glBindBuffer( GL_ARRAY_BUFFER, mask_vbo );
        glBufferData( GL_ARRAY_BUFFER, sizeof( DebugVertex ) * 6, mask_vertices, GL_STATIC_DRAW );
        glBindBuffer( GL_ARRAY_BUFFER, 0 );
     
        glGenVertexArrays( 1, &mask_vao );

        glBindVertexArray( mask_vao );

        glBindBuffer( GL_ARRAY_BUFFER, mask_vbo );

        if ( position_location >= 0 )
        {
            glEnableVertexAttribArray( position_location );
            glVertexAttribPointer( position_location, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (GLubyte*)0 );
        }

        if ( color_location >= 0 )
        {
            glEnableVertexAttribArray( color_location );
            glVertexAttribPointer( color_location, 4, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (GLubyte*) ( 3 * 4 ) );
        }

        glBindBuffer( GL_ARRAY_BUFFER, 0 );

        glBindVertexArray( 0 );

        glUseProgram( 0 );
    }

    initialized = true;

    check_opengl_error( "after cubes render init" );
}

void Render::ResizeDisplay( int _display_width, int _display_height )
{
    display_width = _display_width;
    display_height = _display_height;
}

void Render::SetLightPosition( const vectorial::vec3f & _light_position )
{
    light_position = _light_position;
}

void Render::SetCamera( const vectorial::vec3f & position, const vectorial::vec3f & lookat, const vectorial::vec3f & up )
{
    camera_position = position;
    camera_lookat = lookat;
    camera_up = up;
}

void Render::ClearScreen()
{
    glViewport( 0, 0, display_width, display_height );
    glClearStencil( 0 );
    glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );     
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
}

void Render::BeginScene( float x1, float y1, float x2, float y2 )
{
    if ( !initialized )
        Initialize();

    if ( !initialized )
        return;

    glViewport( x1, y1, x2 - x1, y2 - y1 );

    glEnable( GL_SCISSOR_TEST );
    glScissor( x1, y1, x2 - x1, y2 - y1 );

    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glEnable( GL_DEPTH_TEST );
    glDepthFunc( GL_LESS );
}
        
void Render::RenderScene( const RenderCubes & cubes )
{
    if ( cubes.num_cubes == 0 )
        return;

    glUseProgram( cubes_shader );

    const int eye_location = glGetUniformLocation( cubes_shader, "EyePosition" );
    const int light_location = glGetUniformLocation( cubes_shader, "LightPosition" );

    if ( eye_location >= 0 )
    {
        float data[3];
        camera_position.store( data );
        glUniform3fv( eye_location, 1, data );
    }

    if ( light_location >= 0 )
    {
        float data[3];
        light_position.store( data );
        glUniform3fv( light_location, 1, data );
    }

    vectorial::mat4f viewMatrix = vectorial::mat4f::lookAt( camera_position, camera_lookat, camera_up );

    vectorial::mat4f projectionMatrix = vectorial::mat4f::perspective( 40.0f, display_width / (float)display_height, 0.1f, 100.0f );

    for ( int i = 0; i < cubes.num_cubes; ++i )
    {
        RenderCubeInstance & instance = instance_data[i];
        instance.r = cubes.cube[i].r;
        instance.g = cubes.cube[i].g;
        instance.b = cubes.cube[i].b;
        instance.a = cubes.cube[i].a;
        instance.model = cubes.cube[i].transform;
        instance.modelView = viewMatrix * instance.model;
        instance.modelViewProjection = projectionMatrix * instance.modelView;
    }

    glBindBuffer( GL_ARRAY_BUFFER, cubes_instance_buffer );
    glBufferData( GL_ARRAY_BUFFER, sizeof(RenderCubeInstance) * cubes.num_cubes, instance_data, GL_STREAM_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    glBindVertexArray( cubes_vao );

    glDrawElementsInstanced( GL_TRIANGLES, sizeof( cube_indices ) / 2, GL_UNSIGNED_SHORT, nullptr, cubes.num_cubes );

    glBindVertexArray( 0 );

    glUseProgram( 0 );

    check_opengl_error( "after render scene" );
}

inline void GenerateSilhoutteVerts( int & vertex_index,
                                    __restrict vectorial::vec3f * vertices,
                                    const vectorial::mat4f & transform,
                                    const vectorial::vec3f & local_light,
                                    const vectorial::vec3f & world_light,
                                    vectorial::vec3f a, 
                                    vectorial::vec3f b,
                                    const vectorial::vec3f & left_normal,
                                    const vectorial::vec3f & right_normal,
                                    bool left_dot,
                                    bool right_dot )
{
    // check if silhouette edge
    if ( left_dot ^ right_dot )
    {
        // ensure correct winding order for silhouette edge

        const vectorial::vec3f cross = vectorial::cross( b - a, local_light );
        if ( dot( cross, a ) < 0 )
        {
            vectorial::vec3f tmp = a;
            a = b;
            b = tmp;
        }
        
        // transform into world space
        
        vectorial::vec3f world_a = transformPoint( transform, a );
        vectorial::vec3f world_b = transformPoint( transform, b );
        
        // extrude to ground plane z=0 in world space
        
        vectorial::vec3f difference_a = world_a - world_light;
        vectorial::vec3f difference_b = world_b - world_light;
        
        const float at = world_light.z() / difference_a.z();
        const float bt = world_light.z() / difference_b.z();
        
        vectorial::vec3f extruded_a = world_light - difference_a * at;
        vectorial::vec3f extruded_b = world_light - difference_b * bt;
        
        // emit extruded quad as two triangles
        
        vertices[vertex_index]   = world_b;
        vertices[vertex_index+1] = world_a;
        vertices[vertex_index+2] = extruded_a;
        vertices[vertex_index+3] = world_b;
        vertices[vertex_index+4] = extruded_a;
        vertices[vertex_index+5] = extruded_b;
        
        vertex_index += 6;
    }
}

static vectorial::vec3f a(-1,+1,-1);
static vectorial::vec3f b(+1,+1,-1);
static vectorial::vec3f c(+1,+1,+1);
static vectorial::vec3f d(-1,+1,+1);
static vectorial::vec3f e(-1,-1,-1);
static vectorial::vec3f f(+1,-1,-1);
static vectorial::vec3f g(+1,-1,+1);
static vectorial::vec3f h(-1,-1,+1);

static vectorial::vec3f normals[6] =
{
    vectorial::vec3f(+1,0,0),
    vectorial::vec3f(-1,0,0),
    vectorial::vec3f(0,+1,0),
    vectorial::vec3f(0,-1,0),
    vectorial::vec3f(0,0,+1),
    vectorial::vec3f(0,0,-1)
};

void Render::RenderShadows( const RenderCubes & cubes )
{
    // generate shadow silhoutte vertices

    vectorial::vec3f world_light = light_position;
    
    int vertex_index = 0;
    
    for ( int i = 0; i < (int) cubes.num_cubes; ++i )
    {
        const RenderCube & cube = cubes.cube[i];

        if ( cube.a < ShadowAlphaThreshold )
            continue;

        vectorial::vec3f local_light = transformPoint( cube.inverse_transform, world_light );
        
        bool dot[6];
        dot[0] = vectorial::dot( normals[0], local_light ) > 0;
        dot[1] = vectorial::dot( normals[1], local_light ) > 0;
        dot[2] = vectorial::dot( normals[2], local_light ) > 0;
        dot[3] = vectorial::dot( normals[3], local_light ) > 0;
        dot[4] = vectorial::dot( normals[4], local_light ) > 0;
        dot[5] = vectorial::dot( normals[5], local_light ) > 0;
        
        assert( vertex_index < MaxCubeShadowVertices );

        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, a, b, normals[5], normals[2], dot[5], dot[2] );
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, b, c, normals[0], normals[2], dot[0], dot[2] );
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, c, d, normals[4], normals[2], dot[4], dot[2] );
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, d, a, normals[1], normals[2], dot[1], dot[2] );
        
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, e, f, normals[3], normals[5], dot[3], dot[5] );
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, f, g, normals[3], normals[0], dot[3], dot[0] );
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, g, h, normals[3], normals[4], dot[3], dot[4] );
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, h, e, normals[3], normals[1], dot[3], dot[1] );

        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, a, e, normals[1], normals[5], dot[1], dot[5] );
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, b, f, normals[5], normals[0], dot[5], dot[0] );
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, c, g, normals[0], normals[4], dot[0], dot[4] );
        GenerateSilhoutteVerts( vertex_index, shadow_vertices, cube.transform, local_light, world_light, d, h, normals[4], normals[1], dot[4], dot[1] );

        assert( vertex_index < MaxCubeShadowVertices );
    }

    // upload vertices to shadow vbo

    glBindBuffer( GL_ARRAY_BUFFER, shadow_vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof( vectorial::vec3f ) * vertex_index, shadow_vertices, GL_STREAM_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    // setup for zpass stencil shadow rendering in one pass

#if !DEBUG_CUBE_SHADOWS

    glStencilOpSeparate( GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT );
    glStencilOpSeparate( GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT );
    glStencilMask( ~0 );
    glStencilFunc( GL_ALWAYS, 0, ~0 );

    glDepthMask( GL_FALSE );
    glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
    glEnable( GL_STENCIL_TEST );
    glDisable( GL_CULL_FACE ); 

#else

    glDisable( GL_CULL_FACE );

#endif

    // render shadow silhouette triangles

    glUseProgram( shadow_shader );

    vectorial::mat4f viewMatrix = vectorial::mat4f::lookAt( camera_position, camera_lookat, camera_up );

    vectorial::mat4f projectionMatrix = vectorial::mat4f::perspective( 40.0f, display_width / (float)display_height, 0.1f, 100.0f );

    vectorial::mat4f modelViewProjection = projectionMatrix * viewMatrix;

    int location = glGetUniformLocation( shadow_shader, "ModelViewProjection" );
    if ( location >= 0 )
        glUniformMatrix4fv( location, 1, GL_FALSE, (float*) &modelViewProjection );

    glBindVertexArray( shadow_vao );

    glDrawArrays( GL_TRIANGLES, 0, vertex_index );

    glBindVertexArray( 0 );

    glUseProgram( 0 );

    // clean up

    glCullFace( GL_BACK );
    glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
    glDepthMask( GL_TRUE );
    glDisable( GL_STENCIL_TEST );
   
    check_opengl_error( "after cube shadows" );
}

void Render::RenderShadowQuad()
{
    float x1 = 0;
    float y1 = 0;
    float x2 = display_width;
    float y2 = display_height;

    glViewport( x1, y1, x2 - x1, y2 - y1 );

    glEnable( GL_SCISSOR_TEST );
    glScissor( x1, y1, x2 - x1, y2 - y1 );

    glUseProgram( debug_shader );

    glEnable( GL_DEPTH_TEST );

    glEnable( GL_STENCIL_TEST );

    glStencilFunc( GL_NOTEQUAL, 0x0, 0xff );
    glStencilOp( GL_REPLACE, GL_REPLACE, GL_REPLACE );

    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    vectorial::mat4f modelViewProjection = vectorial::mat4f::ortho( 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f );

    int location = glGetUniformLocation( debug_shader, "ModelViewProjection" );
    if ( location >= 0 )
    {
        float values[16];
        modelViewProjection.store( values );
        glUniformMatrix4fv( location, 1, GL_FALSE, values );
    }

    glBindVertexArray( mask_vao );

    glDrawArrays( GL_TRIANGLES, 0, 6 );

    glBindVertexArray( 0 );

    glUseProgram( 0 );

    glDisable( GL_BLEND );

    glDisable( GL_STENCIL_TEST );

    check_opengl_error( "after shadow quad" );
}

void Render::EndScene()
{
    glDisable( GL_SCISSOR_TEST );
    glDisable( GL_BLEND );
}

// ========================================================================================================

// todo: remove this C++ bullshit!
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

void clear_opengl_error()
{
    while ( glGetError() != GL_NO_ERROR );
}

void check_opengl_error( const char * message )
{
    int error = glGetError();
    if ( error != GL_NO_ERROR )
    {
        printf( "opengl error: %s (%s)\n", gluErrorString( error ), message );
        exit( 1 );
    }    
}

uint32_t load_shader( const char * vertex_file_path, const char * fragment_file_path )
{
    printf( "loading shader \"%s\" \"%s\"\n", vertex_file_path, fragment_file_path );

    // Create the shaders
    GLuint VertexShaderID = glCreateShader( GL_VERTEX_SHADER );
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER );
 
    // Read the Vertex Shader code from the file
    std::string VertexShaderCode;
    std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
    if(VertexShaderStream.is_open())
    {
        std::string Line = "";
        while(getline(VertexShaderStream, Line))
            VertexShaderCode += "\n" + Line;
        VertexShaderStream.close();
    }
 
    // Read the Fragment Shader code from the file
    std::string FragmentShaderCode;
    std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
    if(FragmentShaderStream.is_open()){
        std::string Line = "";
        while(getline(FragmentShaderStream, Line))
            FragmentShaderCode += "\n" + Line;
        FragmentShaderStream.close();
    }
 
    GLint result = GL_FALSE;
    int InfoLogLength;
 
    // Compile Vertex Shader
    char const * VertexSourcePointer = VertexShaderCode.c_str();
    glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
    glCompileShader(VertexShaderID);
 
    // Check Vertex Shader
    glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    std::vector<char> VertexShaderErrorMessage(InfoLogLength);
    glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
    if ( result == GL_FALSE )
    {
        printf( "%s", &VertexShaderErrorMessage[0] );
        return 0;
    }
 
    // Compile Fragment Shader
    char const * FragmentSourcePointer = FragmentShaderCode.c_str();
    glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
    glCompileShader(FragmentShaderID);
 
    // Check Fragment Shader
    glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
    glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
    if ( result == GL_FALSE )
    {
        printf( "%s", &FragmentShaderErrorMessage[0] );
        return 0;
    }
 
    // Link the program
    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);
 
    // Check the program
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &result);
    glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
    glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
    if ( result == GL_FALSE )
    {
        printf( "%s", &ProgramErrorMessage[0] );
        exit( 1 );
        return 0;
    }
 
    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);
 
    return ProgramID;
}

void delete_shader( uint32_t shader )
{
    glDeleteShader( shader );
}
