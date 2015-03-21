// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "protocol.h"

template <typename Stream> void serialize_unsigned_range( Stream & stream, uint32_t & value, int num_ranges, const int * range_bits )
{
    assert( num_ranges > 0 );

    int range_min = 0;
    
    for ( int i = 0; i < num_ranges - 1; ++i )
    {
        const int range_max = range_min + ( ( 1 << range_bits[i] ) - 1 );
        bool in_range = Stream::IsWriting && value <= range_max;
        serialize_bool( stream, in_range );
        if ( in_range )
        {
            serialize_int( stream, value, range_min, range_max );
            return;
        }
        range_min += ( 1 << range_bits[i] );
    }

    serialize_int( stream, value, range_min, range_min + ( ( 1 << range_bits[num_ranges-1] ) - 1 ) );
}

inline int unsigned_range_limit( int num_ranges, const int * range_bits )
{
    int range_limit = 0;
    for ( int i = 0; i < num_ranges; ++i )
        range_limit += ( 1 << range_bits[i] );
    return range_limit;
}

template <typename Stream> void serialize_relative_position( Stream & stream,
                                                             int & position_x,
                                                             int & position_y,
                                                             int & position_z,
                                                             int base_position_x,
                                                             int base_position_y,
                                                             int base_position_z )
{
    bool all_small;
    bool too_large;
    uint32_t dx,dy,dz;

    const int range_bits[] = { 5, 6, 7 };
    const int num_ranges = sizeof( range_bits ) / sizeof( int );

    const int small_limit = 15;
    const int large_limit = unsigned_range_limit( num_ranges, range_bits );

    const int max_delta = 2047;

    if ( Stream::IsWriting )
    {
        dx = signed_to_unsigned( position_x - base_position_x );
        dy = signed_to_unsigned( position_y - base_position_y );
        dz = signed_to_unsigned( position_z - base_position_z );
        all_small = dx <= small_limit && dy <= small_limit && dz <= small_limit;
        too_large = dx >= large_limit || dy >= large_limit || dz >= large_limit;
    }

    serialize_bool( stream, all_small );

    if ( all_small )
    {
        serialize_int( stream, dx, 0, small_limit );
        serialize_int( stream, dy, 0, small_limit );
        serialize_int( stream, dz, 0, small_limit );
    }
    else
    {
        serialize_bool( stream, too_large );

        if ( !too_large )
        {
            serialize_unsigned_range( stream, dx, num_ranges, range_bits );
            serialize_unsigned_range( stream, dy, num_ranges, range_bits );
            serialize_unsigned_range( stream, dz, num_ranges, range_bits );
        }
        else
        {
            serialize_int( stream, dx, 0, max_delta );
            serialize_int( stream, dy, 0, max_delta );            
            serialize_int( stream, dz, 0, max_delta );            
        }
    }

    if ( Stream::IsReading )
    {
        int signed_dx = unsigned_to_signed( dx );
        int signed_dy = unsigned_to_signed( dy );
        int signed_dz = unsigned_to_signed( dz );

        position_x = base_position_x + signed_dx;
        position_y = base_position_y + signed_dy;
        position_z = base_position_z + signed_dz;
    }
}

template <int bits> struct compressed_quaternion
{
    enum { max_value = (1<<bits)-1 };

    uint32_t largest : 2;
    uint32_t integer_a : bits;
    uint32_t integer_b : bits;
    uint32_t integer_c : bits;

    void Load( float x, float y, float z, float w )
    {
        assert( bits > 1 );
        assert( bits <= 10 );

        const float minimum = - 1.0f / 1.414214f;       // 1.0f / sqrt(2)
        const float maximum = + 1.0f / 1.414214f;

        const float scale = float( ( 1 << bits ) - 1 );

        const float abs_x = fabs( x );
        const float abs_y = fabs( y );
        const float abs_z = fabs( z );
        const float abs_w = fabs( w );

        largest = 0;
        float largest_value = abs_x;

        if ( abs_y > largest_value )
        {
            largest = 1;
            largest_value = abs_y;
        }

        if ( abs_z > largest_value )
        {
            largest = 2;
            largest_value = abs_z;
        }

        if ( abs_w > largest_value )
        {
            largest = 3;
            largest_value = abs_w;
        }

        float a = 0;
        float b = 0;
        float c = 0;

        switch ( largest )
        {
            case 0:
                if ( x >= 0 )
                {
                    a = y;
                    b = z;
                    c = w;
                }
                else
                {
                    a = -y;
                    b = -z;
                    c = -w;
                }
                break;

            case 1:
                if ( y >= 0 )
                {
                    a = x;
                    b = z;
                    c = w;
                }
                else
                {
                    a = -x;
                    b = -z;
                    c = -w;
                }
                break;

            case 2:
                if ( z >= 0 )
                {
                    a = x;
                    b = y;
                    c = w;
                }
                else
                {
                    a = -x;
                    b = -y;
                    c = -w;
                }
                break;

            case 3:
                if ( w >= 0 )
                {
                    a = x;
                    b = y;
                    c = z;
                }
                else
                {
                    a = -x;
                    b = -y;
                    c = -z;
                }
                break;

            default:
                assert( false );
        }

        const float normal_a = ( a - minimum ) / ( maximum - minimum ); 
        const float normal_b = ( b - minimum ) / ( maximum - minimum );
        const float normal_c = ( c - minimum ) / ( maximum - minimum );

        integer_a = floor( normal_a * scale + 0.5f );
        integer_b = floor( normal_b * scale + 0.5f );
        integer_c = floor( normal_c * scale + 0.5f );
    }

    void Save( float & x, float & y, float & z, float & w ) const
    {
        // note: you're going to want to normalize the quaternion returned from this function

        assert( bits > 1 );
        assert( bits <= 10 );

        const float minimum = - 1.0f / 1.414214f;       // 1.0f / sqrt(2)
        const float maximum = + 1.0f / 1.414214f;

        const float scale = float( ( 1 << bits ) - 1 );

        const float inverse_scale = 1.0f / scale;

        const float a = integer_a * inverse_scale * ( maximum - minimum ) + minimum;
        const float b = integer_b * inverse_scale * ( maximum - minimum ) + minimum;
        const float c = integer_c * inverse_scale * ( maximum - minimum ) + minimum;

        switch ( largest )
        {
            case 0:
            {
                x = sqrtf( 1 - a*a - b*b - c*c );
                y = a;
                z = b;
                w = c;
            }
            break;

            case 1:
            {
                x = a;
                y = sqrtf( 1 - a*a - b*b - c*c );
                z = b;
                w = c;
            }
            break;

            case 2:
            {
                x = a;
                y = b;
                z = sqrtf( 1 - a*a - b*b - c*c );
                w = c;
            }
            break;

            case 3:
            {
                x = a;
                y = b;
                z = c;
                w = sqrtf( 1 - a*a - b*b - c*c );
            }
            break;

            default:
            {
                assert( false );
                x = 0;
                y = 0;
                z = 0;
                w = 1;
            }
        }
    }

    SERIALIZE_OBJECT( stream )
    {
        serialize_bits( stream, largest, 2 );
        serialize_bits( stream, integer_a, bits );
        serialize_bits( stream, integer_b, bits );
        serialize_bits( stream, integer_c, bits );
    }

    bool operator == ( const compressed_quaternion & other ) const
    {
        if ( largest != other.largest )
            return false;

        if ( integer_a != other.integer_a )
            return false;

        if ( integer_b != other.integer_b )
            return false;

        if ( integer_c != other.integer_c )
            return false;

        return true;
    }

    bool operator != ( const compressed_quaternion & other ) const
    {
        return ! ( *this == other );
    }
};

struct QuantizedCubeState
{
    bool interacting;

    int position_x;
    int position_y;
    int position_z;

    compressed_quaternion<OrientationBits> orientation;

    bool operator == ( const QuantizedCubeState & other ) const
    {
        if ( interacting != other.interacting )
            return false;

        if ( position_x != other.position_x )
            return false;

        if ( position_y != other.position_y )
            return false;

        if ( position_z != other.position_z )
            return false;

        if ( orientation != other.orientation )
            return false;

        return true;
    }

    bool operator != ( const QuantizedCubeState & other ) const
    {
        return ! ( *this == other );
    }
};

struct QuantizedSnapshot
{
    QuantizedCubeState cubes[NumCubes];

    bool operator == ( const QuantizedSnapshot & other ) const
    {
        for ( int i = 0; i < NumCubes; ++i )
        {
            if ( cubes[i] != other.cubes[i] )
                return false;
        }

        return true;
    }

    bool operator != ( const QuantizedSnapshot & other ) const
    {
        return ! ( *this == other );
    }
};

template <typename Stream> void serialize_relative_orientation( Stream & stream, 
                                                                compressed_quaternion<OrientationBits> & orientation, 
                                                                const compressed_quaternion<OrientationBits> & base_orientation )
{
    const int range_bits[] = { 4, 5, 7 };
    const int num_ranges = sizeof( range_bits ) / sizeof( int );

    const int small_limit = 3;
    const int large_limit = unsigned_range_limit( num_ranges, range_bits );

    uint32_t da,db,dc;
    bool all_small = false;
    bool relative_orientation = false;

    if ( Stream::IsWriting && orientation.largest == base_orientation.largest )
    {
        da = signed_to_unsigned( orientation.integer_a - base_orientation.integer_a );
        db = signed_to_unsigned( orientation.integer_b - base_orientation.integer_b );
        dc = signed_to_unsigned( orientation.integer_c - base_orientation.integer_c );

        all_small = da <= small_limit && db <= small_limit && dc <= small_limit;

        relative_orientation = da < large_limit && db < large_limit && dc < large_limit;
    }

    serialize_bool( stream, relative_orientation );

    if ( relative_orientation )
    {
        serialize_bool( stream, all_small );

        if ( all_small )
        {
            serialize_int( stream, da, 0, small_limit );
            serialize_int( stream, db, 0, small_limit );
            serialize_int( stream, dc, 0, small_limit );
        }
        else
        {
            serialize_unsigned_range( stream, da, num_ranges, range_bits );
            serialize_unsigned_range( stream, db, num_ranges, range_bits );
            serialize_unsigned_range( stream, dc, num_ranges, range_bits );
        }

        if ( Stream::IsReading )
        {
            int signed_da = unsigned_to_signed( da );
            int signed_db = unsigned_to_signed( db );
            int signed_dc = unsigned_to_signed( dc );

            orientation.largest = base_orientation.largest;
            orientation.integer_a = base_orientation.integer_a + signed_da;
            orientation.integer_b = base_orientation.integer_b + signed_db;
            orientation.integer_c = base_orientation.integer_c + signed_dc;
        }
    }
    else
    {
        serialize_object( stream, orientation );
    }
}

template <typename Stream> void serialize_cube_relative_to_base( Stream & stream, QuantizedCubeState & cube, const QuantizedCubeState & base, int base_dx, int base_dy, int base_dz )
{
    serialize_bool( stream, cube.interacting );

    bool position_changed;

    if ( Stream::IsWriting )
        position_changed = cube.position_x != base.position_x || cube.position_y != base.position_y || cube.position_z != base.position_z;

    serialize_bool( stream, position_changed );

    if ( position_changed )
    {
        const int gravity = 3;
        const int ground_limit = 105;

        const int drag_x = - ceil( base_dx * 0.062f );
        const int drag_y = - ceil( base_dy * 0.062f );
        const int drag_z = - ceil( base_dz * 0.062f );

        const int position_estimate_x = base.position_x + base_dx + drag_x;
        const int position_estimate_y = base.position_y + base_dy + drag_y;
        const int position_estimate_z = max( base.position_z + base_dz - gravity + drag_z, ground_limit );

        serialize_relative_position( stream, cube.position_x, cube.position_y, cube.position_z, position_estimate_x, position_estimate_y, position_estimate_z );
    }
    else if ( Stream::IsReading )
    {
        cube.position_x = base.position_x;
        cube.position_y = base.position_y;
        cube.position_z = base.position_z;
    }

    serialize_relative_orientation( stream, cube.orientation, base.orientation );
}

struct CompressionState
{
    float delta_x[NumCubes];
    float delta_y[NumCubes];
    float delta_z[NumCubes];
};

void calculate_compression_state( CompressionState & compression_state, QuantizedSnapshot & current_snapshot, QuantizedSnapshot & baseline_snapshot )
{
    for ( int i = 0; i < NumCubes; ++i )
    {
        compression_state.delta_x[i] = current_snapshot.cubes[i].position_x - baseline_snapshot.cubes[i].position_x;
        compression_state.delta_y[i] = current_snapshot.cubes[i].position_y - baseline_snapshot.cubes[i].position_y;
        compression_state.delta_z[i] = current_snapshot.cubes[i].position_z - baseline_snapshot.cubes[i].position_z;
    }
}

inline int count_relative_index_bits( bool * changed )
{
    int bits = 8;           // 0..255 num changed
    bool first = true;
    int previous_index = 0;

    for ( int i = 0; i < NumCubes; ++i )
    {
        if ( !changed[i] )
            continue;

        if ( first )
        {
            bits += 10;
            first = false;
            previous_index = i;
        }
        else
        {
            const int difference = i - previous_index;

            if ( difference == 1 )
            {
                bits += 1;
            }
            else if ( difference <= 6 )
            {
                bits += 1 + 1 + 2;
            }
            else if ( difference <= 14 )
            {
                bits += 1 + 1 + 1 + 3;
            }
            else if ( difference <= 30 )
            {
                bits += 1 + 1 + 1 + 1 + 4;
            }
            else if ( difference <= 62 )
            {
                bits += 1 + 1 + 1 + 1 + 1 + 5;
            }
            else if ( difference <= 126 )
            {
                bits += 1 + 1 + 1 + 1 + 1 + 1 + 6;
            }
            else
            {
                bits += 1 + 1 + 1 + 1 + 1 + 1 + 1 + 10;
            }

            previous_index = i;
        }
    }

    return bits;
}

template <typename Stream> void serialize_relative_index( Stream & stream, int previous, int & current )
{
    uint32_t difference;
    if ( Stream::IsWriting )
    {
        assert( previous < current );
        difference = current - previous;
        assert( difference > 0 );
    }

    // +1 (1 bit)

    bool plusOne;
    if ( Stream::IsWriting )
        plusOne = difference == 1;
    serialize_bool( stream, plusOne );
    if ( plusOne )
    {
        current = previous + 1;
        return;
    }

    // [+2,6] (2 bits)

    bool twoBits;
    if ( Stream::IsWriting )
        twoBits = difference <= 6;
    serialize_bool( stream, twoBits );
    if ( twoBits )
    {
        serialize_int( stream, difference, 2, 6 );
        if ( Stream::IsReading )
            current = previous + difference;
        return;
    }

    // [7,14] -> [0,7] (3 bits)

    bool threeBits;
    if ( Stream::IsWriting )
        threeBits = difference <= 14;
    serialize_bool( stream, threeBits );
    if ( threeBits )
    {
        serialize_int( stream, difference, 7, 14 );
        if ( Stream::IsReading )
            current = previous + difference;
        return;
    }

    // [15,30] -> [0,15] (4 bits)

    bool fourBits;
    if ( Stream::IsWriting )
        fourBits = difference <= 30;
    serialize_bool( stream, fourBits );
    if ( fourBits )
    {
        serialize_int( stream, difference, 15, 30 );
        if ( Stream::IsReading )
            current = previous + difference;
        return;
    }

    // [31,62] -> [0,31] (5 bits)

    bool fiveBits;
    if ( Stream::IsWriting )
        fiveBits = difference <= 62;
    serialize_bool( stream, fiveBits );
    if ( fiveBits )
    {
        serialize_int( stream, difference, 31, 62 );
        if ( Stream::IsReading )
            current = previous + difference;
        return;
    }

    // [63,126] -> [0,63] (6 bits)

    bool sixBits;
    if ( Stream::IsWriting )
        sixBits = difference <= 126;
    serialize_bool( stream, sixBits );
    if ( sixBits )
    {
        serialize_int( stream, difference, 63, 126 );
        if ( Stream::IsReading )
            current = previous + difference;
        return;
    }

    // [127,NumCubes]

    serialize_int( stream, difference, 127, NumCubes - 1 );
    if ( Stream::IsReading )
        current = previous + difference;
}

template <typename Stream> void serialize_snapshot_relative_to_baseline( Stream & stream, CompressionState & compression_state, QuantizedSnapshot & current_snapshot, QuantizedSnapshot & baseline_snapshot )
{
    QuantizedCubeState * quantized_cubes = &current_snapshot.cubes[0];
    QuantizedCubeState * quantized_base_cubes = &baseline_snapshot.cubes[0];

    const int MaxChanged = 256;

    int num_changed = 0;
    bool use_indices = false;
    bool changed[NumCubes];
    if ( Stream::IsWriting )
    {
        for ( int i = 0; i < NumCubes; ++i )
        {
            changed[i] = quantized_cubes[i] != quantized_base_cubes[i];
            if ( changed[i] )
                num_changed++;
        }

        if ( num_changed > 0 )
        {
            int relative_index_bits = count_relative_index_bits( changed );
            if ( num_changed <= MaxChanged && relative_index_bits <= NumCubes )
                use_indices = true;
        }
    }

    serialize_bool( stream, use_indices );

    if ( use_indices )
    {
        serialize_int( stream, num_changed, 1, MaxChanged );

        if ( Stream::IsWriting )
        {
            int num_written = 0;

            bool first = true;
            int previous_index = 0;

            for ( int i = 0; i < NumCubes; ++i )
            {
                if ( changed[i] )
                {
                    if ( first )
                    {
                        serialize_int( stream, i, 0, NumCubes - 1 );
                        first = false;
                    }
                    else
                    {   
                        serialize_relative_index( stream, previous_index, i );
                    }

                    serialize_cube_relative_to_base( stream, quantized_cubes[i], quantized_base_cubes[i], compression_state.delta_x[i], compression_state.delta_y[i], compression_state.delta_z[i] );

                    num_written++;

                    previous_index = i;
                }
            }

            assert( num_written == num_changed );
        }
        else
        {
            memset( changed, 0, sizeof( changed ) );

            int previous_index = 0;

            for ( int j = 0; j < num_changed; ++j )
            {
                int i;
                if ( j == 0 )
                    serialize_int( stream, i, 0, NumCubes - 1 );
                else                                
                    serialize_relative_index( stream, previous_index, i );

                serialize_cube_relative_to_base( stream, quantized_cubes[i], quantized_base_cubes[i], compression_state.delta_x[i], compression_state.delta_y[i], compression_state.delta_z[i] );

                changed[i] = true;

                previous_index = i;
            }

            for ( int i = 0; i < NumCubes; ++i )
            {
                if ( !changed[i] )
                    memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
            }
        }
    }
    else
    {
        for ( int i = 0; i < NumCubes; ++i )
        {
            serialize_bool( stream, changed[i] );

            if ( changed[i] )
            {
                serialize_cube_relative_to_base( stream, quantized_cubes[i], quantized_base_cubes[i], compression_state.delta_x[i], compression_state.delta_y[i], compression_state.delta_z[i] );
            }
            else if ( Stream::IsReading )
            {
                memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
            }
        }
    }
}

struct FrameCubeData
{
    int orientation_largest;
    int orientation_a;
    int orientation_b;
    int orientation_c;
    int position_x;
    int position_y;
    int position_z;
    int interacting;
};

struct Frame
{
    FrameCubeData cubes[NumCubes];
};

struct Packet
{
    int size;
    uint8_t data[MaxPacketSize];
};

void convert_frame_to_snapshot( const Frame & frame, QuantizedSnapshot & snapshot )
{
    for ( int j = 0; j < NumCubes; ++j )
    {
        assert( frame.cubes[j].orientation_largest >= 0 );
        assert( frame.cubes[j].orientation_largest <= 3 );

        snapshot.cubes[j].orientation.largest = frame.cubes[j].orientation_largest;

        assert( frame.cubes[j].orientation_a >= 0 );
        assert( frame.cubes[j].orientation_b >= 0 );
        assert( frame.cubes[j].orientation_c >= 0 );

        assert( frame.cubes[j].orientation_a <= ( 1 << OrientationBits ) - 1 );
        assert( frame.cubes[j].orientation_b <= ( 1 << OrientationBits ) - 1 );
        assert( frame.cubes[j].orientation_c <= ( 1 << OrientationBits ) - 1 );

        snapshot.cubes[j].orientation.integer_a = frame.cubes[j].orientation_a;
        snapshot.cubes[j].orientation.integer_b = frame.cubes[j].orientation_b;
        snapshot.cubes[j].orientation.integer_c = frame.cubes[j].orientation_c;

        assert( frame.cubes[j].position_x >= -QuantizedPositionBoundXY );
        assert( frame.cubes[j].position_y >= -QuantizedPositionBoundXY );
        assert( frame.cubes[j].position_z >= 0 );

        assert( frame.cubes[j].position_x <= QuantizedPositionBoundXY );
        assert( frame.cubes[j].position_y <= QuantizedPositionBoundXY );
        assert( frame.cubes[j].position_z <= QuantizedPositionBoundZ );

        snapshot.cubes[j].position_x = frame.cubes[j].position_x;
        snapshot.cubes[j].position_y = frame.cubes[j].position_y;
        snapshot.cubes[j].position_z = frame.cubes[j].position_z;

        assert( frame.cubes[j].interacting == 0 || frame.cubes[j].interacting == 1 );

        snapshot.cubes[j].interacting = frame.cubes[j].interacting;
    }
}

#endif // #ifndef SNAPSHOT_H
