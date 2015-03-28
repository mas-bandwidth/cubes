#ifndef CORE_H
#define CORE_H

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

#endif // #ifndef CORE_H
