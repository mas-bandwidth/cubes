// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef NETWORK_H
#define NETWORK_H

struct addrinfo;
struct sockaddr_in6;
struct sockaddr_storage;

#include <stdint.h>

extern bool InitializeNetwork();

extern void ShutdownNetwork();

extern bool IsNetworkInitialized();

enum AddressType
{
    ADDRESS_UNDEFINED,
    ADDRESS_IPV4,
    ADDRESS_IPV6
};

class Address
{
    AddressType m_type;

    union
    {
        uint32_t m_address4;
        uint16_t m_address6[8];
    };

    uint16_t m_port;

public:

    Address();

    Address( uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port = 0 );

    explicit Address( uint32_t address, int16_t port = 0 );

    explicit Address( uint16_t a, uint16_t b, uint16_t c, uint16_t d,
                      uint16_t e, uint16_t f, uint16_t g, uint16_t h,
                      uint16_t port = 0 );

    explicit Address( const uint16_t address[], uint16_t port = 0 );

    explicit Address( const sockaddr_storage & addr );

    explicit Address( const sockaddr_in6 & addr_ipv6 );

    explicit Address( addrinfo * p );

    explicit Address( const char * address );

    explicit Address( const char * address, uint16_t port );

    void Clear();

    uint32_t GetAddress4() const;

    const uint16_t * GetAddress6() const;

    void SetPort( uint64_t port );

    uint16_t GetPort() const;

    AddressType GetType() const;

    const char * ToString( char buffer[], int bufferSize ) const;

    bool IsValid() const;

    bool operator ==( const Address & other ) const;

    bool operator !=( const Address & other ) const;

protected:

    void Parse( const char * address );
};

enum SocketError
{
    SOCKET_ERROR_NONE,
    SOCKET_ERROR_CREATE_FAILED,
    SOCKET_ERROR_SOCKOPT_IPV6_ONLY_FAILED,
    SOCKET_ERROR_BIND_IPV6_FAILED,
    SOCKET_ERROR_BIND_IPV4_FAILED,
    SOCKET_ERROR_SET_NON_BLOCKING_FAILED
};

class Socket
{
public:

    Socket( uint16_t port, bool ipv6 = true );

    ~Socket();

    bool SendPacket( const Address & address, const uint8_t * data, int bytes );

    int ReceivePacket( Address & sender, uint8_t * buffer, int buffer_size );

    uint16_t GetPort() { return m_port; }

    SocketError GetError() const { return m_error; }

private:

    int m_socket;
    uint16_t m_port;
    SocketError m_error;

    Socket( Socket & other );
    Socket & operator = ( Socket & other );
};

#endif // #ifndef NETWORK_H
