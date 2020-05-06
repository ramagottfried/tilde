/* udpsend.c 20060424. Martin Peach did it based on x_net.c. x_net.c header follows: */
/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* network */

#include "m_pd.h"

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h> // for interface addresses
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/ioctl.h> // for SIOCGIFCONF
#include <net/if.h> // for SIOCGIFCONF
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#endif // _WIN32
#ifdef __APPLE__
#include <ifaddrs.h> // for getifaddrs
#endif // __APPLE__

static t_class *udpsend_class;

typedef struct _udpsend
{
    t_object        x_obj;
    int             x_fd; /* the socket */
    struct sockaddr_in6 server_addr;
    unsigned int    x_multicast_loop_state;
    unsigned int    x_multicast_ttl; /* time to live for multicast */
} t_udpsend;

void udpsend_setup(void);
static void udpsend_free(t_udpsend *x);
static void udpsend_send(t_udpsend *x, t_symbol *s, int argc, t_atom *argv);
static void udpsend_disconnect(t_udpsend *x);
static void udpsend_connect(t_udpsend *x, t_symbol *hostname, t_floatarg fportno);
static void udpsend_set_multicast_loopback(t_udpsend *x, t_floatarg loop_state);
static void udpsend_set_multicast_ttl(t_udpsend *x, t_floatarg ttl_hops);
static void udpsend_set_multicast_interface (t_udpsend *x, t_symbol *s, int argc, t_atom *argv);
static void udpsend_sock_err(t_udpsend *x, char *err_string);
static void *udpsend_new(void);

static void *udpsend_new(void)
{
    t_udpsend *x = (t_udpsend *)pd_new(udpsend_class);
    outlet_new(&x->x_obj, &s_float);
    x->x_fd = -1;
    memset(&x->server_addr, 0, sizeof(x->server_addr));
    
    return (x);
}

static void udpsend_connect(t_udpsend *x, t_symbol *hostname,
    t_floatarg fportno)
{
    struct sockaddr_in6  server;
    struct hostent      *hp;
    int                 sockfd;
    int                 portno = fportno;
    int                 broadcast = 1;/* nonzero is true */
    unsigned char       multicast_loop_state;
    unsigned char       multicast_ttl;
    unsigned int        size;

    if (x->x_fd >= 0)
    {
        pd_error(x, "udpsend: already connected");
        return;
    }

    /* create a socket */
    sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
#ifdef DEBUG
    fprintf(stderr, "udpsend_connect: send socket %d\n", sockfd);
#endif
    if (sockfd < 0)
    {
        udpsend_sock_err(x, "udpsend socket");
        return;
    }
/* Based on zmoelnig's patch 2221504:
Enable sending of broadcast messages (if hostname is a broadcast address)*/
#ifdef SO_BROADCAST
    if( 0 != setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (const void *)&broadcast, sizeof(broadcast)))
    {
        udpsend_sock_err(x, "couldn't switch to broadcast mode");
    }
#endif /* SO_BROADCAST */
    
    /* connect socket using hostname provided in command line */
    server.sin6_family = AF_INET6;
    hp = gethostbyname2(hostname->s_name, AF_INET6);
    if (hp == 0)
    {
        post("udpsend: bad host?\n");
        return;
    }

//    memcpy((char *)&server.sin6_addr, (char *)hp->h_addr, hp->h_length);

    inet_pton(AF_INET6, hostname->s_name, &server.sin6_addr);

    if(IN6_IS_ADDR_MULTICAST(&server.sin6_addr))
        post ("udpsend: connecting to a multicast address");

    
//    if (0xE0000000 == (ntohl(server.sin6_addr.s_addr) & 0xF0000000))
//        post ("udpsend: connecting to a multicast address");
    size = sizeof(multicast_loop_state);
    getsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &multicast_loop_state, &size);
    size = sizeof(multicast_ttl);
    getsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &multicast_ttl, &size);
    x->x_multicast_loop_state = multicast_loop_state;
    x->x_multicast_ttl = multicast_ttl;
    
    post("multicast loop %d", (int)multicast_loop_state);
    
    /* assign client port number */
    server.sin6_port = htons((u_short)portno);

    post("udpsend: connecting to port %d", portno);
    /* try to connect. */

    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        udpsend_sock_err(x, "udpsend connect");
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
        return;
    }

    x->x_fd = sockfd;
    x->server_addr = server;
    outlet_float(x->x_obj.ob_outlet, 1);
}

static void udpsend_set_multicast_loopback(t_udpsend *x, t_floatarg loop_state)
{
    int             sockfd = x->x_fd;
    unsigned char   multicast_loop_state = loop_state;
    unsigned int    size;

    if (x->x_fd < 0)
    {
        pd_error(x, "udpsend_set_multicast_loopback: not connected");
        return;
    }
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
        &multicast_loop_state, sizeof(multicast_loop_state)) < 0) 
        udpsend_sock_err(x, "udpsend setsockopt IPV6_MULTICAST_LOOP");
    getsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &multicast_loop_state, &size);
    x->x_multicast_loop_state = multicast_loop_state;
}

static void udpsend_set_multicast_ttl(t_udpsend *x, t_floatarg ttl_hops)
{
    int             sockfd = x->x_fd;
    unsigned char   multicast_ttl = ttl_hops;
    unsigned int    size;

    if (x->x_fd < 0)
    {
        pd_error(x, "udpsend_set_multicast_ttl: not connected");
        return;
    }
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
        &multicast_ttl, sizeof(multicast_ttl)) < 0) 
        udpsend_sock_err(x, "udpsend setsockopt IPV6_MULTICAST_HOPS");
    getsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &multicast_ttl, &size);
    x->x_multicast_ttl = multicast_ttl;
}

static void udpsend_set_multicast_interface (t_udpsend *x, t_symbol *s, int argc, t_atom *argv)
{
#ifdef _WIN32
    int                 i, n_ifaces = 32;
    PMIB_IPADDRTABLE    pIPAddrTable;
    DWORD               dwSize;
    IN_ADDR             IPAddr;
    int                 if_index = -1;
    int                 found = 0;
    t_symbol            *interfacename = gensym("none");
    struct hostent      *hp = 0;
    struct sockaddr_in6  server;

    if (x->x_fd < 0)
    {
        pd_error(x, "udpsend_set_multicast_interface: not connected");
        return;
    }
    switch (argv[0].a_type)
    {
        case A_FLOAT:
            if_index = (int)atom_getfloat(&argv[0]);
            break;
        case A_SYMBOL:
            interfacename = atom_getsymbol(&argv[0]);    
            break;
        default:
            pd_error(x, "udpsend_set_multicast_interface: argument not float or symbol");
            return;
    }
    if (if_index == -1)
    {
        hp = gethostbyname2(interfacename->s_name, AF_INET6); // if interface is a dotted or named IP address (192.168.0.88)
    }
    if (hp != 0) memcpy((char *)&server.sin6_addr, (char *)hp->h_addr, hp->h_length);
    else // maybe interface is its index (1) (names aren't available in _WIN32)
    {
        /* get the list of interfaces, IPv4 only */
        dwSize = sizeof(MIB_IPADDRTABLE)*n_ifaces;
        if ((pIPAddrTable = (MIB_IPADDRTABLE *) getbytes(dwSize)) == NULL)
        {
            post("udpsend: unable to allocate %lu bytes for GetIpAddrTable", dwSize);
            return;
        }
        if (GetIpAddrTable(pIPAddrTable, &dwSize, 0))
        { 
            udpsend_sock_err(x, "udpsend_set_multicast_interface: GetIpAddrTable");
            return;
        }

        n_ifaces = pIPAddrTable->dwNumEntries;
        post("udpsend: %d interface%s available:", n_ifaces, (n_ifaces == 1)?"":"s");
        for (i = 0; i < n_ifaces; i++)
        {
            IPAddr.S_un.S_addr = (u_long) pIPAddrTable->table[i].dwAddr;
            post("[%d]: %s", pIPAddrTable->table[i].dwIndex, inet_ntop(IPAddr));
            if (pIPAddrTable->table[i].dwIndex == if_index)
            {
                server.sin6_addr = IPAddr;
                found = 1;
            }
        }

        if (pIPAddrTable)
        {
            freebytes(pIPAddrTable, dwSize);
            pIPAddrTable = NULL;
        }
        if (! found)
        {
            post("udpsend_set_multicast_interface: bad host name? (%s)\n", interfacename->s_name);
            return;
        }
    }
    if (setsockopt(x->x_fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char *)&server.sin6_addr, sizeof(struct in6_addr)) == SOCKET_ERROR)
        udpsend_sock_err(x, "udpsend setsockopt IP_MULTICAST_IF");
    else post("udpsend multicast interface is %s", inet_ntop(server.sin6_addr));

#elif defined __APPLE__
    int                 if_index = -1;
    int                 found = 0;
    t_symbol            *interfacename = gensym("none");
    struct ifaddrs      *ifap;
    int                 i = 0;
    int                 n_ifaces = 0;
    struct hostent      *hp = 0;
    struct sockaddr_in6  server;
    struct sockaddr     *sa;
    char                ifname[IFNAMSIZ]; /* longest possible interface name */
    
    if (x->x_fd < 0)
    {
        pd_error(x, "udpsend_set_multicast_interface: not connected");
        return;
    }
    switch (argv[0].a_type)
    {
        case A_FLOAT:
            if_index = (int)atom_getfloat(&argv[0]);
            break;
        case A_SYMBOL:
            interfacename = atom_getsymbol(&argv[0]);    
            break;
        default:
            pd_error(x, "udpsend_set_multicast_interface: argument not float or symbol");
            return;
    }
    if (if_index == -1)
    {
        hp = gethostbyname2(interfacename->s_name, AF_INET6); // if interface is a dotted or named IP address (192.168.0.88)
    }
    if (hp != 0) memcpy((char *)&server.sin6_addr, (char *)hp->h_addr, hp->h_length);
    else // maybe interface is its name (eth0) or index (1)
    { // scan all the interfaces to get the IP address of interface
        if (getifaddrs(&ifap)) udpsend_sock_err(x, "udpsend getifaddrs");
        i = found = n_ifaces = 0;
        while (NULL != ifap)
        {
            sa = ifap->ifa_addr;
            if (sa->sa_family == AF_INET6)
            {
                ++n_ifaces;
                strncpy (ifname, ifap->ifa_name, IFNAMSIZ);
                char buf[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), buf, INET6_ADDRSTRLEN );
                post("[%d]: %s: %s", i, ifname, buf);
                if((i == if_index) || ((if_index == -1) && (!strncmp(interfacename->s_name, ifname, IFNAMSIZ))))
                { // either the index or the name match
                    server.sin6_addr = ((struct sockaddr_in6 *)sa)->sin6_addr;
                    found = 1;
                }
            }
            i++;
            ifap = ifap->ifa_next; // next record or NULL
        }
        freeifaddrs(ifap);
        post ("udpsend: %d interfaces", n_ifaces);
        if (!found) return;
    }
    if (setsockopt(x->x_fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char *)&server.sin6_addr, sizeof(struct in6_addr)))
        udpsend_sock_err(x, "udpsend setsockopt IP_MULTICAST_IF");
    else
    {
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), buf, INET6_ADDRSTRLEN );
        post("udpsend multicast interface is %s", buf);
    }
    return;
#else // __linux__
    struct sockaddr_in6  server;
    struct sockaddr     *sa;
    struct hostent      *hp = 0;
    struct ifconf       ifc;
    int                 n_ifaces = 32, i, origbuflen, found = 0;
    char                ifname[IFNAMSIZ]; /* longest possible interface name */
    t_symbol            *interface = gensym("none");
    int                 if_index = -1;
    
    if (x->x_fd < 0)
    {
        pd_error(x, "udpsend_set_multicast_interface: not connected");
        return;
    }
    switch (argv[0].a_type)
    {
        case A_FLOAT:
            if_index = (int)atom_getfloat(&argv[0]);
            break;
        case A_SYMBOL:
            interface = atom_getsymbol(&argv[0]);    
            break;
        default:
            pd_error(x, "udpsend_set_multicast_interface: argument not float or symbol");
            return;
    }
    if (if_index == -1)
    {
        hp = gethostbyname2(interface->s_name, AF_INET6); // if interface is a dotted or named IP address (192.168.0.88)
    }
    if (hp != 0) memcpy((char *)&server.sin6_addr, (char *)hp->h_addr, hp->h_length);
    else // maybe interface is its name (eth0) or index (1)
    { // scan all the interfaces to get the IP address of interface
        // find the number of interfaces
        origbuflen = n_ifaces * sizeof (struct ifreq);// save maximum length for free()
        ifc.ifc_len = origbuflen; // SIOCGIFCONF changes it to valid length
        ifc.ifc_buf = (char*)getzbytes(origbuflen);
        if (ifc.ifc_buf != NULL)
        { // 
            if (ioctl(x->x_fd, SIOCGIFCONF, &ifc) < 0) // get list of interfaces
                udpsend_sock_err(x, "udpsend_set_multicast_interface: getting list of interfaces");
            else
            {
                n_ifaces = ifc.ifc_len/sizeof(struct ifreq);
                post("udpsend: %d interface%s available:", n_ifaces, (n_ifaces == 1)?"":"s");
                for(i = 0; i < n_ifaces; i++)
                {
                    sa = (struct sockaddr *)&(ifc.ifc_req[i].ifr_addr);
                    strncpy (ifname, ifc.ifc_req[i].ifr_name, IFNAMSIZ);
                    post("[%d]: %s: %s", i, ifname, inet_ntop(((struct sockaddr_in6 *)sa)->sin6_addr));
                    if
                    (
                        (i == if_index) ||
                        ((if_index == -1) && (!strncmp(interface->s_name, ifname, IFNAMSIZ)))
                    )
                    {
                        server.sin6_addr = ((struct sockaddr_in6 *)sa)->sin6_addr;
                        found = 1;
                    }
                } 
            }
        }
        freebytes(ifc.ifc_buf, origbuflen);

        if (! found)
        {
            post("udpsend_set_multicast_interface: bad host name? (%s)\n", interface->s_name);
            return;
        }
    }
    if (setsockopt(x->x_fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &server.sin6_addr, sizeof(struct in6_addr)) < 0)
        udpsend_sock_err(x, "udpsend_set_multicast_interface: setsockopt");
    else post("udpsend multicast interface is %s", inet_ntop(server.sin6_addr));
#endif // _WIN32
}

static void udpsend_sock_err(t_udpsend *x, char *err_string)
{
/* prints the last error from errno or WSAGetLastError() */
#ifdef _WIN32
    void            *lpMsgBuf;
    unsigned long   errornumber = WSAGetLastError();
    int             len = 0, i;
    char            *cp;

    if (len = FormatMessageA((FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS)
        , NULL, errornumber, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&lpMsgBuf, 0, NULL))
    {
        cp = (char *)lpMsgBuf;
        for(i = 0; i < len; ++i)
        {
            if (cp[i] < 0x20)
            { /* end string at first weird character */
                cp[i] = 0;
                break;
            }
        }
        pd_error(x, "%s: %s (%d)", err_string, lpMsgBuf, errornumber);
        LocalFree(lpMsgBuf);
    }
#else
    pd_error(x, "%s: %s (%d)", err_string, strerror(errno), errno);
#endif
}

static void udpsend_disconnect(t_udpsend *x)
{
    if (x->x_fd >= 0)
    {
#ifdef _WIN32
        closesocket(x->x_fd);
#else
        close(x->x_fd);
#endif
        x->x_fd = -1;
        outlet_float(x->x_obj.ob_outlet, 0);
    }
}

static void udpsend_send(t_udpsend *x, t_symbol *s, int argc, t_atom *argv)
{
#define BYTE_BUF_LEN 65536 // arbitrary maximum similar to max IP packet size
    static char    byte_buf[BYTE_BUF_LEN];
    int            d;
    int            i, j;
    unsigned char  c;
    float          f, e;
    char           *bp;
    int            length, sent;
    int            result;
    static double  lastwarntime;
    static double  pleasewarn;
    double         timebefore;
    double         timeafter;
    int            late;
    char           fpath[FILENAME_MAX];
    FILE           *fptr;

#ifdef DEBUG
    post("s: %s", s->s_name);
    post("argc: %d", argc);
#endif
    for (i = j = 0; i < argc; ++i)
    {
        if (argv[i].a_type == A_FLOAT)
        {
            f = argv[i].a_w.w_float;
            d = (int)f;
            e = f - d;
            if (e != 0)
            {
                pd_error(x, "udpsend_send: item %d (%f) is not an integer", i, f);
                return;
            }
            c = (unsigned char)d;
            if (c != d)
            {
                pd_error(x, "udpsend_send: item %d (%f) is not between 0 and 255", i, f);
                return;
            }
#ifdef DEBUG
            post("udpsend_send: argv[%d]: %d", i, c);
#endif
            byte_buf[j++] = c;
        }
        else if (argv[i].a_type == A_SYMBOL)
        {

            atom_string(&argv[i], fpath, FILENAME_MAX);
#ifdef DEBUG
            post ("udpsend fname: %s", fpath);
#endif
            fptr = sys_fopen(fpath, "rb");
            if (fptr == NULL)
            {
                post("udpsend: unable to open \"%s\"", fpath);
                return;
            }
            rewind(fptr);
#ifdef DEBUG
            post("udpsend: d is %d", d);
#endif
            while ((d = fgetc(fptr)) != EOF)
            {
                byte_buf[j++] = (char)(d & 0x0FF);
#ifdef DEBUG
                post("udpsend: byte_buf[%d] = %d", j-1, byte_buf[j-1]);
#endif
                if (j >= BYTE_BUF_LEN)
                {
                    post ("udpsend: file too long, truncating at %lu", BYTE_BUF_LEN);
                    break;
                }
            }
            fclose(fptr);
            fptr = NULL;
            post("udpsend: read \"%s\" length %d byte%s", fpath, j, ((d==1)?"":"s"));
        }
        else
        {
            pd_error(x, "udpsend_send: item %d is not a float or a file name", i);
            return;
        }
    }

    length = j;
    if ((x->x_fd >= 0) && (length > 0))
    {
        for (bp = byte_buf, sent = 0; sent < length;)
        {
            timebefore = sys_getrealtime();
            result = send(x->x_fd, byte_buf, (ssize_t)(length - sent), 0);
            
            //using sendto could allow to change the output address, if used remove connect() above
           // result = sendto(x->x_fd, byte_buf, (ssize_t)(length - sent), 0, (struct sockaddr *)&x->server_addr, sizeof(x->server_addr));
            timeafter = sys_getrealtime();
            late = (timeafter - timebefore > 0.005);
            if (late || pleasewarn)
            {
                if (timeafter > lastwarntime + 2)
                {
                    post("udpsend blocked %d msec",
                        (int)(1000 * ((timeafter - timebefore) + pleasewarn)));
                    pleasewarn = 0;
                    lastwarntime = timeafter;
                }
                else if (late) pleasewarn += timeafter - timebefore;
            }
            if (result <= 0)
            {
                udpsend_sock_err(x, "udpsend send");
                udpsend_disconnect(x);
                break;
            }
            else
            {
                sent += result;
                bp += result;
            }
        }
    }
    else pd_error(x, "udpsend: not connected");
}

static void udpsend_free(t_udpsend *x)
{
    udpsend_disconnect(x);
}

void udpsend6_setup(void)
{
    udpsend_class = class_new(gensym("udpsend6"), (t_newmethod)udpsend_new, (t_method)udpsend_free, sizeof(t_udpsend), 0, 0);
    class_addmethod(udpsend_class, (t_method)udpsend_connect, gensym("connect"), A_SYMBOL, A_FLOAT, 0);
    class_addmethod(udpsend_class, (t_method)udpsend_set_multicast_ttl, gensym("multicast_ttl"), A_DEFFLOAT, 0);
    class_addmethod(udpsend_class, (t_method)udpsend_set_multicast_loopback, gensym("multicast_loopback"), A_DEFFLOAT, 0);
    class_addmethod(udpsend_class, (t_method)udpsend_set_multicast_interface, gensym("multicast_interface"), A_GIMME, 0);
    class_addmethod(udpsend_class, (t_method)udpsend_disconnect, gensym("disconnect"), 0);
    class_addmethod(udpsend_class, (t_method)udpsend_send, gensym("send"), A_GIMME, 0);
    class_addlist(udpsend_class, (t_method)udpsend_send);
}

/* end udpsend.c*/

