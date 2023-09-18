#ifndef _NETINET_TCP_H
#define _NETINET_TCP_H

enum {
    TCP_NODELAY = 1, //< Avoid coalescing of small segments.
    TCP_MAXSEG = 2, //< Set maximum segment size.
    TCP_CORK = 3, //< Never send partially complete segments.
    TCP_KEEPIDLE = 4, //< Set time before keepalive probes are sent.
    TCP_KEEPINTVL = 5, //< Set time between keepalive probes.
    TCP_KEEPCNT = 6, //< Set number of keepalive probes.
    TCP_SYNCNT = 7, //< Set number of SYN retransmits.
    // TODO: Add more.
};

#endif // _NETINET_TCP_H