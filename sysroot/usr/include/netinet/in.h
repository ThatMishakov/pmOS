#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#include <inttypes.h>
#include <sys/socket.h>
// #include <arpa/inet.h>
#include <stdint.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family; //< Address family (AF_INET)
    in_port_t sin_port; //< Port number
    struct in_addr sin_addr; //< IP address
    char sin_zero[8]; //< Not used
};

struct in6_addr {
    uint8_t s6_addr[16]; //< IPv6 address
};

struct socketaddr_in6 {
    sa_family_t sin6_family; //< AF_INET6
    in_port_t sin6_port; //< port number
    uint32_t sin6_flowinfo; //< IPv6 flow information
    struct in6_addr sin6_addr; //< IPv6 address
    uint32_t sin6_scope_id; //< Scope ID
};

extern const struct in6_addr in6addr_any; //< IPv6 wildcard address
extern const struct in6_addr in6addr_loopback; //< IPv6 loopback address

struct ipv6_mreq {
    struct in6_addr ipv6mr_multiaddr; //< IPv6 multicast address
    unsigned int ipv6mr_interface; //< Interface index
};

#define IPPROTO_IP 0 //< Internet protocol
#define IPPROTO_ICMP 1 //< Internet Control Message Protocol
#define IPPROTO_TCP 6 //< Transmission Control Protocol
#define IPPROTO_UDP 17 //< User Datagram Protocol
#define IPPROTO_IPV6 41 //< Internet Protocol Version 6
#define IPPROTO_RAW 255 //< Raw IP packets

#define INADDR_ANY ((in_addr_t)0x00000000) //< INADDR_ANY
#define INADDR_BROADCAST ((in_addr_t)0xffffffff) //< INADDR_BROADCAST

#define INET_ADDRSTRLEN 16 //< Length of the string form for IPv4
#define INET6_ADDRSTRLEN 46 //< Length of the string form for IPv6

#define IPV6_JOIN_GROUP 12 //< Join a group membership
#define IPV6_LEAVE_GROUP 13 //< Leave a group membership

enum {
    IPV6_UNICAST_HOPS = 16, //< Set the hop limit for outgoing multicast packets
    IPV6_MULTICAST_IF = 17, //< Set the interface for outgoing multicast packets
    IPV6_MULTICAST_HOPS = 18, //< Set the hop limit for outgoing multicast packets
    IPV6_MULTICAST_LOOP = 19, //< Set whether outgoing multicast packets should be looped back to the local sockets
    IPV6_ADD_MEMBERSHIP = 20, //< Join a group membership
    IPV6_DROP_MEMBERSHIP = 21, //< Leave a group membership
    IPV6_V6ONLY = 26, //< Set whether an IPv6 socket is IPv6 only

    IP_MULTICAST_IF = 32, //< Set the interface for outgoing multicast packets
    IP_MULTICAST_TTL = 33, //< Set the time-to-live for outgoing multicast packets
    IP_MULTICAST_LOOP = 34, //< Set whether outgoing multicast packets should be looped back to the local sockets
};

/// Unspecified address
#define IN6_IS_ADDR_UNSPECIFIED(a) \
    (((uint32_t *)(a))[0] == 0 && ((uint32_t *)(a))[1] == 0 && ((uint32_t *)(a))[2] == 0 && ((uint32_t *)(a))[3] == 0)

/// Loopback address
#define IN6_IS_ADDR_LOOPBACK(a) \
    (((uint32_t *)(a))[0] == 0 && ((uint32_t *)(a))[1] == 0 && ((uint32_t *)(a))[2] == 0 && ((uint32_t *)(a))[3] == 1)

/// Multicast address
#define IN6_IS_ADDR_MULTICAST(a) (((uint8_t *)(a))[0] == 0xff)

/// Unicast link-local address
#define IN6_IS_ADDR_LINKLOCAL(a) \
    ((((uint32_t *)(a))[0] & 0xffc00000) == 0xfe800000 && ((uint32_t *)(a))[1] == 0 && ((uint32_t *)(a))[2] == 0 && \
     ((uint32_t *)(a))[3] == 0)

/// Unicast site-local address
#define IN6_IS_ADDR_SITELOCAL(a) \
    ((((uint32_t *)(a))[0] & 0xffc00000) == 0xfec00000 && ((uint32_t *)(a))[1] == 0 && ((uint32_t *)(a))[2] == 0 && \
     ((uint32_t *)(a))[3] == 0)

/// IPv4 mapped address.
#define IN6_IS_ADDR_V4MAPPED(a) \
    (((uint32_t *)(a))[0] == 0 && ((uint32_t *)(a))[1] == 0 && ((uint32_t *)(a))[2] == 0xffff)

/// Multicast node-local address
#define IN6_IS_ADDR_MC_NODELOCAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0x0f) == 0x01))

/// Multicast link-local address
#define IN6_IS_ADDR_MC_LINKLOCAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0x0f) == 0x02))

/// Multicast site-local address
#define IN6_IS_ADDR_MC_SITELOCAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0x0f) == 0x05))

/// Multicast organization-local address
#define IN6_IS_ADDR_MC_ORGLOCAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0x0f) == 0x08))

/// Multicast global address
#define IN6_IS_ADDR_MC_GLOBAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0x0f) == 0x0e))

#endif // _NETINET_IN_H