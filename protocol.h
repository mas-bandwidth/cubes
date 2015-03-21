// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "const.h"
#include <string.h>

template <uint32_t x> struct PopCount
{
    enum {   a = x - ( ( x >> 1 )       & 0x55555555 ),
             b =   ( ( ( a >> 2 )       & 0x33333333 ) + ( a & 0x33333333 ) ),
             c =   ( ( ( b >> 4 ) + b ) & 0x0f0f0f0f ),
             d =   c + ( c >> 8 ),
             e =   d + ( d >> 16 ),

        result = e & 0x0000003f 
    };
};

template <uint32_t x> struct Log2
{
    enum {   a = x | ( x >> 1 ),
             b = a | ( a >> 2 ),
             c = b | ( b >> 4 ),
             d = c | ( c >> 8 ),
             e = d | ( d >> 16 ),
             f = e >> 1,

        result = PopCount<f>::result
    };
};

template <int64_t min, int64_t max> struct BitsRequired
{
    static const uint32_t result = ( min == max ) ? 0 : Log2<uint32_t(max-min)>::result + 1;
};

inline uint32_t popcount( uint32_t x )
{
    const uint32_t a = x - ( ( x >> 1 )       & 0x55555555 );
    const uint32_t b =   ( ( ( a >> 2 )       & 0x33333333 ) + ( a & 0x33333333 ) );
    const uint32_t c =   ( ( ( b >> 4 ) + b ) & 0x0f0f0f0f );
    const uint32_t d =   c + ( c >> 8 );
    const uint32_t e =   d + ( d >> 16 );
    const uint32_t result = e & 0x0000003f;
    return result;
}

#ifdef __GNUC__

inline int bits_required( uint32_t min, uint32_t max )
{
    return 32 - __builtin_clz( max - min );
}

#else

inline uint32_t log2( uint32_t x )
{
    const uint32_t a = x | ( x >> 1 );
    const uint32_t b = a | ( a >> 2 );
    const uint32_t c = b | ( b >> 4 );
    const uint32_t d = c | ( c >> 8 );
    const uint32_t e = d | ( d >> 16 );
    const uint32_t f = e >> 1;
    return popcount( f );
}

inline int bits_required( uint32_t min, uint32_t max )
{
    return ( min == max ) ? 0 : log2( max-min ) + 1;
}

#endif

template <typename T> const T & min( const T & a, const T & b )
{
    return ( a < b ) ? a : b;
}

template <typename T> const T & max( const T & a, const T & b )
{
    return ( a > b ) ? a : b;
}

template <typename T> T clamp( const T & value, const T & min, const T & max )
{
    if ( value < min )
        return min;
    else if ( value > max )
        return max;
    else
        return value;
}

template <typename T> void swap( T & a, T & b )
{
    T tmp = a;
    a = b;
    b = tmp;
};

template <typename T> T abs( const T & value )
{
    return ( value < 0 ) ? -value : value;
}

#define CPU_LITTLE_ENDIAN 1
#define CPU_BIG_ENDIAN 2

#if    defined(__386__) || defined(i386)    || defined(__i386__)  \
    || defined(__X86)   || defined(_M_IX86)                       \
    || defined(_M_X64)  || defined(__x86_64__)                    \
    || defined(alpha)   || defined(__alpha) || defined(__alpha__) \
    || defined(_M_ALPHA)                                          \
    || defined(ARM)     || defined(_ARM)    || defined(__arm__)   \
    || defined(WIN32)   || defined(_WIN32)  || defined(__WIN32__) \
    || defined(_WIN32_WCE) || defined(__NT__)                     \
    || defined(__MIPSEL__)
  #define CPU_ENDIAN CPU_LITTLE_ENDIAN
#else
  #define CPU_ENDIAN CPU_BIG_ENDIAN
#endif

inline uint32_t host_to_network( uint32_t value )
{
#if CPU_ENDIAN == CPU_BIG_ENDIAN
    return __builtin_bswap32( value );
#else
    return value;
#endif
}

inline uint32_t network_to_host( uint32_t value )
{
#if CPU_ENDIAN == CPU_BIG_ENDIAN
    return __builtin_bswap32( value );
#else
    return value;
#endif
}

class BitWriter
{
public:

    BitWriter( void * data, int bytes ) : m_data( (uint32_t*)data ), m_numWords( bytes / 4 )
    {
        assert( data );
        assert( ( bytes % 4 ) == 0 );           // IMPORTANT: buffer size must be a multiple of four!
        m_numBits = m_numWords * 32;
        m_bitsWritten = 0;
        m_scratch = 0;
        m_bitIndex = 0;
        m_wordIndex = 0;
        m_overflow = false;
        memset( m_data, 0, bytes );
    }

    void WriteBits( uint32_t value, int bits )
    {
        assert( bits > 0 );
        assert( bits <= 32 );
        assert( m_bitsWritten + bits <= m_numBits );

        if ( m_bitsWritten + bits > m_numBits )
        {
            m_overflow = true;
            return;
        }

        value &= ( uint64_t( 1 ) << bits ) - 1;

        m_scratch |= uint64_t( value ) << ( 64 - m_bitIndex - bits );

        m_bitIndex += bits;

        if ( m_bitIndex >= 32 )
        {
            assert( m_wordIndex < m_numWords );
            m_data[m_wordIndex] = host_to_network( uint32_t( m_scratch >> 32 ) );
            m_scratch <<= 32;
            m_bitIndex -= 32;
            m_wordIndex++;
        }

        m_bitsWritten += bits;
    }

    void WriteAlign()
    {
        const int remainderBits = m_bitsWritten % 8;
        if ( remainderBits != 0 )
        {
            uint32_t zero = 0;
            WriteBits( zero, 8 - remainderBits );
            assert( m_bitsWritten % 8 == 0 );
        }
    }

    void WriteBytes( const uint8_t * data, int bytes )
    {
        assert( GetAlignBits() == 0 );
        if ( m_bitsWritten + bytes * 8 >= m_numBits )
        {
            m_overflow = true;
            return;
        }

        assert( m_bitIndex == 0 || m_bitIndex == 8 || m_bitIndex == 16 || m_bitIndex == 24 );

        int headBytes = ( 4 - m_bitIndex / 8 ) % 4;
        if ( headBytes > bytes )
            headBytes = bytes;
        for ( int i = 0; i < headBytes; ++i )
            WriteBits( data[i], 8 );
        if ( headBytes == bytes )
            return;

        assert( GetAlignBits() == 0 );

        int numWords = ( bytes - headBytes ) / 4;
        if ( numWords > 0 )
        {
            assert( m_bitIndex == 0 );
            memcpy( &m_data[m_wordIndex], data + headBytes, numWords * 4 );
            m_bitsWritten += numWords * 32;
            m_wordIndex += numWords;
            m_scratch = 0;
        }

        assert( GetAlignBits() == 0 );

        int tailStart = headBytes + numWords * 4;
        int tailBytes = bytes - tailStart;
        assert( tailBytes >= 0 && tailBytes < 4 );
        for ( int i = 0; i < tailBytes; ++i )
            WriteBits( data[tailStart+i], 8 );

        assert( GetAlignBits() == 0 );

        assert( headBytes + numWords * 4 + tailBytes == bytes );
    }

    void FlushBits()
    {
        if ( m_bitIndex != 0 )
        {
            assert( m_wordIndex < m_numWords );
            if ( m_wordIndex >= m_numWords )
            {
                m_overflow = true;
                return;
            }
            m_data[m_wordIndex++] = host_to_network( uint32_t( m_scratch >> 32 ) );
        }
    }

    int GetAlignBits() const
    {
        return ( 8 - m_bitsWritten % 8 ) % 8;
    }

    int GetBitsWritten() const
    {
        return m_bitsWritten;
    }

    int GetBitsAvailable() const
    {
        return m_numBits - m_bitsWritten;
    }

    const uint8_t * GetData() const
    {
        return (uint8_t*) m_data;
    }

    int GetBytesWritten() const
    {
        return m_wordIndex * 4;
    }

    int GetTotalBytes() const
    {
        return m_numWords * 4;
    }

    bool IsOverflow() const
    {
        return m_overflow;
    }

private:

    uint32_t * m_data;
    uint64_t m_scratch;
    int m_numBits;
    int m_numWords;
    int m_bitsWritten;
    int m_bitIndex;
    int m_wordIndex;
    bool m_overflow;
};

class BitReader
{
public:

    BitReader( const void * data, int bytes ) : m_data( (const uint32_t*)data ), m_numWords( bytes / 4 )
    {
        assert( data );
        assert( ( bytes % 4 ) == 0 );           // IMPORTANT: buffer size must be a multiple of four!
        m_numBits = m_numWords * 32;
        m_bitsRead = 0;
        m_bitIndex = 0;
        m_wordIndex = 0;
        m_scratch = network_to_host( m_data[0] );
        m_overflow = false;
    }

    uint32_t ReadBits( int bits )
    {
        assert( bits > 0 );
        assert( bits <= 32 );
        assert( m_bitsRead + bits <= m_numBits );

        if ( m_bitsRead + bits > m_numBits )
        {
            m_overflow = true;
            return 0;
        }

        m_bitsRead += bits;

        assert( m_bitIndex < 32 );

        if ( m_bitIndex + bits < 32 )
        {
            m_scratch <<= bits;
            m_bitIndex += bits;
        }
        else
        {
            m_wordIndex++;
            assert( m_wordIndex < m_numWords );
            const uint32_t a = 32 - m_bitIndex;
            const uint32_t b = bits - a;
            m_scratch <<= a;
            m_scratch |= network_to_host( m_data[m_wordIndex] );
            m_scratch <<= b;
            m_bitIndex = b;
        }

        const uint32_t output = uint32_t( m_scratch >> 32 );

        m_scratch &= 0xFFFFFFFF;

        return output;
    }

    void ReadAlign()
    {
        const int remainderBits = m_bitsRead % 8;
        if ( remainderBits != 0 )
        {
            #ifdef NDEBUG
            ReadBits( 8 - remainderBits );
            #else
            uint32_t value = ReadBits( 8 - remainderBits );
            assert( value == 0 );
            assert( m_bitsRead % 8 == 0 );
            #endif
        }
    }

    void ReadBytes( uint8_t * data, int bytes )
    {
        assert( GetAlignBits() == 0 );

        if ( m_bitsRead + bytes * 8 >= m_numBits )
        {
            memset( data, bytes, 0 );
            m_overflow = true;
            return;
        }

        assert( m_bitIndex == 0 || m_bitIndex == 8 || m_bitIndex == 16 || m_bitIndex == 24 );

        int headBytes = ( 4 - m_bitIndex / 8 ) % 4;
        if ( headBytes > bytes )
            headBytes = bytes;
        for ( int i = 0; i < headBytes; ++i )
            data[i] = ReadBits( 8 );
        if ( headBytes == bytes )
            return;

        assert( GetAlignBits() == 0 );

        int numWords = ( bytes - headBytes ) / 4;
        if ( numWords > 0 )
        {
            assert( m_bitIndex == 0 );
            memcpy( data + headBytes, &m_data[m_wordIndex], numWords * 4 );
            m_bitsRead += numWords * 32;
            m_wordIndex += numWords;
            m_scratch = network_to_host( m_data[m_wordIndex] );
        }

        assert( GetAlignBits() == 0 );

        int tailStart = headBytes + numWords * 4;
        int tailBytes = bytes - tailStart;
        assert( tailBytes >= 0 && tailBytes < 4 );
        for ( int i = 0; i < tailBytes; ++i )
            data[tailStart+i] = ReadBits( 8 );

        assert( GetAlignBits() == 0 );

        assert( headBytes + numWords * 4 + tailBytes == bytes );
    }

    int GetAlignBits() const
    {
        return ( 8 - m_bitsRead % 8 ) % 8;
    }

    int GetBitsRead() const
    {
        return m_bitsRead;
    }

    int GetBytesRead() const
    {
        return ( m_wordIndex + 1 ) * 4;
    }

    int GetBitsRemaining() const
    {
        return m_numBits - m_bitsRead;
    }

    int GetTotalBits() const 
    {
        return m_numBits;
    }

    int GetTotalBytes() const
    {
        return m_numBits * 8;
    }

    bool IsOverflow() const
    {
        return m_overflow;
    }

private:

    const uint32_t * m_data;
    uint64_t m_scratch;
    int m_numBits;
    int m_numWords;
    int m_bitsRead;
    int m_bitIndex;
    int m_wordIndex;
    bool m_overflow;
};

class WriteStream
{
public:

    enum { IsWriting = 1 };
    enum { IsReading = 0 };

    WriteStream( uint8_t * buffer, int bytes ) : m_writer( buffer, bytes ), m_context( nullptr ), m_aborted( false ) {}

    void SerializeInteger( int32_t value, int32_t min, int32_t max )
    {
        assert( min < max );
        assert( value >= min );
        assert( value <= max );
        const int bits = bits_required( min, max );
        uint32_t unsigned_value = value - min;
        m_writer.WriteBits( unsigned_value, bits );
    }

    void SerializeBits( uint32_t value, int bits )
    {
        assert( bits > 0 );
        assert( bits <= 32 );
        m_writer.WriteBits( value, bits );
    }

    void SerializeBytes( const uint8_t * data, int bytes )
    {
        Align();
        m_writer.WriteBytes( data, bytes );
    }

    void Align()
    {
        m_writer.WriteAlign();
    }

    int GetAlignBits() const
    {
        return m_writer.GetAlignBits();
    }

    bool Check( uint32_t magic )
    {
        Align();
        SerializeBits( magic, 32 );
        return true;
    }

    void Flush()
    {
        m_writer.FlushBits();
    }

    const uint8_t * GetData() const
    {
        return m_writer.GetData();
    }

    int GetBytesProcessed() const
    {
        return m_writer.GetBytesWritten();
    }

    int GetBitsProcessed() const
    {
        return m_writer.GetBitsWritten();
    }

    int GetBitsRemaining() const
    {
        return GetTotalBits() - GetBitsProcessed();
    }

    int GetTotalBits() const
    {
        return m_writer.GetTotalBytes() * 8;
    }

    int GetTotalBytes() const
    {
        return m_writer.GetTotalBytes();
    }

    bool IsOverflow() const
    {
        return m_writer.IsOverflow();
    }

    void SetContext( const void ** context )
    {
        m_context = context;
    }

    const void * GetContext( int index ) const
    {
        assert( index >= 0 );
        assert( index < MaxContexts );
        return m_context ? m_context[index] : nullptr;
    }

    void Abort()
    {
        m_aborted = true;
    }

    bool Aborted() const
    {
        return m_aborted;
    }

private:

    BitWriter m_writer;
    const void ** m_context;
    bool m_aborted;
};

class ReadStream
{
public:

    enum { IsWriting = 0 };
    enum { IsReading = 1 };

    ReadStream( uint8_t * buffer, int bytes ) : m_bitsRead(0), m_reader( buffer, bytes ), m_context( nullptr ), m_aborted( false ) {}

    void SerializeInteger( int32_t & value, int32_t min, int32_t max )
    {
        assert( min < max );
        const int bits = bits_required( min, max );
        uint32_t unsigned_value = m_reader.ReadBits( bits );
        value = (int32_t) unsigned_value + min;
        m_bitsRead += bits;
    }

    void SerializeBits( uint32_t & value, int bits )
    {
        assert( bits > 0 );
        assert( bits <= 32 );
        uint32_t read_value = m_reader.ReadBits( bits );
        value = read_value;
        m_bitsRead += bits;
    }

    void SerializeBytes( uint8_t * data, int bytes )
    {
        Align();
        m_reader.ReadBytes( data, bytes );
        m_bitsRead += bytes * 8;
    }

    void Align()
    {
        m_reader.ReadAlign();
    }

    int GetAlignBits() const
    {
        return m_reader.GetAlignBits();
    }

    bool Check( uint32_t magic )
    {
        Align();
        uint32_t value = 0;
        SerializeBits( value, 32 );
        assert( value == magic );
        return value == magic;
    }

    int GetBitsProcessed() const
    {
        return m_bitsRead;
    }

    int GetBytesProcessed() const
    {
        return m_bitsRead / 8 + ( m_bitsRead % 8 ? 1 : 0 );
    }

    bool IsOverflow() const
    {
        return m_reader.IsOverflow();
    }

    void SetContext( const void ** context )
    {
        m_context = context;
    }

    const void * GetContext( int index ) const
    {
        assert( index >= 0 );
        assert( index < MaxContexts );
        return m_context ? m_context[index] : nullptr;
    }

    void Abort()
    {
        m_aborted = true;
    }

    bool Aborted() const
    {
        return m_aborted;
    }

    int GetBytesRead() const
    {
        return m_reader.GetBytesRead();
    }

private:

    int m_bitsRead;
    BitReader m_reader;
    const void ** m_context;
    bool m_aborted;
};

class MeasureStream
{
public:

    enum { IsWriting = 1 };
    enum { IsReading = 0 };

    MeasureStream( int bytes ) : m_totalBytes( bytes ), m_bitsWritten(0), m_context( nullptr ), m_aborted( false ) {}

    void SerializeInteger( int32_t value, int32_t min, int32_t max )
    {
        assert( min < max );
        assert( value >= min );
        assert( value <= max );
        const int bits = bits_required( min, max );
        m_bitsWritten += bits;
    }

    void SerializeBits( uint32_t value, int bits )
    {
        assert( bits > 0 );
        assert( bits <= 32 );
        m_bitsWritten += bits;
    }

    void SerializeBytes( const uint8_t * data, int bytes )
    {
        Align();
        m_bitsWritten += bytes * 8;
    }

    void Align()
    {
        const int alignBits = GetAlignBits();
        m_bitsWritten += alignBits;
    }

    int GetAlignBits() const
    {
        return 7;       // worst case
    }

    bool Check( uint32_t magic )
    {
        Align();
        m_bitsWritten += 32;
        return true;
    }

    int GetBitsProcessed() const
    {
        return m_bitsWritten;
    }

    int GetBytesProcessed() const
    {
        return m_bitsWritten / 8 + ( m_bitsWritten % 8 ? 1 : 0 );
    }

    int GetTotalBytes() const
    {
        return m_totalBytes;
    }

    int GetTotalBits() const
    {
        return m_totalBytes * 8;
    }

    bool IsOverflow() const
    {
        return m_bitsWritten > m_totalBytes * 8;
    }

    void SetContext( const void ** context )
    {
        m_context = context;
    }

    const void * GetContext( int index ) const
    {
        assert( index >= 0 );
        assert( index < MaxContexts );
        return m_context ? m_context[index] : nullptr;
    }

    void Abort()
    {
        m_aborted = true;
    }

    bool Aborted() const
    {
        return m_aborted;
    }

private:

    int m_totalBytes;
    int m_bitsWritten;
    const void ** m_context;
    bool m_aborted;
};

template <typename T> void serialize_object( ReadStream & stream, T & object )
{                        
    object.SerializeRead( stream );
}

template <typename T> void serialize_object( WriteStream & stream, T & object )
{                        
    object.SerializeWrite( stream );
}

template <typename T> void serialize_object( MeasureStream & stream, T & object )
{                        
    object.SerializeMeasure( stream );
}

#define serialize_int( stream, value, min, max )            \
    do                                                      \
    {                                                       \
        assert( min < max );                                \
        int32_t int32_value;                                \
        if ( Stream::IsWriting )                            \
        {                                                   \
            assert( value >= min );                         \
            assert( value <= max );                         \
            int32_value = (int32_t) value;                  \
        }                                                   \
        stream.SerializeInteger( int32_value, min, max );   \
        if ( Stream::IsReading )                            \
        {                                                   \
            value = (decltype(value)) int32_value;          \
            assert( value >= min );                         \
            assert( value <= max );                         \
        }                                                   \
    } while (0)

#define serialize_bits( stream, value, bits )               \
    do                                                      \
    {                                                       \
        assert( bits > 0 );                                 \
        assert( bits <= 32 );                               \
        uint32_t uint32_value;                              \
        if ( Stream::IsWriting )                            \
            uint32_value = (uint32_t) value;                \
        stream.SerializeBits( uint32_value, bits );         \
        if ( Stream::IsReading )                            \
            value = (decltype(value)) uint32_value;         \
    } while (0)

#define serialize_bool( stream, value ) serialize_bits( stream, value, 1 )

template <typename Stream> void serialize_uint16( Stream & stream, uint16_t & value )
{
    serialize_bits( stream, value, 16 );
}

template <typename Stream> void serialize_uint32( Stream & stream, uint32_t & value )
{
    serialize_bits( stream, value, 32 );
}

template <typename Stream> void serialize_uint64( Stream & stream, uint64_t & value )
{
    uint32_t hi,lo;
    if ( Stream::IsWriting )
    {
        lo = value & 0xFFFFFFFF;
        hi = value >> 32;
    }
    serialize_bits( stream, lo, 32 );
    serialize_bits( stream, hi, 32 );
    if ( Stream::IsReading )
        value = ( uint64_t(hi) << 32 ) | lo;
}

template <typename Stream> void serialize_int16( Stream & stream, int16_t & value )
{
    serialize_bits( stream, value, 16 );
}

template <typename Stream> void serialize_int32( Stream & stream, int32_t & value )
{
    serialize_bits( stream, value, 32 );
}

template <typename Stream> void serialize_int64( Stream & stream, int64_t & value )
{
    uint32_t hi,lo;
    if ( Stream::IsWriting )
    {
        lo = uint64_t(value) & 0xFFFFFFFF;
        hi = uint64_t(value) >> 32;
    }
    serialize_bits( stream, lo, 32 );
    serialize_bits( stream, hi, 32 );
    if ( Stream::IsReading )
        value = ( int64_t(hi) << 32 ) | lo;
}

template <typename Stream> void serialize_float( Stream & stream, float & value )
{
    union FloatInt
    {
        float float_value;
        uint32_t int_value;
    };

    FloatInt tmp;
    if ( Stream::IsWriting )
        tmp.float_value = value;

    serialize_uint32( stream, tmp.int_value );

    if ( Stream::IsReading )
        value = tmp.float_value;
}

template <typename Stream> inline void internal_serialize_float( Stream & stream, float & value, float min, float max, float res )
{
    const float delta = max - min;
    const float values = delta / res;
    const uint32_t maxIntegerValue = (uint32_t) ceil( values );
    const int bits = bits_required( 0, maxIntegerValue );
    
    uint32_t integerValue = 0;
    
    if ( Stream::IsWriting )
    {
        float normalizedValue = clamp( ( value - min ) / delta, 0.0f, 1.0f );
        integerValue = (uint32_t) floor( normalizedValue * maxIntegerValue + 0.5f );
    }
    
    stream.SerializeBits( integerValue, bits );

    if ( Stream::IsReading )
    {
        const float normalizedValue = integerValue / float( maxIntegerValue );
        value = normalizedValue * delta + min;
    }
}

#define serialize_compressed_float( stream, value, min, max, res )                                        \
do                                                                                                        \
{                                                                                                         \
    internal_serialize_float( stream, value, min, max, res );                                             \
}                                                                                                         \
while(0)

template <typename Stream> void serialize_double( Stream & stream, double & value )
{
    union DoubleInt
    {
        double double_value;
        uint64_t int_value;
    };

    DoubleInt tmp;
    if ( Stream::IsWriting )
        tmp.double_value = value;

    serialize_uint64( stream, tmp.int_value );

    if ( Stream::IsReading )
        value = tmp.double_value;
}

template <typename Stream> void serialize_bytes( Stream & stream, uint8_t * data, int bytes )
{
    stream.SerializeBytes( data, bytes );        
}

template <typename Stream> void serialize_string( Stream & stream, char * string, int buffer_size )
{
    uint32_t length;
    if ( Stream::IsWriting )
        length = strlen( string );
    stream.Align();
    stream.SerializeBits( length, 32 );
    assert( length < buffer_size - 1 );
    stream.SerializeBytes( (uint8_t*)string, length );
    if ( Stream::IsReading )
        string[length] = '\0';
}

template <typename Stream> bool serialize_check( Stream & stream, uint32_t magic )
{
    return stream.Check( magic );
}

inline int signed_to_unsigned( int n )
{
    return ( n << 1 ) ^ ( n >> 31 );
}

inline int unsigned_to_signed( uint32_t n )
{
    return ( n >> 1 ) ^ ( -( n & 1 ) );
}

#define SERIALIZE_OBJECT( stream )                                                          \
    void SerializeRead( class ReadStream & stream ) { Serialize( stream ); };               \
    void SerializeWrite( class WriteStream & stream ) { Serialize( stream ); };             \
    void SerializeMeasure( class MeasureStream & stream ) { Serialize( stream ); };         \
    template <typename Stream> void Serialize( Stream & stream )                            

#endif // #ifndef PROTOCOL_H
