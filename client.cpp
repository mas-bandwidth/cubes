// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "protocol.h"
#include "platform.h"
#include "game.h"
#include "world.h"
#include "render.h"
#include <stdio.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

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

    glViewport( 0, 0, width, height );
}

void key_callback( GLFWwindow * window, int key, int scancode, int action, int mods )
{
    // ...
}

void char_callback( GLFWwindow * window, unsigned int code )
{
    // ...
}

void client_tick( World & world )
{
    printf( "%llu-%llu: %f [%+.4f]\n", world.frame, world.tick, world.time, TickDeltaTime );

    world_tick( world );
}

void client_frame( World & world, double real_time, double frame_time )
{
    for ( int i = 0; i < TicksPerClientFrame; ++i )
    {
        client_tick( world );
        world.time += TickDeltaTime;
        world.tick++;
    }

    world.frame++;
}

void client_clear()
{
    glViewport( 0, 0, global.display_width, global.display_height );
    glClearStencil( 0 );
    glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );     
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
}

void client_render()
{
    client_clear();

    // ...
}

int client_main( int argc, char ** argv )
{
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

    glfwSetKeyCallback( window, key_callback );
    glfwSetCharCallback( window, char_callback );

    glewExperimental = GL_TRUE;
    glewInit();

    client_clear();

    double previous_frame_time = platform_time();

    World world;
    world_init( world );
    world_setup_cubes( world );

    while ( true )
    {
        glfwSwapBuffers( window );

        double frame_start_time = platform_time();

        previous_frame_time = frame_start_time;

        client_frame( world, frame_start_time, world.frame * ClientFrameDeltaTime );

        client_render();

        glfwPollEvents();

        if ( glfwWindowShouldClose( window ) )
            break;
    }

    world_free( world );

    glfwTerminate();

    return 0;
}

int main( int argc, char ** argv )
{
    client_main( argc, argv );

    return 0;
}
