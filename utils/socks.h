#ifndef SOCKS5_H
#define SOCKS5_H

ssize_t readn(int ai_fd, void* ap_buf, size_t at_count);
ssize_t writen(int ai_fd, void* ap_buf, size_t at_count);

int socks_get_server_addr(struct sockaddr* addr, socklen_t* addrlen);
int socks_connect(int sock, struct sockaddr* addr, socklen_t addrlen);
int socks_listen(int sock, struct sockaddr* addr, socklen_t addrlen);
int socks_accept(int sock, struct sockaddr* from, socklen_t* fromlen);
int socks_associate(int sock, struct sockaddr* addr, socklen_t addrlen);
int socks_initiate(int sock, struct sockaddr* relay_addr, socklen_t addrlen);
int socks_keep_alive(int sock, struct sockaddr* relay_addr, socklen_t addrlen);
int socks_keep_alive_ack(int sock, struct sockaddr* relay_addr, socklen_t addrlen);

#endif
