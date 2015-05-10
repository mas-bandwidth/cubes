// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef PACKETS_H
#define PACKETS_H

#include "protocol.h"
#include "game.h"

enum PacketType
{
    PACKET_TYPE_CONNECTION_REQUEST,
    PACKET_TYPE_CONNECTION_ACCEPTED,
    PACKET_TYPE_CONNECTION_DENIED,
    PACKET_TYPE_INPUT,
    PACKET_TYPE_SNAPSHOT,
    NUM_PACKET_TYPES
};

struct Packet
{
    uint32_t type;
};

struct ConnectionRequestPacket : public Packet
{
    uint64_t client_guid;
    uint16_t connect_sequence;

    SERIALIZE_OBJECT( stream )
    {
        serialize_uint64( stream, client_guid );
        serialize_uint16( stream, connect_sequence );
    }
};

struct ConnectionAcceptedPacket : public Packet
{
    uint64_t client_guid;
    uint16_t connect_sequence;

    SERIALIZE_OBJECT( stream )
    {
        serialize_uint64( stream, client_guid );
        serialize_uint16( stream, connect_sequence );
    }
};

struct ConnectionDeniedPacket : public Packet
{
    uint64_t client_guid;
    uint16_t connect_sequence;

    SERIALIZE_OBJECT( stream )
    {
        serialize_uint64( stream, client_guid );
    }
};

struct InputPacket : public Packet
{
    bool synchronizing = false;
    bool bracketed = false;
    uint16_t sync_offset = 0;
    uint16_t sync_sequence = 0;
    uint16_t adjustment_sequence = 0;
    uint64_t tick = 0;
    int num_inputs = 0;
    Input input[MaxInputsPerPacket];

    SERIALIZE_OBJECT( stream )
    {
        serialize_bool( stream, synchronizing );
        if ( synchronizing )
        {
            serialize_uint16( stream, sync_offset );
            serialize_uint16( stream, sync_sequence );
            serialize_uint64( stream, tick );
        }
        else
        {
            serialize_uint64( stream, tick );
            serialize_bool( stream, bracketed );
            serialize_uint16( stream, adjustment_sequence );
            serialize_int( stream, num_inputs, 0, MaxInputsPerPacket );
            for ( int i = 0; i < num_inputs; ++i )
            {
                if ( i > 0 )
                {
                    bool different = Stream::IsWriting ? input[i] != input[i-i] : false;
                    serialize_bool( stream, different );
                    if ( different )
                    {
                        serialize_bool( stream, input[i].left );
                        serialize_bool( stream, input[i].right );
                        serialize_bool( stream, input[i].up );
                        serialize_bool( stream, input[i].down );
                        serialize_bool( stream, input[i].push );
                        serialize_bool( stream, input[i].pull );
                    }
                }
                else
                {
                    serialize_bool( stream, input[i].left );
                    serialize_bool( stream, input[i].right );
                    serialize_bool( stream, input[i].up );
                    serialize_bool( stream, input[i].down );
                    serialize_bool( stream, input[i].push );
                    serialize_bool( stream, input[i].pull );
                }
            }
        }
    }
};

struct SnapshotPacket : public Packet
{
    bool synchronizing = false;
    bool bracketing = false;
    bool reconnect = false;
    uint16_t sync_offset = 0;
    uint16_t bracket_offset = 0;
    uint16_t adjustment_sequence = 0;
    int adjustment_offset = 0;
    uint64_t tick = 0;
    uint64_t input_ack = 0;

    SERIALIZE_OBJECT( stream )
    {
        serialize_bool( stream, synchronizing );
        if ( synchronizing )
        {
            serialize_uint64( stream, tick );
            serialize_uint16( stream, sync_offset );
        }
        else
        {
            serialize_bool( stream, reconnect );
            serialize_bool( stream, bracketing );
            serialize_uint16( stream, bracket_offset );

            if ( !bracketing )
            {
                serialize_uint16( stream, adjustment_sequence );
                serialize_int( stream, adjustment_offset, AdjustmentOffsetMinimum, AdjustmentOffsetMaximum );
            }

            serialize_uint64( stream, tick );
            serialize_uint64( stream, input_ack );

            // todo: serialize rest of snapshot
        }
    }
};

bool write_packet( WriteStream & stream, Packet & base_packet, int & packet_bytes )
{
    typedef WriteStream Stream;
    serialize_int( stream, base_packet.type, 0, NUM_PACKET_TYPES - 1 );
    switch ( base_packet.type )
    {
        // todo: convert these to macros

        case PACKET_TYPE_CONNECTION_REQUEST:
        {
            ConnectionRequestPacket & packet = (ConnectionRequestPacket&) base_packet;
            serialize_object( stream, packet );
        }
        break;

        case PACKET_TYPE_CONNECTION_ACCEPTED:
        {
            ConnectionAcceptedPacket & packet = (ConnectionAcceptedPacket&) base_packet;
            serialize_object( stream, packet );
        }
        break;

        case PACKET_TYPE_CONNECTION_DENIED:
        {
            ConnectionDeniedPacket & packet = (ConnectionDeniedPacket&) base_packet;
            serialize_object( stream, packet );
        }
        break;

        case PACKET_TYPE_INPUT:
        {
            InputPacket & packet = (InputPacket&) base_packet;
            serialize_object( stream, packet );
        }
        break;

        case PACKET_TYPE_SNAPSHOT:
        {
            SnapshotPacket & packet = (SnapshotPacket&) base_packet;
            serialize_object( stream, packet );
        }
        break;
    }
    stream.Flush();
    packet_bytes = stream.GetBytesProcessed();
    return !stream.IsOverflow();
}

extern bool process_packet( const class Address & from, Packet & packet, void * context );

bool read_packet( const class Address & from, uint8_t * buffer, int buffer_size, void * context )
{
    typedef ReadStream Stream;
    int packet_type;
    ReadStream stream( buffer, buffer_size );
    serialize_int( stream, packet_type, 0, NUM_PACKET_TYPES - 1 );
    switch( packet_type )
    {
        // todo: convert these to macros

        case PACKET_TYPE_CONNECTION_REQUEST:
        {
            ConnectionRequestPacket packet;
            packet.type = packet_type;
            serialize_object( stream, packet );
            if ( !stream.IsOverflow() )
                return process_packet( from, packet, context );
        }
        break;

        case PACKET_TYPE_CONNECTION_ACCEPTED:
        {
            ConnectionAcceptedPacket packet;
            packet.type = packet_type;
            serialize_object( stream, packet );
            if ( !stream.IsOverflow() )
                return process_packet( from, packet, context );
        }
        break;

        case PACKET_TYPE_CONNECTION_DENIED:
        {
            ConnectionDeniedPacket packet;
            packet.type = packet_type;
            serialize_object( stream, packet );
            if ( !stream.IsOverflow() )
                return process_packet( from, packet, context );
        }
        break;

        case PACKET_TYPE_INPUT:
        {
            InputPacket packet;
            packet.type = packet_type;
            serialize_object( stream, packet );
            if ( !stream.IsOverflow() )
                return process_packet( from, packet, context );
        }
        break;

        case PACKET_TYPE_SNAPSHOT:
        {
            SnapshotPacket packet;
            packet.type = packet_type;
            serialize_object( stream, packet );
            if ( !stream.IsOverflow() )
                return process_packet( from, packet, context );
        }
        break;
    }

    return false;
}

const char * packet_type_string( int packet_type )
{
    switch ( packet_type )
    {
        case PACKET_TYPE_CONNECTION_REQUEST:                return "connection request";
        case PACKET_TYPE_CONNECTION_ACCEPTED:               return "connection accepted";
        case PACKET_TYPE_CONNECTION_DENIED:                 return "connection denied";
        case PACKET_TYPE_INPUT:                             return "input";
        case PACKET_TYPE_SNAPSHOT:                          return "snapshot";
        default:
            assert( false );
            return "???";
    }
}

#endif // #ifndef PACKETS_H
