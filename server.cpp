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

struct BracketData
{
    bool bracketing = false;
    int num_samples = 0;
    int offset = 0;
    bool bracketed = false;
};

struct AdjustmentData
{
    bool reconnect = false;
    int num_dropped_inputs = 0;
    double time_last_dropped_input = 0.0;
    uint16_t sequence = 0;
    int min_ticks_ahead = 0;
    int num_samples = 0;
    int offset = 0;
    uint64_t first_input = 0;
};  

struct InputEntry
{
    uint64_t tick = 0;
    Input input;
};

struct InputData
{
    uint64_t first_input = 0;
    uint64_t most_recent_input = 0;
    InputEntry inputs[InputSlidingWindowSize];
};

struct Server
{
    Socket * socket = nullptr;

    uint64_t tick = 0;

    uint64_t client_guid[MaxClients];

    uint16_t client_connect_sequence[MaxClients];

    Address client_address[MaxClients];

    ClientState client_state[MaxClients];

    double current_real_time;

    double client_time_last_packet_received[MaxClients];

    SyncData client_sync_data[MaxClients];

    BracketData client_bracket_data[MaxClients];

    AdjustmentData client_adjustment_data[MaxClients];

    InputData client_input_data[MaxClients];
};

void server_init( Server & server )
{
    server.socket = new Socket( ServerPort );

    for ( int i = 0; i < MaxClients; ++i )
    {
        server.client_guid[i] = 0;
        server.client_connect_sequence[i] = 0;
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
                printf( "client %d timed out %s (%d)\n", i, server.client_address[i].ToString( buffer, sizeof( buffer ) ), server.client_connect_sequence[i] );
                server.client_state[i] = CLIENT_DISCONNECTED;
                server.client_address[i] = Address();
                server.client_guid[i] = 0;
                server.client_connect_sequence[i] = 0;
                server.client_time_last_packet_received[i] = 0.0;
                server.client_sync_data[i] = SyncData();
                server.client_bracket_data[i] = BracketData();
                server.client_adjustment_data[i] = AdjustmentData();
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
            }
            else
            {
                packet.reconnect = server.client_adjustment_data[i].reconnect;
                packet.bracketing = server.client_bracket_data[i].bracketing;
                packet.bracket_offset = server.client_bracket_data[i].offset;
                packet.adjustment_sequence = server.client_adjustment_data[i].sequence;
                packet.adjustment_offset = server.client_adjustment_data[i].offset;
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
                    printf( "client %d connecting %s (%d)\n", client_slot, from.ToString( buffer, sizeof( buffer ) ), packet.connect_sequence );

                    // connect client
                    server.client_state[client_slot] = CLIENT_CONNECTING;
                    server.client_guid[client_slot] = packet.client_guid;
                    server.client_connect_sequence[client_slot] = packet.connect_sequence;
                    server.client_address[client_slot] = from;
                    server.client_time_last_packet_received[client_slot] = server.current_real_time;
                    server.client_input_data[client_slot] = InputData();
                    server.client_sync_data[client_slot] = SyncData();
                    server.client_bracket_data[client_slot] = BracketData();
                    server.client_adjustment_data[client_slot] = AdjustmentData();
                    server.client_sync_data[client_slot].synchronizing = true;

                    // send connection accepted resonse
                    ConnectionAcceptedPacket response;
                    response.type = PACKET_TYPE_CONNECTION_ACCEPTED;
                    response.client_guid = packet.client_guid;
                    response.connect_sequence = packet.connect_sequence;
                    server_send_packet( server, from, response );
                    return true;
                }
                else
                {
                    // no free client slots. send connection denied response
                    ConnectionDeniedPacket response;
                    response.type = PACKET_TYPE_CONNECTION_DENIED;
                    response.client_guid = packet.client_guid;
                    response.connect_sequence = packet.connect_sequence;
                    server_send_packet( server, from, response );
                    return true;
                }
            }
            else
            {
                if ( packet.connect_sequence == server.client_connect_sequence[client_slot] )
                {
                    if ( server.client_state[client_slot] == CLIENT_CONNECTING )
                    {
                        // we must reply with connection accepted because packets are unreliable
                        ConnectionAcceptedPacket response;
                        response.type = PACKET_TYPE_CONNECTION_ACCEPTED;
                        response.client_guid = packet.client_guid;
                        response.connect_sequence = packet.connect_sequence;
                        server.client_time_last_packet_received[client_slot] = server.current_real_time;
                        server_send_packet( server, from, response );
                        return true;
                    }
                }
                else if ( sequence_greater_than( packet.connect_sequence, server.client_connect_sequence[client_slot] ) )
                {
                    char buffer[256];
                    printf( "client %d reconnecting %s (%d)\n", client_slot, from.ToString( buffer, sizeof( buffer ) ), packet.connect_sequence );

                    // todo: this should be a function -- the code is completely common with initial connect

                    // eg: server_connect_client( ... )

                    // client reconnect
                    server.client_state[client_slot] = CLIENT_CONNECTING;
                    server.client_guid[client_slot] = packet.client_guid;
                    server.client_connect_sequence[client_slot] = packet.connect_sequence;
                    server.client_address[client_slot] = from;
                    server.client_time_last_packet_received[client_slot] = server.current_real_time;
                    server.client_input_data[client_slot] = InputData();
                    server.client_sync_data[client_slot] = SyncData();
                    server.client_bracket_data[client_slot] = BracketData();
                    server.client_adjustment_data[client_slot] = AdjustmentData();
                    server.client_sync_data[client_slot].synchronizing = true;

                    // send connection accepted resonse
                    ConnectionAcceptedPacket response;
                    response.type = PACKET_TYPE_CONNECTION_ACCEPTED;
                    response.client_guid = packet.client_guid;
                    response.connect_sequence = packet.connect_sequence;
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
                    printf( "client %d connected %s (%d)\n", client_slot, from.ToString( buffer, sizeof( buffer ) ), server.client_connect_sequence[client_slot] );
                    server.client_state[client_slot] = CLIENT_CONNECTED;
                }

                if ( packet.synchronizing && server.client_sync_data[client_slot].synchronizing &&
                     packet.sync_sequence == server.client_sync_data[client_slot].sequence )
                {
                    uint64_t oldest_input_tick = packet.tick;
                    
                    if ( server.client_sync_data[client_slot].num_samples > 0 )
                        oldest_input_tick = server.client_sync_data[client_slot].previous_tick + 1;

                    server.client_sync_data[client_slot].previous_tick = packet.tick;

                    const int offset = max( 0, (int) ( server.tick + TicksPerServerFrame - oldest_input_tick ) );

//                    printf( "%d - %d (%d) = %d\n", (int) server.tick, (int) oldest_input_tick, (int) packet.tick, offset );
                    
                    server.client_sync_data[client_slot].num_samples++;
                    server.client_sync_data[client_slot].offset = max( offset, server.client_sync_data[client_slot].offset );
                    
                    if ( server.client_sync_data[client_slot].num_samples > MaxSyncSamples && 
                         server.client_sync_data[client_slot].offset == packet.sync_offset )
                    {
                        printf( "client %d synchronized [+%d]\n", client_slot, server.client_sync_data[client_slot].offset );
                        server.client_sync_data[client_slot].synchronizing = false;
                        server.client_sync_data[client_slot].sequence++;
                        server.client_bracket_data[client_slot].bracketing = true;
                    }
                }

                if ( !packet.synchronizing && !server.client_sync_data[client_slot].synchronizing &&
                     ( ( !packet.bracketed && server.client_bracket_data[client_slot].bracketing ) ||
                       (  packet.bracketed && server.client_bracket_data[client_slot].bracketed ) ) )
                {
                    if ( packet.num_inputs > 0 && packet.tick > server.client_input_data[client_slot].most_recent_input )
                    {
                        const uint64_t oldest_input_in_packet = packet.tick - ( packet.num_inputs - 1 );

                        if ( server.client_input_data[client_slot].first_input == 0 )
                        {
                            server.client_input_data[client_slot].first_input = oldest_input_in_packet;
                        }

                        if ( server.client_adjustment_data[client_slot].first_input == 0 && 
                             server.client_adjustment_data[client_slot].sequence == packet.adjustment_sequence )
                        {
                            server.client_adjustment_data[client_slot].first_input = oldest_input_in_packet;
                            server.client_adjustment_data[client_slot].num_samples = 0;
                            server.client_adjustment_data[client_slot].min_ticks_ahead = 0;
                        }

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

void server_get_client_input( Server & server, int client_slot, uint64_t tick, Input * inputs, int num_inputs, double real_time )
{
    assert( client_slot >= 0 );
    assert( client_slot < MaxClients );

    if ( server.client_adjustment_data[client_slot].reconnect )
        return;

    if ( server.client_state[client_slot] != CLIENT_CONNECTED )
        return;

    if ( server.client_sync_data[client_slot].synchronizing )
        return;

    if ( server.client_input_data[client_slot].most_recent_input == 0 )
        return;

    if ( tick < server.client_input_data[client_slot].first_input )
        return;

    // measure exactly how far ahead the client is delivering inputs

    int num_ticks_ahead = 0;

    while ( true )
    {
        uint64_t input_tick = tick + num_inputs + num_ticks_ahead;
        const int index = input_tick % InputSlidingWindowSize;
        if ( server.client_input_data[client_slot].inputs[index].tick != input_tick )
            break;
        num_ticks_ahead++;
    }

    /*
    if ( !server.client_bracket_data[client_slot].bracketing )
    {
        printf( "client %d delivered input %d ticks early\n", client_slot, num_ticks_ahead );
    }
    */
    
    if ( server.client_bracket_data[client_slot].bracketing )
    {
        int ticks_ahead_of_safety = max( 0, num_ticks_ahead - InputSafety );

        if ( server.client_bracket_data[client_slot].num_samples > 0 )
            server.client_bracket_data[client_slot].offset = min( ticks_ahead_of_safety, server.client_bracket_data[client_slot].offset );
        else
            server.client_bracket_data[client_slot].offset = ticks_ahead_of_safety;

//        printf( "client %d bracketing offset = %d\n", client_slot, (int) server.client_bracket_data[client_slot].offset );

        server.client_bracket_data[client_slot].num_samples++;

        if ( server.client_bracket_data[client_slot].num_samples == MaxBracketSamples )
        {
            printf( "client %d bracketed [-%d]\n", client_slot, server.client_bracket_data[client_slot].offset );
            server.client_input_data[client_slot] = InputData();
            server.client_bracket_data[client_slot].bracketing = false;
            server.client_bracket_data[client_slot].bracketed = true;
        }
    }
    else
    {
        // client is fully connected. gather inputs for simulation

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
                // if the client drops too many inputs, force them to reconnect and resynchronize

                printf( "client %d dropped input %d\n", client_slot, (int) input_tick );

                server.client_adjustment_data[client_slot].num_dropped_inputs++;
                server.client_adjustment_data[client_slot].time_last_dropped_input = real_time;

                if ( server.client_adjustment_data[client_slot].num_dropped_inputs >= ReconnectDroppedInputs )
                {
                    printf( "client %d dropped too many inputs. forcing reconnect\n", client_slot );
                    server.client_adjustment_data[client_slot].reconnect = true;
                    return;
                }
            }
        }

        // detect if we need to make adjustments (speed up or slow down client)

        if ( tick >= server.client_adjustment_data[client_slot].first_input )
        {
            if ( server.client_adjustment_data[client_slot].num_samples > 0 )
                server.client_adjustment_data[client_slot].min_ticks_ahead = min( server.client_adjustment_data[client_slot].min_ticks_ahead, num_ticks_ahead );
            else
                server.client_adjustment_data[client_slot].min_ticks_ahead = num_ticks_ahead;

            server.client_adjustment_data[client_slot].num_samples++;

            if ( server.client_adjustment_data[client_slot].num_samples == MaxAdjustmentSamples )
            {
                server.client_adjustment_data[client_slot].offset = - clamp( server.client_adjustment_data[client_slot].min_ticks_ahead - InputSafety, AdjustmentOffsetMinimum, AdjustmentOffsetMaximum );
                //if ( server.client_adjustment_data[client_slot].offset != 0 )
                    printf( "client %d is %d ticks ahead: adjustment offset = %+d\n", client_slot, server.client_adjustment_data[client_slot].min_ticks_ahead, server.client_adjustment_data[client_slot].offset );
                server.client_adjustment_data[client_slot].min_ticks_ahead = 0;
                server.client_adjustment_data[client_slot].num_samples = 0;
                server.client_adjustment_data[client_slot].first_input = 0;
                server.client_adjustment_data[client_slot].sequence++;
            }
        }

        // if the client has not had any dropped inputs for a period of time, reset their dropped input count

        const double time_since_last_dropped_input = real_time - server.client_adjustment_data[client_slot].time_last_dropped_input;

        if ( server.client_adjustment_data[client_slot].num_dropped_inputs > 0 &&
             time_since_last_dropped_input > DroppedInputForgetTime )
        {
            printf( "client %d forgetting dropped inputs\n", client_slot );
            server.client_adjustment_data[client_slot].num_dropped_inputs = 0;
        }
    }
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

        const double start_of_frame_time = platform_time();

        server_update( server, world.tick, real_time );

        server_receive_packets( server );

        server_send_packets( server );

        Input inputs[TicksPerServerFrame];
        server_get_client_input( server, 0, world.tick, inputs, TicksPerServerFrame, start_of_frame_time );

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
