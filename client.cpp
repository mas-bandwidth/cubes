// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "protocol.h"
#include "platform.h"
#include "entity.h"
#include "cubes.h"
#include "game.h"
#include "render.h"
#include <stdio.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

struct Global
{
    int displayWidth;
    int displayHeight;
};

Global global;

void framebuffer_size_callback( GLFWwindow * window, int width, int height )
{
    global.displayWidth = width;
    global.displayHeight = height;

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

void client_frame( uint64_t frame, uint64_t tick, double real_time, double frame_time )
{
    printf( "%llu: %f\n", frame, real_time );
}

void client_clear()
{
    glViewport( 0, 0, global.displayWidth, global.displayHeight );
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

    glfwGetFramebufferSize( window, &global.displayWidth, &global.displayHeight );

    glfwSetFramebufferSizeCallback( window, framebuffer_size_callback );

    glfwMakeContextCurrent( window );

    glfwSetKeyCallback( window, key_callback );
    glfwSetCharCallback( window, char_callback );

    glewExperimental = GL_TRUE;
    glewInit();

    uint64_t frame = 0;
    uint64_t tick = 0;

    double previous_frame_time = platform_time();

    client_clear();

    while ( true )
    {
        glfwSwapBuffers( window );

        double frame_start_time = platform_time();

        previous_frame_time = frame_start_time;

        client_frame( tick, frame, frame * ClientFrameDeltaTime, frame_start_time );

        client_render();

        glfwPollEvents();

        if ( glfwWindowShouldClose( window ) )
            break;

        tick += TicksPerClientFrame;

        frame++;
    }

    glfwTerminate();

    return 0;
}

int main( int argc, char ** argv )
{
    client_main( argc, argv );

    return 0;
}
