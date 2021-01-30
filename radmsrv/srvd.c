#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ip_list.h"

#define LOCKFILE "./daemon_radmsrv"
#define LOCKMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

extern int errlog;
extern char* ht_user;
extern char* ht_pass;
extern char* ht_realm;
extern char* cfgfile;
extern char* startsh;
extern int httpport;
extern int srvport;
extern unsigned short port_min;
extern unsigned short port_max;
extern unsigned short get_port;
extern int radmsrv_main(struct sockaddr_in addr, int httpport, char* key);

struct sockaddr_in addr;
char* key = "dbl#admin";

int lockfile(int fd)
{
    struct flock fl;

    fl.l_type = F_WRLCK;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;
    return (fcntl(fd, F_SETLK, &fl));
}

int already_running()
{
    int fd;
    char buf[16];

    fd = open(LOCKFILE, O_RDWR | O_CREAT, LOCKMODE);
    if (fd < 0) {
        fprintf(stderr, "cannot open %s: %s\n", LOCKFILE, strerror(errno));
        exit(1);
    }
    if (lockfile(fd) < 0) {
        if (errno == EACCES || errno == EAGAIN) {
            close(fd);
            fprintf(stderr, "lockfile error\n");
            exit(2);
        }
        fprintf(stderr, "cannot lock %s: %s\n", LOCKFILE, strerror(errno));
        exit(1);
    }
    fprintf(stderr, "lockfile ok\n");
    ftruncate(fd, 0);
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf) + 1);
    return 0;
}

void closeall(int fd)
{
    int fdlimit = sysconf(_SC_OPEN_MAX);

    while (fd < fdlimit)
        close(fd++);
}

int daemon(int nochdir, int noclose)
{
    switch (fork()) {
    case 0:
        break;
    case -1:
        return -1;
    default:
        _exit(0); /* 原进程退出 */
    }

    if (setsid() < 0) { /* 不应该失败 */
        perror("setsid");
        return -1;
    }
    /* 如果你希望将来获得一个控制tty,则排除(dyke)以下的switch语句 */
    /* -- 正常情况不建议用于守护程序 */

    switch (fork()) {
    case 0:
        break;
    case -1:
        return -1;
    default:
        _exit(0);
    }

    if (!nochdir)
        chdir("/");

    if (!noclose) {
        closeall(0);
        open("/dev/null", O_RDWR);
        dup(0);
        dup(0);
    }

    return 0;
}

void mysystem()
{
    switch (fork()) {
    case -1:
        fprintf(stderr, "fork failed");
        exit(-1);
    case 0:
        radmsrv_main(addr, httpport, key);
        exit(0);
    default:
        break;
    }
}

void child_exit(int signo)
{
    int status;

    /* 非阻塞地等待任何子进程结束 */
    if (waitpid(-1, &status, WNOHANG) < 0) {
        /*
              * 不建议在信号处理函数中调用标准输入/输出函数，
              * 但在一个类似这个的玩具程序里或许没问题
              */
        fprintf(stderr, "waitpid failed\n");
        return;
    }
    fprintf(stderr, "child exit\n");
    sleep(10);
    mysystem();
    fprintf(stderr, "mysys over\n");
}

int main(int argc, char* argv[])
{

    int i = 1;
    int noclose = 0;
    int nochdir = 1;
    //int srvport=8085;

    fprintf(stderr, "v1.06\n");
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    while (i < argc) {
        if (strcmp(argv[i], "-w") == 0) { //客户访问页面的端口，默认8086

            httpport = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-W") == 0) { //白名单的IP
            strncpy(white_list_ip, argv[++i], sizeof(white_list_ip));
        } else if (strcmp(argv[i], "-p") == 0) { //服务器监听设备的端口，默认1920
            srvport = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0) { //
            if (!inet_aton(argv[++i], &addr.sin_addr)) {
                fprintf(stderr, "invailid address: %s\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "-k") == 0) { //设备连接到 服务器的密码
            key = argv[++i];
        } else if (strcmp(argv[i], "-u") == 0) { //网页登录用户名，默认admin
            ht_user = argv[++i];
        } else if (strcmp(argv[i], "-P") == 0) { //网页登录密码, 默认admin
            ht_pass = argv[++i];
        } else if (strcmp(argv[i], "-R") == 0) { //网页要求密码时的提示信息
            ht_realm = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0) { //是否打印log
            noclose = 1;
        } else if (strcmp(argv[i], "-f") == 0) { //读取的配置文件
            cfgfile = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0) { //程序的启动脚本路径
            startsh = argv[++i];
        } else if (strcmp(argv[i], "-nl") == 0) { //是否关闭错误错误log 到err_radmsrv.log
            errlog = 0;
        } else if (strcmp(argv[i], "-r") == 0) { //开放的客户的连接的端口范围
            sscanf(argv[++i], "%d-%d", &port_min, &port_max);
            //fprintf(stderr, "client port:%d, %d\n", port_min, port_max);
            if (port_min <= 0 || port_max <= port_min)
                port_min = 0, port_max = 0;
            else {
                get_port = port_min;
            }
        } else {
            fprintf(stderr, "unknow option: %s\n", argv[i]);
            return -1;
        }

        i++;
    }

    addr.sin_port = htons(srvport);
    if (daemon(nochdir, noclose) < 0) {
        perror("daemon");
        exit(2);
    }

    //already_running();
    //chdir("../webd");

    //fprintf(stderr, "sdsdsd\n");

    struct sigaction act;

    /* 设定sig_chld函数作为我们SIGCHLD信号的处理函数 */
    act.sa_handler = child_exit;

    /* 在这个范例程序里，我们不想阻塞其它信号 */
    sigemptyset(&act.sa_mask);

    /*
          * 我们只关心被终止的子进程，而不是被中断
          * 的子进程 (比如用户在终端上按Control-Z)
          */
    act.sa_flags = SA_NOCLDSTOP;

    /*
          * 使这些设定的值生效. 如果我们是写一个真实的应用程序，
          * 我们也许应该保存这些原有值，而不是传递一个NULL。
          */
    if (sigaction(SIGCHLD, &act, NULL) < 0) {
        fprintf(stderr, "sigaction failed\n");
        return 1;
    }

    mysystem();
    while (1)
        pause();
    return 0;
}
