// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "platform.h"
#include "protocol.h"
#include "network.h"
#include "game.h"
#include "world.h"
#include "render.h"
#include <stdio.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

Render render;

Camera camera;

struct Client
{
    Socket * socket = nullptr;
};

void client_init( Client & client )
{
     client.socket = new Socket( 0 );
}

void client_free( Client & client )
{
    delete client.socket;
    client = Client();    
}

struct Global
{
    int display_width;
    int display_height;
};

Global global;

void framebuffer_size_callback( GLFWwindow * window, int width, int height )
{
    global.display_width = width;
    global.display_height = height;
}

void client_tick( World & world, const Input & input )
{
//    printf( "%d-%d: %f [%+.4f]\n", (int) world.frame, (int) world.tick, world.time, TickDeltaTime );

    const int player_id = 0;

    game_process_player_input( world, input, player_id );

    world_tick( world );
}

void client_frame( World & world, const Input & input, double real_time, double frame_time )
{
    for ( int i = 0; i < TicksPerClientFrame; ++i )
        client_tick( world, input );

    world.frame++;
}

void client_clear()
{
    glViewport( 0, 0, global.display_width, global.display_height );
    glClearStencil( 0 );
    glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );     
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
}

void client_render( const World & world )
{
    client_clear();

    CubeEntity * player = (CubeEntity*) world.entity_manager->GetEntity( ENTITY_PLAYER_BEGIN );

    vec3f origin = player ? player->position : vec3f(0,0,0);

    vec3f lookat = origin - vec3f(0,0,1);

    vec3f position = lookat + vec3f(0,-11,5);

    camera.EaseIn( lookat, position );

    RenderState render_state;
    render_get_state( world, render_state );

    render.ResizeDisplay( global.display_width, global.display_height );

    render.BeginScene( 0, 0, global.display_width, global.display_height );

    render.SetCamera( camera.position, camera.lookat, camera.up );
    
    render.SetLightPosition( camera.lookat + vectorial::vec3f( 25.0f, -50.0f, 100.0f ) );

    render.RenderScene( render_state );
    
    render.RenderShadows( render_state );

    render.RenderShadowQuad();
    
    render.EndScene();
}

Input client_sample_input( GLFWwindow * window )
{
    assert( window );
    Input input;
    input.left = glfwGetKey( window, GLFW_KEY_LEFT );
    input.right = glfwGetKey( window, GLFW_KEY_RIGHT );
    input.up = glfwGetKey( window, GLFW_KEY_UP );
    input.down = glfwGetKey( window, GLFW_KEY_DOWN );
    input.push = glfwGetKey( window, GLFW_KEY_SPACE );
    input.pull = glfwGetKey( window, GLFW_KEY_Z );
    return input;
}

int client_main( int argc, char ** argv )
{
    Client client;

    client_init( client );

    glfwInit();

    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
    glfwWindowHint( GLFW_SRGB_CAPABLE, GL_TRUE );
    glfwWindowHint( GLFW_RESIZABLE, GL_FALSE );
    glfwWindowHint( GLFW_SAMPLES, 8 );
    glfwWindowHint( GLFW_STENCIL_BITS, 8 );
    glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE );

    const GLFWvidmode * mode = glfwGetVideoMode( glfwGetPrimaryMonitor() );

    GLFWwindow * window = nullptr;

    bool fullscreen = false;

    if ( fullscreen )
        window = glfwCreateWindow( mode->width, mode->height, "Client", glfwGetPrimaryMonitor(), nullptr );
    else
        window = glfwCreateWindow( 1000, 500, "Client", nullptr, nullptr );

    if ( !window )
    {
        printf( "error: failed to create window\n" );
        exit( 1 );
    }

    int window_width, window_height;
    glfwGetWindowSize( window, &window_width, &window_height );

    const GLFWvidmode * desktop_mode = glfwGetVideoMode( glfwGetPrimaryMonitor() );
    if ( !desktop_mode )
    {
        printf( "error: desktop mode is null!\n" );
        return 1;
    }

    const int desktop_width = desktop_mode->width;
    const int desktop_height = desktop_mode->height;

    glfwSetWindowPos( window, desktop_width / 2 - window_width / 2, desktop_height / 2 - window_height / 2 );

    glfwGetFramebufferSize( window, &global.display_width, &global.display_height );

    glfwSetFramebufferSizeCallback( window, framebuffer_size_callback );

    glfwMakeContextCurrent( window );

    glewExperimental = GL_TRUE;
    glewInit();

    clear_opengl_error();

    client_clear();

    double previous_frame_time = platform_time();

    World world;
    world_init( world );
    world_setup_cubes( world );

    glEnable( GL_FRAMEBUFFER_SRGB );

    glEnable( GL_CULL_FACE );
    glFrontFace( GL_CW );

    while ( true )
    {
        glfwSwapBuffers( window );

        double frame_start_time = platform_time();

        previous_frame_time = frame_start_time;

        Input input = client_sample_input( window );

        client_frame( world, input, frame_start_time, world.frame * ClientFrameDeltaTime );

        client_render( world );

        glfwPollEvents();

        if ( glfwWindowShouldClose( window ) )
            break;
    }

    world_free( world );

    client_free( client );

    glfwTerminate();

    return 0;
}

int main( int argc, char ** argv )
{
    client_main( argc, argv );

    return 0;
}
