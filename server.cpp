// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "protocol.h"
#include "network.h"
#include "packets.h"
#include "shared.h"
#include "world.h"
#include "game.h"
#include <stdio.h>
#include <signal.h>

enum ClientState
{
    CLIENT_DISCONNECTED,
    CLIENT_CONNECTING,
    CLIENT_CONNECTED
};

struct SyncData
{
    bool synchronizing = false;
    uint16_t sequence = 0;
    int num_samples = 0;
    int offset = 0;
    uint64_t previous_tick = 0;
};

struct InputEntry
{
    uint64_t tick = 0;
    Input input;
};

struct InputData
{
    uint64_t most_recent_input = 0;
    InputEntry inputs[InputSlidingWindowSize];
};

struct Server
{
    Socket * socket = nullptr;

    uint64_t tick = 0;

    uint64_t client_guid[MaxClients];

    Address client_address[MaxClients];

    ClientState client_state[MaxClients];

    double current_real_time;

    double client_time_last_packet_received[MaxClients];

    SyncData client_sync_data[MaxClients];

    InputData client_input_data[MaxClients];
};

void server_init( Server & server )
{
    server.socket = new Socket( ServerPort );

    for ( int i = 0; i < MaxClients; ++i )
    {
        server.client_guid[i] = 0;
        server.client_state[i] = CLIENT_DISCONNECTED;
        server.client_time_last_packet_received[i] = 0.0;
    }

    server.current_real_time = 0.0;

    printf( "server listening on port %d\n", server.socket->GetPort() );
}

void server_update( Server & server, uint64_t tick, double current_real_time )
{
    server.tick = tick;
    server.current_real_time = current_real_time;

    for ( int i = 0; i < MaxClients; ++i )
    {
        if ( server.client_state[i] != CLIENT_DISCONNECTED )
        {
            if ( current_real_time > server.client_time_last_packet_received[i] + Timeout )
            {
                char buffer[256];
                printf( "client %d timed out %s\n", i, server.client_address[i].ToString( buffer, sizeof( buffer ) ) );
                server.client_state[i] = CLIENT_DISCONNECTED;
                server.client_address[i] = Address();
                server.client_guid[i] = 0;
                server.client_time_last_packet_received[i] = 0.0;
                server.client_sync_data[i] = SyncData();
                server.client_input_data[i] = InputData();
            }
        }
    }
}

int server_find_client_slot( const Server & server, const Address & from, uint64_t client_guid )
{
    for ( int i = 0; i < MaxClients; ++i )
    {
        if ( server.client_guid[i] == client_guid && from == server.client_address[i] )
            return i;
    }
    return -1;
}

int server_find_client_slot( const Server & server, const Address & from )
{
    for ( int i = 0; i < MaxClients; ++i )
    {
        if ( from == server.client_address[i] )
            return i;
    }
    return -1;
}

int server_find_free_slot( const Server & server )
{
    for ( int i = 0; i < MaxClients; ++i )
    {
        if ( server.client_state[i] == CLIENT_DISCONNECTED )
            return i;
    }
    return -1;
}

void server_send_packet( Server & server, const Address & address, Packet & packet )
{
    uint8_t buffer[MaxPacketSize];
    int packet_bytes = 0;
    WriteStream stream( buffer, MaxPacketSize );
    if ( write_packet( stream, packet, packet_bytes ) )
    {
        if ( server.socket->SendPacket( address, buffer, packet_bytes ) && packet.type )
        {
            /*
            char buffer[256];
            printf( "sent %s packet to client %s\n", packet_type_string( packet.type ), address.ToString( buffer, sizeof( buffer ) ) );
            */
        }
    }
}

void server_send_packets( Server & server )
{
    for ( int i = 0; i < MaxClients; ++i )
    {
        if ( server.client_state[i] == CLIENT_CONNECTED )
        {
            SnapshotPacket packet;
            packet.type = PACKET_TYPE_SNAPSHOT;
            packet.tick = server.tick;
            packet.synchronizing = server.client_sync_data[i].synchronizing;
            if ( packet.synchronizing )
            {
                packet.sync_offset = server.client_sync_data[i].offset;
                packet.sync_sequence = server.client_sync_data[i].sequence;
            }
            else
            {
                packet.input_ack = server.client_input_data[i].most_recent_input;
            }
            server_send_packet( server, server.client_address[i], packet );
        }
    }
}

bool process_packet( const Address & from, Packet & base_packet, void * context )
{
    Server & server = *(Server*)context;

    /*
    char buffer[256];
    printf( "received %s packet from %s\n", packet_type_string( base_packet.type ), from.ToString( buffer, sizeof( buffer ) ) );
    */

    switch ( base_packet.type )
    {
        case PACKET_TYPE_CONNECTION_REQUEST:
        {
            // is the client already connected?
            ConnectionRequestPacket & packet = (ConnectionRequestPacket&) base_packet;
            int client_slot = server_find_client_slot( server, from, packet.client_guid );
            if ( client_slot == -1 )
            {
                // is there a free client slot?
                client_slot = server_find_free_slot( server );
                if ( client_slot != -1 )
                {
                    char buffer[256];
                    printf( "client %d connecting %s\n", client_slot, from.ToString( buffer, sizeof( buffer ) ) );

                    // connect client
                    server.client_state[client_slot] = CLIENT_CONNECTING;
                    server.client_guid[client_slot] = packet.client_guid;
                    server.client_address[client_slot] = from;
                    server.client_time_last_packet_received[client_slot] = server.current_real_time;
                    server.client_sync_data[client_slot] = SyncData();
                    server.client_sync_data[client_slot].synchronizing = true;

                    // send connection accepted resonse
                    ConnectionAcceptedPacket response;
                    response.type = PACKET_TYPE_CONNECTION_ACCEPTED;
                    response.client_guid = packet.client_guid;
                    server_send_packet( server, from, response );
                    return true;
                }
                else
                {
                    // no free client slots. send connection denied response
                    ConnectionDeniedPacket response;
                    response.type = PACKET_TYPE_CONNECTION_DENIED;
                    response.client_guid = packet.client_guid;
                    server_send_packet( server, from, response );
                    return true;
                }
            }
            else
            {
                if ( server.client_state[client_slot] == CLIENT_CONNECTING )
                {
                    // we must reply with connection accepted because packets are unreliable
                    ConnectionAcceptedPacket response;
                    response.type = PACKET_TYPE_CONNECTION_ACCEPTED;
                    response.client_guid = packet.client_guid;
                    server_send_packet( server, from, response );
                    return true;
                }
            }
        }
        break;

        case PACKET_TYPE_INPUT:
        {
            InputPacket & packet = (InputPacket&) base_packet;
            int client_slot = server_find_client_slot( server, from );
            if ( client_slot != -1 )
            {
                if ( server.client_state[client_slot] == CLIENT_CONNECTING )
                {
                    char buffer[256];
                    printf( "client %d connected %s\n", client_slot, from.ToString( buffer, sizeof( buffer ) ) );
                    server.client_state[client_slot] = CLIENT_CONNECTED;
                }

                if ( packet.synchronizing && server.client_sync_data[client_slot].synchronizing &&
                     packet.sync_sequence == server.client_sync_data[client_slot].sequence )
                {
                    uint64_t oldest_input_tick = packet.tick;
                    
                    if ( server.client_sync_data[client_slot].num_samples > 0 )
                        oldest_input_tick = server.client_sync_data[client_slot].previous_tick + 1;

                    server.client_sync_data[client_slot].previous_tick = packet.tick;

                    const int offset = (int) ( server.tick + ( TicksPerServerFrame - 1 ) - ( TicksPerClientFrame - 1 ) - oldest_input_tick );

//                    printf( "%d - %d (%d) = %d\n", (int) server.tick, (int) oldest_input_tick, (int) packet.tick, offset );
                    
                    server.client_sync_data[client_slot].num_samples++;
                    server.client_sync_data[client_slot].offset = max( offset, server.client_sync_data[client_slot].offset );
                    
                    if ( server.client_sync_data[client_slot].num_samples > MaxSyncSamples && 
                         server.client_sync_data[client_slot].offset == packet.sync_offset )
                    {
                        printf( "client %d synchronized [%d]\n", client_slot, server.client_sync_data[client_slot].offset );
                        server.client_sync_data[client_slot].synchronizing = false;
                        server.client_sync_data[client_slot].sequence++;
                    }
                }

                if ( !packet.synchronizing && !server.client_sync_data[client_slot].synchronizing )
                {
                    if ( packet.tick > server.client_input_data[client_slot].most_recent_input )
                    {
                        server.client_input_data[client_slot].most_recent_input = packet.tick;

                        for ( int i = 0; i < packet.num_inputs; ++i )
                        {
                            uint64_t input_tick = packet.tick - i;
                            const int index = input_tick % InputSlidingWindowSize;
                            server.client_input_data[client_slot].inputs[index].tick = input_tick;
                            server.client_input_data[client_slot].inputs[index].input = packet.input[i];
                        }
                    }
                }

                server.client_time_last_packet_received[client_slot] = server.current_real_time;

                return true;
            }
        }
        break;

        default:
            break;
    }

    return false;
}

void server_receive_packets( Server & server )
{
    uint8_t buffer[MaxPacketSize];
    while ( true )
    {
        Address from;
        int bytes_read = server.socket->ReceivePacket( from, buffer, sizeof( buffer ) );
        if ( bytes_read == 0 )
            break;
//        char address_buffer[256];
//        printf( "received packet from %s\n", from.ToString( address_buffer, sizeof( address_buffer ) ) );
        read_packet( from, buffer, bytes_read, &server );
    }
}

void server_get_client_input( Server & server, int client_slot, uint64_t tick, Input * inputs, int num_inputs )
{
    assert( client_slot >= 0 );
    assert( client_slot < MaxClients );

    if ( server.client_state[client_slot] != CLIENT_CONNECTED || server.client_sync_data[client_slot].synchronizing )
        return;

    for ( int i = 0; i < num_inputs; ++i )
    {
        uint64_t input_tick = tick + i;
        const int index = input_tick % InputSlidingWindowSize;
        if ( server.client_input_data[client_slot].inputs[index].tick == input_tick )
        {
            inputs[i] = server.client_input_data[client_slot].inputs[index].input;
        }
        else
        {
            printf( "client %d dropped input %d\n", client_slot, (int) input_tick );

            // todo: might want to bump a counter here. too many dropped inputs = tell client to reconnect

            // todo: also a time value. if no dropped inputs for n seconds, clear dropped input count back to zero.
        }
    }

    // lets measure exactly how far ahead the client is delivering inputs

    int num_ticks_ahead = 0;
    while ( true )
    {
        uint64_t input_tick = tick + num_inputs + num_ticks_ahead;
        const int index = input_tick % InputSlidingWindowSize;
        if ( server.client_input_data[client_slot].inputs[index].tick != input_tick )
            break;
        num_ticks_ahead++;
    }
//    printf( "client %d is delivering input %d ticks early\n", client_slot, num_ticks_ahead );
}

void server_free( Server & server )
{
    printf( "shutting down\n" );
    delete server.socket;
    server = Server();
}

void server_tick( World & world, const Input & input )
{
//    printf( "%d-%d: %f [%+.4f]\n", (int) world.frame, (int) world.tick, world.time, TickDeltaTime );

    game_process_player_input( world, input, 0 );

    world_tick( world );
}

void server_frame( World & world, double real_time, double frame_time, double jitter, const Input * inputs )
{
    //printf( "%d: %f [%+.2fms]\n", (int) frame, real_time, jitter * 1000 );
    
    for ( int i = 0; i < TicksPerServerFrame; ++i )
    {
        server_tick( world, inputs[i] );
    }
}

static volatile int quit = 0;

void interrupt_handler( int dummy )
{
    quit = 1;
}

int server_main( int argc, char ** argv )
{
    printf( "starting server\n" );

    InitializeNetwork();

    Server server;

    server_init( server );

    World world;
    world_init( world );
    world_setup_cubes( world );

    const double start_time = platform_time();

    double previous_frame_time = start_time;
    double next_frame_time = previous_frame_time + ServerFrameDeltaTime;

    signal( SIGINT, interrupt_handler );

    while ( !quit )
    {
        const double time_to_sleep = max( 0.0, next_frame_time - platform_time() - AverageSleepJitter );

        platform_sleep( time_to_sleep );

        const double real_time = platform_time();

        const double frame_time = next_frame_time;

        const double jitter = real_time - frame_time;

        server_update( server, world.tick, real_time );

        server_receive_packets( server );

        server_send_packets( server );

        Input inputs[TicksPerServerFrame];
        server_get_client_input( server, 0, world.tick, inputs, TicksPerServerFrame );

        server_frame( world, real_time, frame_time, jitter, inputs );

        const double end_of_frame_time = platform_time();

        int num_frames_advanced = 0;
        while ( next_frame_time < end_of_frame_time + ServerFrameSafety * ServerFrameDeltaTime )
        {
            next_frame_time += ServerFrameDeltaTime;
            num_frames_advanced++;
        }

        const int num_dropped_frames = max( 0, num_frames_advanced - 1 );

        if ( num_dropped_frames > 0 )
        {
            // todo: would be nice to print out length of previous frame in milliseconds x/y here

            printf( "dropped frame %d (%d)\n", (int) world.frame, num_dropped_frames );
        }

        previous_frame_time = next_frame_time - ServerFrameDeltaTime;

        world.frame++;
    }

    printf( "\n" );

    world_free( world );

    server_free( server );

    ShutdownNetwork();

    return 0;
}

int main( int argc, char ** argv )
{
    return server_main( argc, argv ); 
}
