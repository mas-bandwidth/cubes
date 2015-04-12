#include "render.h"
#include "world.h"
#include <stdio.h>
#include <assert.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

void render_get_state( const World & world, RenderState & render_state )
{
    render_state.num_cubes = 0;

    for ( int i = 0; i < MaxCubes; ++i )
    {
        if ( !world.cube_manager->allocated[i] )
            continue;

        CubeEntity & cube_entity = world.cube_manager->cubes[i];

        mat4f translation = mat4f::translation( cube_entity.position );
        mat4f rotation = mat4f::rotation( cube_entity.orientation );
        mat4f scale = mat4f::scale( cube_entity.scale * 0.5f );

        mat4f inv_translation = mat4f::translation( -cube_entity.position );
        mat4f inv_rotation = transpose( rotation );
        mat4f inv_scale = mat4f::scale( 1.0f / ( cube_entity.scale * 0.5f ) );

        RenderCube & render_cube = render_state.cube[render_state.num_cubes];

        render_cube.transform = translation * rotation * scale;
        render_cube.inverse_transform = inv_rotation * inv_translation * inv_scale;
        
        render_cube.r = 0.75f;    //object->r;
        render_cube.g = 0.75f;    //object->g;
        render_cube.b = 0.75f;    //object->b;
        render_cube.a = 1.0f;     //object->a;

        render_state.num_cubes++;
    }
}

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

    check_opengl_error( "before load shaders" );

    cubes_shader = load_shader( "shaders/cubes.vert", "shaders/cubes.frag" );
    shadow_shader = load_shader( "shaders/shadow.vert", "shaders/shadow.frag" );
    debug_shader = load_shader( "shaders/debug.vert", "shaders/debug.frag" );

    assert( cubes_shader );
    assert( shadow_shader );
    assert( debug_shader );

    if ( !shadow_shader || !cubes_shader || !debug_shader )
        return;

    check_opengl_error( "after load shaders" );

    if ( !cubes_shader )
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
                glVertexAttribPointer( model_view_location + i, 4, GL_FLOAT, GL_FALSE, sizeof( RenderCubeInstance ), (void*) ( offsetof( RenderCubeInstance, model_view ) + ( sizeof(float) * 4 * i ) ) );
                glVertexAttribDivisor( model_view_location + i, 1 );
            }

            if ( model_view_projection_location >= 0 )
            {
                glEnableVertexAttribArray( model_view_projection_location + i );
                glVertexAttribPointer( model_view_projection_location + i, 4, GL_FLOAT, GL_FALSE, sizeof( RenderCubeInstance ), (void*) ( offsetof( RenderCubeInstance, model_view_projection ) + ( sizeof(float) * 4 * i ) ) );
                glVertexAttribDivisor( model_view_projection_location + i, 1 );
            }
        }

        glBindBuffer( GL_ARRAY_BUFFER, 0 );

        glBindVertexArray( 0 );

        glUseProgram( 0 );
    }

    check_opengl_error( "after cube render setup" );

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
            glVertexAttribPointer( position_location, 3, GL_FLOAT, GL_FALSE, sizeof(vec3f), (GLubyte*)0 );
        }

        glBindBuffer( GL_ARRAY_BUFFER, 0 );

        glBindVertexArray( 0 );

        glUseProgram( 0 );
    }

    check_opengl_error( "after shadow render setup" );

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

    check_opengl_error( "after shadow mask render init" );
    
    initialized = true;
}

void Render::ResizeDisplay( int _display_width, int _display_height )
{
    display_width = _display_width;
    display_height = _display_height;
}

void Render::SetLightPosition( const vec3f & _light_position )
{
    light_position = _light_position;
}

void Render::SetCamera( const vec3f & position, const vec3f & lookat, const vec3f & up )
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
        
void Render::RenderScene( const RenderState & render_state )
{
    if ( render_state.num_cubes == 0 )
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

    mat4f view_matrix = mat4f::lookAt( camera_position, camera_lookat, camera_up );

    mat4f projection_matrix = mat4f::perspective( 40.0f, display_width / (float)display_height, 0.1f, 100.0f );

    for ( int i = 0; i < render_state.num_cubes; ++i )
    {
        RenderCubeInstance & instance = instance_data[i];
        instance.r = render_state.cube[i].r;
        instance.g = render_state.cube[i].g;
        instance.b = render_state.cube[i].b;
        instance.a = render_state.cube[i].a;
        instance.model = render_state.cube[i].transform;
        instance.model_view = view_matrix * instance.model;
        instance.model_view_projection = projection_matrix * instance.model_view;
    }

    glBindBuffer( GL_ARRAY_BUFFER, cubes_instance_buffer );
    glBufferData( GL_ARRAY_BUFFER, sizeof(RenderCubeInstance) * render_state.num_cubes, instance_data, GL_STREAM_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    glBindVertexArray( cubes_vao );

    glDrawElementsInstanced( GL_TRIANGLES, sizeof( cube_indices ) / 2, GL_UNSIGNED_SHORT, nullptr, render_state.num_cubes );

    glBindVertexArray( 0 );

    glUseProgram( 0 );

    check_opengl_error( "after render scene" );
}

inline void GenerateSilhoutteVerts( int & vertex_index,
                                    __restrict vec3f * vertices,
                                    const mat4f & transform,
                                    const vec3f & local_light,
                                    const vec3f & world_light,
                                    vec3f a, 
                                    vec3f b,
                                    const vec3f & left_normal,
                                    const vec3f & right_normal,
                                    bool left_dot,
                                    bool right_dot )
{
    // check if silhouette edge
    if ( left_dot ^ right_dot )
    {
        // ensure correct winding order for silhouette edge

        if ( dot( cross( b - a, local_light ), a ) < 0 )
        {
            vec3f tmp = a;
            a = b;
            b = tmp;
        }
        
        // transform into world space
        
        vec3f world_a = transformPoint( transform, a );
        vec3f world_b = transformPoint( transform, b );
        
        // extrude to ground plane z=0 in world space
        
        vec3f difference_a = world_a - world_light;
        vec3f difference_b = world_b - world_light;
        
        const float at = world_light.z() / difference_a.z();
        const float bt = world_light.z() / difference_b.z();
        
        vec3f extruded_a = world_light - difference_a * at;
        vec3f extruded_b = world_light - difference_b * bt;
        
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

static vec3f a(-1,+1,-1);
static vec3f b(+1,+1,-1);
static vec3f c(+1,+1,+1);
static vec3f d(-1,+1,+1);
static vec3f e(-1,-1,-1);
static vec3f f(+1,-1,-1);
static vec3f g(+1,-1,+1);
static vec3f h(-1,-1,+1);

static vec3f normals[6] =
{
    vec3f(+1,0,0),
    vec3f(-1,0,0),
    vec3f(0,+1,0),
    vec3f(0,-1,0),
    vec3f(0,0,+1),
    vec3f(0,0,-1)
};

void Render::RenderShadows( const RenderState & render_state )
{
    // generate shadow silhoutte vertices

    vec3f world_light = light_position;
    
    int vertex_index = 0;
    
    for ( int i = 0; i < (int) render_state.num_cubes; ++i )
    {
        const RenderCube & cube = render_state.cube[i];

        if ( cube.a < ShadowAlphaThreshold )
            continue;

        vec3f local_light = transformPoint( cube.inverse_transform, world_light );
        
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
    glBufferData( GL_ARRAY_BUFFER, sizeof( vec3f ) * vertex_index, shadow_vertices, GL_STREAM_DRAW );
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

    mat4f viewMatrix = mat4f::lookAt( camera_position, camera_lookat, camera_up );

    mat4f projectionMatrix = mat4f::perspective( 40.0f, display_width / (float)display_height, 0.1f, 100.0f );

    mat4f modelViewProjection = projectionMatrix * viewMatrix;

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

    mat4f modelViewProjection = mat4f::ortho( 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f );

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

bool load_text_file( const char * filename, char * buffer, int buffer_size )
{
    FILE * file = fopen( filename, "r" );
    if ( !file )
        return false;

    fseek( file, 0L, SEEK_END );
    uint64_t file_size = ftell( file );
    fseek( file, 0L, SEEK_SET );

    if ( file_size >= buffer_size )
    {
        fclose( file );
        return false;
    }

    fread( buffer, 1, file_size, file );

    fclose( file );

    buffer[file_size] = '\0';

    return true;
}

uint32_t load_shader( const char * filename, int type )
{
    GLuint shader_id = glCreateShader( type );

    static const int BufferSize = 256 * 1024;
    char buffer[BufferSize];
    if ( !load_text_file( filename, buffer, BufferSize ) )
    {
        printf( "error: failed to load shader %s\n", filename );
        return 0;
    }

    char const * source = buffer;
    glShaderSource( shader_id, 1, &source, NULL );
    glCompileShader( shader_id );
 
    GLint result = GL_FALSE;
    glGetShaderiv( shader_id, GL_COMPILE_STATUS, &result );
    if ( result == GL_FALSE )
    {
        int info_log_length;
        glGetShaderiv( shader_id, GL_INFO_LOG_LENGTH, &info_log_length );
        char info_log[info_log_length];
        glGetShaderInfoLog( shader_id, info_log_length, NULL, info_log );
        printf( "=================================================================\n"
                "failed to compile shader: %s\n"
                "=================================================================\n"
                "%s"
                "=================================================================\n", filename, info_log );
        glDeleteShader( shader_id );
        return 0;
    }

    return shader_id;
}

uint32_t load_shader( const char * vertex_file_path, const char * fragment_file_path )
{
    uint32_t vertex_shader_id = load_shader( vertex_file_path, GL_VERTEX_SHADER );
    if ( !vertex_shader_id )
    {
        return 0;
    }

    uint32_t fragment_shader_id = load_shader( fragment_file_path, GL_FRAGMENT_SHADER );
    if ( !fragment_shader_id )
    {
        glDeleteShader( vertex_shader_id );
        return 0;
    }

    GLuint program_id = glCreateProgram();
    glAttachShader( program_id, vertex_shader_id );
    glAttachShader( program_id, fragment_shader_id );
    glLinkProgram( program_id );
 
    GLint result = GL_FALSE;
    glGetProgramiv( program_id, GL_LINK_STATUS, &result );
    if ( result == GL_FALSE )
    {
        int info_log_length;
        glGetShaderiv( program_id, GL_INFO_LOG_LENGTH, &info_log_length );
        char info_log[info_log_length];
        glGetShaderInfoLog( program_id, info_log_length, NULL, info_log );
        printf( "=================================================================\n" \
                "failed to link shader: %s - %s\n"
                "=================================================================\n"
                "%s"
                "=================================================================\n", vertex_file_path, fragment_file_path, info_log );
        glDeleteShader( vertex_shader_id );
        glDeleteShader( fragment_shader_id );
        glDeleteProgram( program_id );
        return 0;
    }
 
    glDeleteShader( vertex_shader_id );
    glDeleteShader( fragment_shader_id );

    return program_id;
}

void delete_shader( uint32_t shader )
{
    glDeleteProgram( shader );
}
