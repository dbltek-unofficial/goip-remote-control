#include <alloca.h>
#include <arpa/inet.h>
#include <linux/sockios.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <errno.h>

#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "debug.h"
#include "net.h"
#include "trace.h" /* added by kevin 2002-11-20 */

const char* get_hostname()
{
    static char hn[256];

    if (gethostname(hn, 255) < 0) {
        return 0;
    }

    return hn;
}

int gethostaddr(const char* hostname, struct sockaddr* addr, socklen_t salen)
{
    struct hostent* host;

    if (!hostname) {
        hostname = get_hostname();
    }

    if ((host = gethostbyname(hostname)) == NULL) {
        return -1;
    }
    memcpy(&((struct sockaddr_in*)addr)->sin_addr, &host->h_addr[0], sizeof(struct in_addr));
    addr->sa_family = AF_INET;
    return 0;
}

int getsockerr(int sock)
{
    int error, len = sizeof(error);

    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        return -1;
    }

    return error;
}

int tcp_connect(int sock, struct sockaddr* addr, socklen_t salen)
{
    int sock_created = 0;

    if (!is_valid_addr(addr, salen)) {
        return -1;
    }

    if (addr->sa_family == AF_INET) {
        struct linger l;

        if (sock <= 0) {
            if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                LOG_SYS_ERROR;
                return -1;
            }
            sock_created = 1;
        }

        l.l_onoff = 1;
        l.l_linger = 5;
        if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l)) < 0) {
            LOG_SYS_ERROR;
            goto failed;
        }

        if (connect(sock, addr, salen) < 0) {
            LOG_SYS_ERROR;
            goto failed;
        }
    } else {
        return -1;
    }

    return sock;

failed:
    if (sock_created) {
        DLOG("close %d", sock);
        close(sock);
    }
    return -1;
}

int tcp_bind(int sock, struct sockaddr* addr, socklen_t salen)
{
    int sock_created = 0;

    if (!is_valid_addr(addr, salen)) {
        return -1;
    }

    if (addr->sa_family == AF_INET) {
        unsigned long enable = 1L;

        if (sock <= 0) {
            if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                LOG_SYS_ERROR;
                return -1;
            }
            sock_created = 1;
        }

        /* Allow multiple instance of the client to share the same address and port */
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable)) < 0) {
            LOG_SYS_ERROR;
            goto failed;
        }

        if (bind(sock, addr, salen) < 0) {
            LOG_SYS_ERROR;
            goto failed;
        }
    } else {
        return -1;
    }

    return sock;

failed:
    if (sock_created)
        close(sock);
    return -1;
}

int tcp_listen(int sock, struct sockaddr* addr, socklen_t salen)
{
    if ((sock = tcp_bind(sock, addr, salen)) < 0) {
        return -1;
    }

    if (!is_valid_addr(addr, salen)) {
        return -1;
    }

    if (addr->sa_family == AF_INET) {
        struct linger l;

        l.l_onoff = 1;
        l.l_linger = 5;
        if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l)) < 0) {
            LOG_SYS_ERROR;
            return -1;
        }

        if (listen(sock, 8) < 0) {
            LOG_SYS_ERROR;
            return -1;
        }
    } else {
        return -1;
    }

    return sock;
}

int udp_connect(int sock, struct sockaddr* addr, socklen_t salen)
{
    int sock_created = 0;

    if (!is_valid_addr(addr, salen)) {
        return -1;
    }

    if (addr->sa_family == AF_INET) {
        if (sock <= 0) {
            if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
                LOG_SYS_ERROR;
                return -1;
            }
            sock_created = 1;
        }

        if (connect(sock, addr, sizeof(struct sockaddr)) < 0) {
            LOG_SYS_ERROR;
            goto failed;
        }
    } else {
        return -1;
    }

    return sock;

failed:
    if (sock_created)
        close(sock);
    return -1;
}

int udp_bind(int sock, struct sockaddr* addr, socklen_t salen)
{
    int sock_created = 0;
    struct sockaddr_in* lpst_inaddr; /* added by Kevin 2003-1-14 */
    int i;
    unsigned short lui_port;

    if (!is_valid_addr(addr, salen)) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    if (addr->sa_family == AF_INET) {
        unsigned long enable = 1L;
        lpst_inaddr = (struct sockaddr_in*)addr; /* added by Kevin 2003-1-14 */

        /* Create a UDP socket */
        if (sock <= 0) {
            if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                LOG_SYS_ERROR;
                return -1;
            }
            sock_created = 1;
        }

        /* Allow multiple instance of the client to share the same address and port */
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(unsigned long)) < 0) {
            LOG_SYS_ERROR;
            goto failed;
        }

        /* If the address is multicast, register to the multicast group */
        if (IN_IS_ADDR_MULTICAST(((struct sockaddr_in*)addr)->sin_addr.s_addr)) {
            struct sockaddr_in lca;
            struct ip_mreq mreq;

            bzero(&lca, sizeof(lca));

            /* Bind the socket to port */
            lca.sin_family = AF_INET;
            lca.sin_addr.s_addr = htonl(INADDR_ANY);
            lca.sin_port = ((struct sockaddr_in*)addr)->sin_port;
            if (bind(sock, (struct sockaddr*)&lca, sizeof(lca)) < 0) {
                LOG_SYS_ERROR;
                goto failed;
            }

            /* Register to a multicast address */
            mreq.imr_multiaddr.s_addr = ((struct sockaddr_in*)addr)->sin_addr.s_addr;
            mreq.imr_interface.s_addr = INADDR_ANY;
            if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
                LOG_SYS_ERROR;
                goto failed;
            }
        } else {
            /* modified by Kevin 2003-1-24 add a port assigned function to
             * correct a bug in the current Linux Kernel
             */
            if (lpst_inaddr->sin_port == 0) {
                for (i = 0; i < 100; i++) {
                    lui_port = random() & 0xffff;
                    if (lui_port < 5000)
                        lui_port += 5000;
                    lpst_inaddr->sin_port = lui_port;
                    if (bind(sock, addr, salen) < 0) {
                        if (errno != EINVAL) {
                            LOG_SYS_ERROR;
                            goto failed;
                        }
                    } else
                        break;
                }
                if (i == 100) {
                    LOG_SYS_ERROR;
                    goto failed;
                }
            } else {
                /* Bind the socket to port */
                if (bind(sock, addr, salen) < 0) {
                    LOG_SYS_ERROR;
                    goto failed;
                }
            }
        }
    } else {
        return -1;
    }

    return sock;

failed:
    if (sock_created)
        close(sock);
    return -1;
}

int unix_listen(int sock, const char* path)
{
    int sock_created = 0;
    struct sockaddr_un addr;

    if (sock < 0) {
        if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            LOG_SYS_ERROR;
            return -1;
        }
        sock_created = 1;
    }
    unlink(path);

    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        LOG_SYS_ERROR;
        goto failed;
    }

    if (listen(sock, 1) != 0) {
        LOG_SYS_ERROR;
        goto failed;
    }

    return sock;

failed:
    if (sock_created)
        close(sock);
    return -1;
}

int unix_connect(int sock, const char* path)
{
    int sock_created = 0;
    int len;
    struct sockaddr_un addr;

    if (sock < 0) {
        if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            LOG_SYS_ERROR;
            return -1;
        }
        sock_created = 1;
    }

    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    len = strlen(addr.sun_path) + sizeof(addr.sun_family);

    if (connect(sock, (struct sockaddr*)&addr, len) != 0) {
        LOG_SYS_ERROR;
        goto failed;
    }

    return sock;

failed:
    if (sock_created)
        close(sock);
    return -1;
}

int send_ctlmsg(int ctlsock, struct ctlmsg* ctlmsg)
{
    struct msghdr msg;
    struct iovec iov[2];
    int len;

    bzero(&msg, sizeof(msg));
    bzero(iov, sizeof(struct iovec) * 2);

    iov[0].iov_base = ctlmsg->name;
    iov[0].iov_len = strlen(ctlmsg->name) + 1;
    iov[1].iov_base = ctlmsg->data;
    iov[1].iov_len = ctlmsg->data_len;
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;

    if (ctlmsg->numfds) {
        int *pfd, i;
        struct cmsghdr* cmsg;
        int cmsg_len;

        cmsg_len = CMSG_LEN(sizeof(int) * ctlmsg->numfds);
        cmsg = (struct cmsghdr*)alloca(cmsg_len);

        cmsg->cmsg_len = cmsg_len;
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;

        pfd = (int*)CMSG_DATA(cmsg);
        for (i = 0; i < ctlmsg->numfds; i++) {
            *pfd++ = ctlmsg->fds[i];
        }

        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg_len;
    }
    if ((len = sendmsg(ctlsock, &msg, 0)) < 0) {
        LOG_SYS_ERROR;
        return -1;
    }

    return len;
}

int recv_ctlmsg(int ctlsock, struct ctlmsg* ctlmsg)
{
    struct msghdr msg;
    struct iovec iov;
    char buf[4096];
    int len, data_len;

    bzero(&iov, sizeof(struct iovec));
    bzero(&msg, sizeof(msg));

    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (ctlmsg->numfds) {
        struct cmsghdr* cmsg;
        int cmsg_len;

        cmsg_len = CMSG_LEN(sizeof(int) * ctlmsg->numfds);
        cmsg = alloca(cmsg_len);
        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg_len;
        msg.msg_flags = 0;
    }

    if ((len = recvmsg(ctlsock, &msg, 0)) < 0) {
        LOG_SYS_ERROR;
        return -1;
    }
    data_len = len;

    strncpy(ctlmsg->name, buf, 32);
    len -= (strlen(ctlmsg->name) + 1);
    if (len > ctlmsg->data_len) {
        len = ctlmsg->data_len;
    }
    memcpy(ctlmsg->data, buf + strlen(ctlmsg->name) + 1, len);
    ctlmsg->data_len = len;

    if (msg.msg_controllen) {
        memcpy(ctlmsg->fds, CMSG_DATA((struct cmsghdr*)msg.msg_control),
            msg.msg_controllen - sizeof(struct cmsghdr));
    }
    len = data_len;

    return len;
}

int broadcast(int sock, void* msg, size_t len, int flags, unsigned port)
{
    int res = -1, on = 1;
    struct ifi_info *ifi_head, *ifi_p;

    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
        LOG_SYS_ERROR;
        return -1;
    }

    if (!(ifi_head = get_ifi_info(sock, IFI_BRDADDR))) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    ifi_p = ifi_head;
    while (ifi_p) {
        if (!ifi_p->ifi_brdaddr) {
            ifi_p = ifi_p->ifi_next;
            continue;
        }
        setsockport(ifi_p->ifi_brdaddr, port);
        if ((res = sendto(sock, msg, len, flags, ifi_p->ifi_brdaddr, socklen(ifi_p->ifi_brdaddr))) < 0) {
            LOG_SYS_ERROR;
            break;
        }
        ifi_p = ifi_p->ifi_next;
    }

    free_ifi_info(ifi_head);
    return res;
}

int wait_socket(int sock, int event, int timeout)
{
    struct pollfd pfd;
    int result;

    pfd.fd = sock;
    pfd.events = event;
    pfd.revents = 0;

    if ((result = poll(&pfd, 1, timeout)) <= 0) {
        return result;
    }

    return pfd.revents;
}

int ifaddr_match(struct sockaddr* sa, struct sockaddr* ifaddr)
{
    if (sa->sa_family != ifaddr->sa_family) {
        return 0;
    }

    switch (sa->sa_family) {
    case AF_INET: {
        struct sockaddr_in *sin1, *sin2;
        sin1 = (struct sockaddr_in*)sa;
        sin2 = (struct sockaddr_in*)ifaddr;
        if (!sin1->sin_addr.s_addr || sin1->sin_addr.s_addr == sin2->sin_addr.s_addr) {
            return 1;
        }
    } break;
    }
    return 0;
}

struct ifi_info*
get_ifi_info(int sock, int f_flags)
{
    struct ifi_info *ifi, *ifihead, **ifipnext;
    int family, len, lastlen, flags;
    char *ptr, *buf, lastname[IFNAMSIZ], *cptr;
    struct ifconf ifc;
    struct ifreq *ifr, ifrcopy;
    struct sockaddr_in* sinptr;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);

    if (getsockname(sock, (struct sockaddr*)&ss, &slen) < 0) {
        return 0;
    }
    family = ss.ss_family;

    lastlen = 0;
    len = 100 * sizeof(struct ifreq);
    while (1) {
        if (!(buf = malloc(len))) {
            return 0;
        }
        ifc.ifc_len = len;
        ifc.ifc_buf = buf;
        if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
            if (errno != EINVAL || lastlen != 0) {
                LOG_FUNCTION_FAILED;
                return 0;
            }
        } else {
            if (ifc.ifc_len == lastlen)
                break;
            lastlen = ifc.ifc_len;
        }
        len += 10 * sizeof(struct ifreq);
        free(buf);
    }

    ifihead = NULL;
    ifipnext = &ifihead;
    lastname[0] = 0;

    for (ptr = buf; ptr < buf + ifc.ifc_len;) {
        ifr = (struct ifreq*)ptr;

        switch (ifr->ifr_addr.sa_family) {
#ifdef IPV6
        case AF_INET6:
            len = sizeof(struct sockaddr_in6);
            break;
#endif
        case AF_INET:
        default:
            len = sizeof(struct sockaddr);
            break;
        }
        ptr += sizeof(ifr->ifr_name) + len;

        if (!ifaddr_match((struct sockaddr*)&ss, &ifr->ifr_addr))
            continue;

        if ((cptr = strchr(ifr->ifr_name, ':')) != NULL) {
            if (!(f_flags & IFI_ALIASES))
                continue;
        }
        memcpy(lastname, ifr->ifr_name, IFNAMSIZ);

        ifrcopy = *ifr;
        ioctl(sock, SIOCGIFFLAGS, &ifrcopy);
        flags = ifrcopy.ifr_flags;
        if ((flags & IFF_UP) == 0)
            continue;

        if ((flags & IFF_LOOPBACK) && !(f_flags & IFI_LOOPBACKS))
            continue;

        if (!(ifi = calloc(1, sizeof(struct ifi_info)))) {
            return ifihead;
        }
        *ifipnext = ifi;
        ifipnext = &ifi->ifi_next;

        ifi->ifi_flags = flags;
        memcpy(ifi->ifi_name, ifr->ifr_name, IFI_NAME);
        ifi->ifi_name[IFI_NAME - 1] = '\0';

        switch (ifr->ifr_addr.sa_family) {
        case AF_INET:
            sinptr = (struct sockaddr_in*)&ifr->ifr_addr;
            if (ifi->ifi_addr == NULL) {
                if (!(ifi->ifi_addr = calloc(1, sizeof(struct sockaddr_in)))) {
                    ;
                }
                sinptr->sin_port = ((struct sockaddr_in*)&ss)->sin_port;
                memcpy(ifi->ifi_addr, sinptr, sizeof(struct sockaddr_in));

                if ((flags & IFF_BROADCAST) && (f_flags & IFI_BRDADDR)) {
                    ioctl(sock, SIOCGIFBRDADDR, &ifrcopy);
                    sinptr = (struct sockaddr_in*)&ifrcopy.ifr_broadaddr;
                    ifi->ifi_brdaddr = calloc(1, sizeof(struct sockaddr_in));
                    memcpy(ifi->ifi_brdaddr, sinptr, sizeof(struct sockaddr_in));
                }

                if ((flags & IFF_POINTOPOINT) && (f_flags & IFI_DSTADDR)) {
                    ioctl(sock, SIOCGIFDSTADDR, &ifrcopy);
                    sinptr = (struct sockaddr_in*)&ifrcopy.ifr_dstaddr;
                    ifi->ifi_dstaddr = calloc(1, sizeof(struct sockaddr_in));
                    memcpy(ifi->ifi_dstaddr, sinptr, sizeof(struct sockaddr_in));
                }

                if (f_flags & IFI_NETMASK) {
                    ioctl(sock, SIOCGIFNETMASK, &ifrcopy);
                    sinptr = (struct sockaddr_in*)&ifrcopy.ifr_addr;
                    ifi->ifi_netmask = calloc(1, sizeof(struct sockaddr_in));
                    memcpy(ifi->ifi_netmask, sinptr, sizeof(struct sockaddr_in));
                }
            }
            break;
        default:
            break;
        }
    }

    free(buf);

    return ifihead;
}

void free_ifi_info(struct ifi_info* ifihead)
{
    struct ifi_info *ifi, *ifi_next;

    for (ifi = ifihead; ifi; ifi = ifi_next) {
        if (ifi->ifi_addr) {
            free(ifi->ifi_addr);
        }
        if (ifi->ifi_brdaddr) {
            free(ifi->ifi_brdaddr);
        }
        if (ifi->ifi_dstaddr) {
            free(ifi->ifi_dstaddr);
        }
        if (ifi->ifi_netmask) {
            free(ifi->ifi_netmask);
        }
        ifi_next = ifi->ifi_next;
        free(ifi);
    }
}

int is_lan_addr(unsigned long s_addr)
{
    if (/*(s_addr & 0xff) == 0 ||*/ (s_addr & 0xff) == 10 || (s_addr & 0xff) == 0x7f) {
        return 1;
    }
    if ((s_addr & 0xffff) == 0xa8c0) {
        return 1;
    }

    return 0;
}

int ifi_dest_match(struct ifi_info* ifi, struct sockaddr* dest)
{
    int score = 0;

    if (ifi->ifi_addr->sa_family != dest->sa_family) {
        DLOG("matching interface: %s(%d) not the same family(%d)\n", sock_ntop_host(ifi->ifi_addr, sizeof(struct sockaddr)), ifi->ifi_addr->sa_family, dest->sa_family);
        return -1;
    }

    switch (dest->sa_family) {
    case AF_INET: {
        unsigned long saddr, sdest, mask;
        int i;

        saddr = ((struct sockaddr_in*)ifi->ifi_addr)->sin_addr.s_addr;
        mask = ((struct sockaddr_in*)ifi->ifi_netmask)->sin_addr.s_addr;
        sdest = ((struct sockaddr_in*)dest)->sin_addr.s_addr;
        if ((saddr & mask) == (sdest & mask)) {
            score = 5;
            break;
        }
        mask = 255;
        for (i = 0; i < 3; i++) {
            if ((saddr & mask) == (sdest & mask)) {
                score++;
            }
            mask = ((mask << 8) | 255);
        }
        if (!is_lan_addr(saddr) && !is_lan_addr(sdest)) {
            score += 2;
        }
    } break;
    }

    return score;
}

static const char* route = "/proc/net/route";
/*
 * Return the source address for the given destination address
 */
const char*
findsaddr(const struct sockaddr_in* to)
{
    int i, n;
    FILE* f;
    u_int32_t mask, gw = 0xFFFFFFFF;
    u_int32_t dest, tmask, tgw;
    char buf[256], tdevice[256];
    static char device[32];

    if ((f = fopen(route, "r")) == NULL) {
        DLOG("open %s: %m\n", route);
        return 0;
    }

    /* Find the appropriate interface */
    n = 0;
    mask = 0;
    device[0] = '\0';
    while (fgets(buf, sizeof(buf), f) != NULL) {
        int flags, ref, use, metric;
        ++n;
        if (n == 1 && strncmp(buf, "Iface", 5) == 0)
            continue;
        if ((i = sscanf(buf, "%s %x %x %d %d %d %d %x",
                 tdevice, &dest, &tgw, &flags, &ref, &use, &metric, &tmask))
            != 8) {
            //DLOG("junk in buffer");
            //return 0;
            continue;
        }
        if ((to->sin_addr.s_addr & tmask) == dest && (tmask > mask || mask == 0) && (tgw < gw)) {
            mask = tmask;
            strcpy(device, tdevice);
            gw = tgw;
        }
    }
    fclose(f);

    if (device[0] == '\0') {
        DLOG("no matching route!");
        return 0;
    }

    return device;
}

struct ifi_info* ifi_select(int sock, struct sockaddr* dest)
{
    struct ifi_info *ifi_head, *ifi_ret = 0;

    if (!(ifi_head = get_ifi_info(sock, IFI_NETMASK | IFI_LOOPBACKS))) {
        return 0;
    }

    if (dest->sa_family == AF_INET) {
        const char* dev;

        if ((dev = findsaddr((struct sockaddr_in*)dest))) {
            struct ifi_info* ifi;

            for (ifi = ifi_head; ifi; ifi = ifi->ifi_next) {
                if (!strcmp(ifi->ifi_name, dev)) {
                    if (!(ifi_ret = malloc(sizeof(struct ifi_info)))) {
                        break;
                    }
                    bzero(ifi_ret, sizeof(struct ifi_info));
                    strcpy(ifi_ret->ifi_name, ifi->ifi_name);
                    ifi_ret->ifi_addr = malloc(sizeof(struct sockaddr_in));
                    sockcpy(ifi_ret->ifi_addr, ifi->ifi_addr);
                    break;
                }
            }
        }
    }

    free_ifi_info(ifi_head);
    return ifi_ret;
}

void print_ifi_info(struct ifi_info* ifi)
{
    struct sockaddr* sa;

    printf("%s: < ", ifi->ifi_name);
    if (ifi->ifi_flags & IFF_UP)
        printf("UP ");
    if (ifi->ifi_flags & IFF_BROADCAST)
        printf("BCAST ");
    if (ifi->ifi_flags & IFF_MULTICAST)
        printf("MCAST ");
    if (ifi->ifi_flags & IFF_LOOPBACK)
        printf("LOOP ");
    if (ifi->ifi_flags & IFF_POINTOPOINT)
        printf("P2P ");
    printf(">\n");

    if ((sa = ifi->ifi_addr) != NULL)
        printf("	IP addr: %s\n", sock_ntop_host(sa, sizeof(*sa)));
    if ((sa = ifi->ifi_brdaddr) != NULL)
        printf("	Broadcast addr: %s\n", sock_ntop_host(sa, sizeof(*sa)));
    if ((sa = ifi->ifi_dstaddr) != NULL)
        printf("	Destination addr: %s\n", sock_ntop_host(sa, sizeof(*sa)));
    if ((sa = ifi->ifi_netmask) != NULL)
        printf("	Subnet mask: %s\n", sock_ntop_host(sa, sizeof(*sa)));
}

/*this echo_gate_addr was rewritten by Sam. it is more stable than before, and
 * allows user to specify ECHOSVR_ADDR*/
struct sockaddr* echo_gateway_addr(struct sockaddr* addr, socklen_t addrlen)
{
    static const char* req = "ECHOGWADDRRQ";
    static struct sockaddr_in gwaddr = { 0, 0, { 0 } }; /*we use gwaddr as return value this makes types of gwaddr has to be 
                                                      static*/
    static struct timeval last = { 0, 0 };

    struct sockaddr_in svraddr, myaddr;
    int sock,
    try
        = 8; /*increase times of retry to 8*/

    char* ls_echo_addr = NULL; /* echo server addr added by Kevin 2002-12-6 */
    int needle = 58; /*":"*/
    char svraddr_str[30];
    socklen_t addr_len;

    if (is_valid_addr((struct sockaddr*)&gwaddr, sizeof(gwaddr))) {
        struct timeval current;

        gettimeofday(&current, 0);
        if (current.tv_sec - last.tv_sec > 60) {
            bzero(&gwaddr, sizeof(gwaddr));
        } else {
            return (struct sockaddr*)&gwaddr;
        }
    }

    if (addrlen < sizeof(struct sockaddr_in)) {
        LOG_FUNCTION_FAILED;
        return 0;
    }

    if (!(ls_echo_addr = getenv("ECHOSVR_ADDR")) || !strlen(ls_echo_addr)) {
        //       DLOG("echo server address is not set, try GK address instead.");

        if (!(ls_echo_addr = getenv("GKADDR")) || !strlen(ls_echo_addr)) {
            //            DLOG("echo server address is not set, GK address is not set either, try 202.96.136.145 instead.");
            ls_echo_addr = "202.96.136.145";
        }
    }

    /*this makes ls_echo_addr no longer point to a string in sys env. we do
     * this because a string in sys env cannot be modified.*/
    memcpy(svraddr_str, ls_echo_addr, strlen(ls_echo_addr) + 1);
    ls_echo_addr = svraddr_str;

    if (!strchr(ls_echo_addr, needle)) {
        //	    DLOG("echo server address does not contain port parameter, try 54210 instead.");
        strcat(ls_echo_addr, ":54210");
    }

    addr_len = sizeof(svraddr);
    if (sock_aton(ls_echo_addr, (struct sockaddr*)&svraddr, &addr_len) < 0) {
        DLOG("echo server address is not correct.");
        return 0;
    }

    DLOG("Echo addr: %s\n", ls_echo_addr);

    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(0);

    if ((sock = udp_bind(-1, (struct sockaddr*)&myaddr, sizeof(myaddr))) < 0) {
        LOG_SYS_ERROR;
        return 0;
    }

    while (try --) {
        struct pollfd pfd;
        int ret;

        if (sendto(sock, req, strlen(req), 0, (struct sockaddr*)&svraddr, sizeof(svraddr)) < 0) {
            LOG_SYS_ERROR;
            break;
        }

        pfd.fd = sock;
        pfd.events = POLLIN;
        if ((ret = poll(&pfd, 1, 250)) < 0) {
            LOG_SYS_ERROR;
            break;
        }

        if (ret > 0) {
            if (pfd.revents & POLLIN) {
                unsigned char recvbuf[20];
                socklen_t addrlen = sizeof(myaddr);

                if (recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&myaddr, &addrlen) < 0) {
                    LOG_SYS_ERROR;
                    break;
                }

                if (!strncmp(recvbuf, "ECHOGWADDRRP", 12)) {
                    gwaddr.sin_family = AF_INET;
                    memcpy(&gwaddr.sin_addr, recvbuf + 12, 4);
                    memcpy(&gwaddr.sin_port, recvbuf + 16, 2);

                    close(sock);
                    return (struct sockaddr*)&gwaddr;
                }
            }
        } else {
            LOG_FUNCTION_FAILED;
            continue;
        }
    }

    close(sock);
    return 0;
}

/*------------------------------------------------------------------------
 * commented by Kevin 2002-11-20
 * modified by Kevin 2002-12-6
 */

/* new detect_gateway_addr
 * added by Kevin 2002-11-20
 * removed by Kevin 2002-12-6
 *
 * enable again and rename to trace_gateway_addr() by James 2003-3-21
 * origin: struct sockaddr *detect_gateway_addr(struct sockaddr *addr, socklen_t addrlen)
 */
struct sockaddr* trace_gateway_addr(struct sockaddr* addr, socklen_t addrlen)
{
    static struct sockaddr_in gst_gate_sockaddr_in;
    char* ls_gate_metric;
    char* ls_gate_timeout;
    int li_gate_metric,
        li_ret;
    long ll_gate_timeout;
    struct sockaddr_in* lpst_dest_sockaddr_in;
    struct in_addr lst_dest_addr,
        lst_gate_addr;

    if (addr->sa_family != AF_INET) {
        LOG_FUNCTION_FAILED;
        return NULL;
    }

    memset(&gst_gate_sockaddr_in, 0, sizeof(struct sockaddr_in));

    lpst_dest_sockaddr_in = (struct sockaddr_in*)addr;
    lst_dest_addr = lpst_dest_sockaddr_in->sin_addr;

    ls_gate_metric = getenv("GATE_METRIC");

    if (ls_gate_metric == NULL)
        li_gate_metric = 0;
    else
        li_gate_metric = atoi(ls_gate_metric);

    ls_gate_timeout = getenv("GATE_TIMEOUT");

    if (ls_gate_timeout == NULL)
        ll_gate_timeout = 3000;
    else
        ll_gate_timeout = atol(ls_gate_timeout);

    li_ret = get_gate_addr(lst_dest_addr,
        li_gate_metric + 1,
        ll_gate_timeout,
        &lst_gate_addr);
    if (li_ret != 0) {
        DLOG("failed: %d", li_ret);
        return NULL;
    }

    gst_gate_sockaddr_in.sin_family = AF_INET;
    gst_gate_sockaddr_in.sin_addr = lst_gate_addr;

    DLOG("%s", sock_ntop_host((struct sockaddr*)&gst_gate_sockaddr_in, sizeof(gst_gate_sockaddr_in)));

    return (struct sockaddr*)&gst_gate_sockaddr_in;
}

struct sockaddr* detect_gateway_addr(struct sockaddr* addr, socklen_t addrlen)
{
    struct sockaddr* gwaddr;

    if (!(gwaddr = echo_gateway_addr(addr, addrlen)) && !(gwaddr = trace_gateway_addr(addr, addrlen))) {
        return 0;
    }

    return gwaddr;
}

int getifaddr(const char* devnam, struct sockaddr* addr, socklen_t* addrlen)
{
    struct ifreq ifr;
    int disc_sock, retval;

    disc_sock = socket(PF_PACKET, SOCK_DGRAM, 0);
    if (disc_sock < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    strncpy(ifr.ifr_name, devnam, sizeof(ifr.ifr_name));

    retval = ioctl(disc_sock, SIOCGIFADDR, &ifr);
    if (retval < 0) {
        DLOG("Bad device name: %s (%m)", devnam);
        return 0;
    }

    if ((retval = sockncpy(addr, *addrlen, &ifr.ifr_addr, socklen(&ifr.ifr_addr))) < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    *addrlen = retval;

    return 0;
}

int getsockifaddr(int sock, struct sockaddr* addr, socklen_t* addrlen)
{
    struct sockaddr_storage name;
    socklen_t namelen = sizeof(name);
    char* env;

    if (getsockname(sock, (struct sockaddr*)&name, &namelen) < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    switch (name.ss_family) {
    case AF_INET:
        if (((struct sockaddr_in*)&name)->sin_addr.s_addr != INADDR_ANY) {
            if (sockncpy(addr, *addrlen, (struct sockaddr*)&name, sizeof(struct sockaddr_in)) < 0) {
                LOG_FUNCTION_FAILED;
                return -1;
            }

            return 0;
        }
        break;
    default:
        return -1;
    }

    if ((env = getenv("USE_INTERFACE"))) {
        int port;

        if (getifaddr(env, addr, addrlen) < 0) {
            LOG_FUNCTION_FAILED;
            return -1;
        }

        if ((port = getsockport((struct sockaddr*)&name)) > 0) {
            DLOG("port=%d", port);
            setsockport(addr, port);
        } else {
            DLOG("cannot get sockport: %s", sock_ntop_host((struct sockaddr*)&name, socklen((struct sockaddr*)&name)));
        }
    } else {
        struct ifi_info* ifi;

        if (!is_valid_addr(addr, *addrlen)) {
            bzero(addr, *addrlen);
            addr->sa_family = name.ss_family;
        }

        if (!(ifi = ifi_select(sock, addr))) {
            LOG_FUNCTION_FAILED;
            return -1;
        }

        if (sockncpy(addr, *addrlen, ifi->ifi_addr, socklen(ifi->ifi_addr)) < 0) {
            free_ifi_info(ifi);
            LOG_FUNCTION_FAILED;
            return -1;
        }
        free_ifi_info(ifi);
    }

    return 0;
}

/* vim:ts=4:sw=4:tw=78:set et: */
