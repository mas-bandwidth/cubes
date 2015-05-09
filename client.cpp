// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "platform.h"
#include "protocol.h"
#include "packets.h"
#include "network.h"
#include "game.h"
#include "world.h"
#include "render.h"
#include <stdio.h>

#define HEADLESS 1

#if !HEADLESS
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#endif // #if !HEADLESS

Render render;

Camera camera;

enum ClientState
{
    CLIENT_DISCONNECTED,
    CLIENT_SENDING_CONNECT_REQUEST,
    CLIENT_TIMED_OUT,
    CLIENT_CONNECTED
};

struct Client
{
    Socket * socket;
    uint64_t guid;
    ClientState state;
    Address server_address;
    
    double current_real_time;
    double time_last_packet_received;
    uint64_t client_tick;
    uint64_t server_tick;

    bool synchronizing;
    bool ready_to_apply_sync;
    uint16_t sync_sequence;
    uint16_t sync_offset;

    // todo: need sliding window of inputs indexed by tick above
};

void client_init( Client & client )
{
    client.socket = new Socket( 0 );
    client.guid = rand();
    client.state = CLIENT_DISCONNECTED;
    client.current_real_time = 0.0;
    client.time_last_packet_received = 0.0;
    client.client_tick = 0;
    client.server_tick = 0;
    client.synchronizing = false;
    client.ready_to_apply_sync = false;
    client.sync_sequence = 0;
    client.sync_offset = 0;
}

void client_connect( Client & client, const Address & address, double current_real_time )
{
    char buffer[256];
    printf( "client connecting to %s\n", address.ToString( buffer, sizeof( buffer ) ) );
    client.state = CLIENT_SENDING_CONNECT_REQUEST;
    client.server_address = address;
    client.current_real_time = current_real_time;
    client.time_last_packet_received = current_real_time;
}

void client_disconnect( Client & client )
{
    client.state = CLIENT_DISCONNECTED;
}

void client_update( Client & client, double current_real_time )
{
    client.current_real_time = current_real_time;
    if ( client.state != CLIENT_DISCONNECTED && client.state != CLIENT_TIMED_OUT )
    {
        if ( current_real_time > client.time_last_packet_received + Timeout )
        {
            printf( "client timed out\n" );
            client.state = CLIENT_TIMED_OUT;
        }
    }
}

void client_send_packet( Client & client, Packet & packet )
{
    uint8_t buffer[MaxPacketSize];
    int packet_bytes = 0;
    WriteStream stream( buffer, MaxPacketSize );
    if ( write_packet( stream, packet, packet_bytes ) )
    {
        if ( client.socket->SendPacket( client.server_address, buffer, packet_bytes ) )
        {
            /*
            char address_buffer[1024];
            printf( "sent %s packet to server %s\n", packet_type_string( packet.type ), client.server_address.ToString( address_buffer, sizeof( address_buffer ) ) );
            */
        }
    }
}

void client_send_packets( Client & client )
{
    switch ( client.state )
    {
        case CLIENT_SENDING_CONNECT_REQUEST:
        {
            ConnectionRequestPacket packet;
            packet.type = PACKET_TYPE_CONNECTION_REQUEST;
            packet.client_guid = client.guid;
            client_send_packet( client, packet );
        }
        break;

        case CLIENT_CONNECTED:
        {
            InputPacket packet;
            packet.type = PACKET_TYPE_INPUT;
            packet.synchronizing = client.synchronizing;
            if ( client.synchronizing )
            {
                packet.sync_offset = client.sync_offset;
                packet.sync_sequence = client.sync_sequence;
                packet.tick = client.server_tick;
            }
            else
            {
                packet.tick = client.client_tick;
                packet.num_inputs = 1;
                // todo: we need access to sliding window of inputs for this client
            }
            client_send_packet( client, packet );
        }
        break;

        default:
            break;
    }
}

bool process_packet( const Address & from, Packet & base_packet, void * context )
{
    Client & client = *(Client*)context;

    switch ( base_packet.type )
    {
        case PACKET_TYPE_CONNECTION_ACCEPTED:
        {
            ConnectionAcceptedPacket & packet = (ConnectionAcceptedPacket&) base_packet;
            if ( packet.client_guid == client.guid && client.state == CLIENT_SENDING_CONNECT_REQUEST )
            {
                printf( "client connected\n" );
                client.state = CLIENT_CONNECTED;
                return true;
            }
        }
        break;

        case PACKET_TYPE_CONNECTION_DENIED:
        {
            ConnectionDeniedPacket & packet = (ConnectionDeniedPacket&) base_packet;
            if ( packet.client_guid == client.guid && client.state == CLIENT_SENDING_CONNECT_REQUEST )
            {
                printf( "connection denied\n" );
                client.state = CLIENT_DISCONNECTED;
                return true;
            }
        }
        break;

        case PACKET_TYPE_SNAPSHOT:
        {
            SnapshotPacket & packet = (SnapshotPacket&) base_packet;
            if ( packet.tick > client.server_tick )
            {
                if ( !client.synchronizing && packet.synchronizing )
                {
                    printf( "synchronizing\n" );
                    client.synchronizing = true;
                }
                
                if ( client.synchronizing )
                {
                    if ( !packet.synchronizing && !client.ready_to_apply_sync )
                    {
                        client.ready_to_apply_sync = true;
                    }
                    else
                    {
                        client.server_tick = packet.tick;
                        client.sync_offset = packet.sync_offset;
                        client.sync_sequence = packet.sync_sequence;
                    }
                }
            }
            return true;
        }
        break;

        default:
            break;
    }

    return false;
}

void client_receive_packets( Client & client )
{
    uint8_t buffer[MaxPacketSize];

    while ( true )
    {
        Address from;
        int bytes_read = client.socket->ReceivePacket( from, buffer, sizeof( buffer ) );
        if ( bytes_read == 0 )
            break;

        if ( read_packet( from, buffer, bytes_read, &client ) )
            client.time_last_packet_received = client.current_real_time;
    }
}

void client_apply_time_synchronization( Client & client, World & world )
{
    if ( client.synchronizing && client.ready_to_apply_sync )
    {
        printf( "synchronized [%d]\n", (int) client.sync_offset );
        world.tick = client.server_tick + client.sync_offset;
        client.client_tick = world.tick;
        client.synchronizing = false;
        client.ready_to_apply_sync = false;
    }
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

void client_post_frame( Client & client, World & world )
{
    client.client_tick = world.tick;
}

#if !HEADLESS

void framebuffer_size_callback( GLFWwindow * window, int width, int height )
{
    global.display_width = width;
    global.display_height = height;
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
    InitializeNetwork();

    Client client;

    client_init( client );

    World world;
    world_init( world );
    world_setup_cubes( world );

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

    glEnable( GL_FRAMEBUFFER_SRGB );

    glEnable( GL_CULL_FACE );
    glFrontFace( GL_CW );

    double previous_frame_time = platform_time();

    while ( true )
    {
        if ( client.state == CLIENT_TIMED_OUT )
            break;

        glfwSwapBuffers( window );

        double frame_start_time = platform_time();

        previous_frame_time = frame_start_time;

        Input input = client_sample_input( window );

        client_update( client, frame_start_time );

        client_receive_packets( client );

        client_apply_time_synchronization( client, world );

        client_send_packets( client );

        client_frame( world, input, frame_start_time, world.frame * ClientFrameDeltaTime );

        client_post_frame( client, world );

        client_render( world );

        glfwPollEvents();

        if ( glfwWindowShouldClose( window ) )
            break;
    }

    world_free( world );

    client_free( client );

    glfwTerminate();

    ShutdownNetwork();

    return 0;
}

#else // #if !HEADLESS

static volatile int quit = 0;

void interrupt_handler( int dummy )
{
    quit = 1;
}

int client_main( int argc, char ** argv )
{
    InitializeNetwork();

    Client client;

    client_init( client );

    World world;
    world_init( world );
    world_setup_cubes( world );

    signal( SIGINT, interrupt_handler );

    client_connect( client, Address( "::1", ServerPort ), 0.0 );

    const double start_time = platform_time();

    double previous_frame_time = start_time;
    double next_frame_time = previous_frame_time + ClientFrameDeltaTime;

    while ( !quit )
    {
        if ( client.state == CLIENT_TIMED_OUT )
            break;

        const double time_to_sleep = max( 0.0, next_frame_time - platform_time() - AverageSleepJitter );

        platform_sleep( time_to_sleep );

        const double frame_time = next_frame_time;

        Input input;

        client_update( client, frame_time );

        client_receive_packets( client );

        client_apply_time_synchronization( client, world );

        client_send_packets( client );

        client_frame( world, input, frame_time, world.frame * ClientFrameDeltaTime );

        client_post_frame( client, world );

        const double end_of_frame_time = platform_time();

        int num_frames_advanced = 0;
        while ( next_frame_time < end_of_frame_time + ClientFrameSafety * ClientFrameDeltaTime )
        {
            next_frame_time += ClientFrameDeltaTime;
            num_frames_advanced++;
        }

        const int num_dropped_frames = max( 0, num_frames_advanced - 1 );

        if ( num_dropped_frames > 0 && client.state == CLIENT_CONNECTED && !client.synchronizing )
        {
            printf( "dropped frame %d (%d)\n", (int) world.frame, num_dropped_frames );
        }

        previous_frame_time = next_frame_time - ClientFrameDeltaTime;

        world.frame++;
    }

    printf( "\n" );

    world_free( world );

    client_free( client );

    ShutdownNetwork();

    return 0;
}

#endif // #if !HEADLESS

int main( int argc, char ** argv )
{
    client_main( argc, argv );

    return 0;
}
