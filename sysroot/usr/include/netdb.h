#ifndef _NETDB_H
#define _NETDB_H 1

#include <sys/socket.h>
#include <netinet/in.h>
// #include <inttypes.h>

struct hostent {
    char * h_name; //< Official name of the host.
    char ** h_aliases; //< A pointer to an array of pointers to alternative host names, terminated by a NULL pointer.
    int h_addrtype; //< The type of address being returned.
    int h_length; //< The length of the address in bytes.
    char ** h_addr_list; //< A pointer to an array of pointers to network addresses for the host (in network byte order), terminated by a NULL pointer.
};

struct netent {
    char * n_name; //< Official, fully-qualified name of the host.
    char ** n_aliases; //< A pointer to an array of pointers to alternative network names, terminated by a NULL pointer.
    int n_addrtype; //< The type of address being returned.
    uint32_t n_net; //< The network number in host byte order.
};


struct protoent {
    char * p_name; //< Official name of the protocol.
    char ** p_aliases; //< A pointer to an array of pointers to alternative protocol names, terminated by a NULL pointer.
    int p_proto; //< The protocol number.
};

struct servent {
    char * s_name; //< Official name of the service.
    char ** s_aliases; //< A pointer to an array of pointers to alternative service names, terminated by a NULL pointer.
    int s_port; //< The port number at which the service resides, in network byte order.
    char * s_proto; //< The name of the protocol to use when contacting the service.
};

static const unsigned IPPORT_RESERVED = 1024; //< Highest reserved Internet port number.


/// Address information structure
struct addrinfo {
    int ai_flags; //< Input flags.
    int ai_family; //< Address family of socket.
    int ai_socktype; //< Socket type.
    int ai_protocol; //< Protocol of socket.
    socklen_t ai_addrlen; //< Length of socket address.
    struct sockaddr * ai_addr; //< Socket address of socket.
    char * ai_canonname; //< Canonical name of service location.
    struct addrinfo * ai_next; //< Pointer to next in list.
};

/// Flags field values for addrinfo structure
enum {
    AI_PASSIVE = 0x0001, //< Socket address is intended for `bind`.
    AI_CANONNAME = 0x0002, //< Request for canonical name.
    AI_NUMERICHOST = 0x0004, //< Return numeric host address as name.
    AI_NUMERICSERV = 0x0008, //< Inhibit service name resolution.
    AI_V4MAPPED = 0x0010, //< IPv4 mapped addresses are acceptable.
    AI_ALL = 0x0020, //< Return both IPv4 and IPv6 addresses.
    AI_ADDRCONFIG = 0x0040, //< Use configuration of this host to choose returned address type.
    AI_DEFAULT = (AI_V4MAPPED | AI_ADDRCONFIG), //< Default flags for `getaddrinfo`.
};

/// Bitwise-distinct constants for flags argument of getnameinfo()
/// @see getnameinfo()
enum {
    NI_NOFQDN = 0x0001, //< Only return nodename portion of the FQDN for local hosts.
    NI_NUMERICHOST = 0x0002, //< Return numeric form of the host's address.
    NI_NAMEREQD = 0x0004, //< Error if the host's name not in DNS.
    NI_NUMERICSERV = 0x0008, //< Return numeric form of the service (port #).
    NI_NUMERICSCOPE = 0x0010, //< Return the numeric form of the scope.
    NI_DGRAM = 0x0020, //< Service is a datagram service. (SOCK_DGRAM)
};

/// @brief Address information errors
///
/// These error constants are used by `getaddrinfo()` and `getnameinfo()`.
enum {
    EAI_AGAIN = 1, //< Temporary failure in name resolution.
    EAI_BADFLAGS = 2, //< Invalid value for `ai_flags` field.
    EAI_FAIL = 3, //< Non-recoverable failure in name resolution.
    EAI_FAMILY = 4, //< Address family `ai_family` not supported.
    EAI_MEMORY = 5, //< Memory allocation failure.
    EAI_NONAME = 6, //< `host` nor service provided, or not known.
    EAI_SERVICE = 7, //< `service` not supported for `ai_socktype`.
    EAI_SOCKTYPE = 8, //< Socket type `ai_socktype` not supported.
    EAI_SYSTEM = 9, //< System error returned in `errno`.
    EAI_OVERFLOW = 10, //< Argument buffer overflow.

    EAI_ADDRFAMILY = 11, //< Address family for `host` not supported.
    EAI_NODATA = 12, //< No address associated with `host`.
};


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__
void              endhostent(void);
void              endnetent(void);
void              endprotoent(void);
void              endservent(void);
void              freeaddrinfo(struct addrinfo *);
const char       *gai_strerror(int);
int               getaddrinfo(const char *restrict, const char *restrict,
                      const struct addrinfo *restrict,
                      struct addrinfo **restrict);
struct hostent   *gethostent(void);
int               getnameinfo(const struct sockaddr *restrict, socklen_t,
                      char *restrict, socklen_t, char *restrict,
                      socklen_t, int);
struct netent    *getnetbyaddr(uint32_t, int);
struct netent    *getnetbyname(const char *);
struct netent    *getnetent(void);
struct protoent  *getprotobyname(const char *);
struct protoent  *getprotobynumber(int);
struct protoent  *getprotoent(void);
struct servent   *getservbyname(const char *, const char *);
struct servent   *getservbyport(int, const char *);
struct servent   *getservent(void);
void              sethostent(int);
void              setnetent(int);
void              setprotoent(int);
void              setservent(int);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _NETDB_H