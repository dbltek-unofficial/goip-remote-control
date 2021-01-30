/* 这个文件是程序启动以及页面处理*/
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "app.h"
#include "debug.h"
#include "httpd.h"
#include "ip_list.h"
#include "radm.h"

struct radmsrv* g_radmsrv;
int radmsrv_ctl[2];
int httpport = 8086;
int srvport = 8085;
int errlog = 1;
char* ht_user = "admin";
char* ht_pass = "dbl#admin";
char* ht_realm = "Please Login";
char* cfgfile = "/root/radmsrv/radmsrv.cfg";
char* startsh = "/root/radmsrv/run_radmsrv";
char* cfgname[] = { "WEBPORT", "USERNAME", "PASSWORD", "CLIPORT", "CLIKEY", "PORTRANGE", "CFGFILE", "STARTSH", "WHITEIP" };
httpd* server;

/*输出页面到客户的浏览器*/
static void show_radmclient_list(httpd* server)
{
    struct radmsrv* radmsrv = g_radmsrv;
    struct radmcli* cli;
    char host[256], buf[256];
    int res;
    httpVar* var;
    fprintf(stderr, "show list\n");
    if ((var = httpdGetVariableByName(server, "findcli"))) {
        if ((cli = radmsrv_find_client(radmsrv, var->value, 1)) == NULL) {
            httpdPrintf(server, "Remote Administration Server cannot find id:%s\n", var->value);
            return;
        }

        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int port;

        memset(&addr, 0, sizeof(addr));
        getsockname(cli->l_sock, (struct sockaddr*)&addr, &addrlen);
        port = ntohs(addr.sin_port);
        if (addr.sin_addr.s_addr == htonl(INADDR_ANY)) {
            if (*server->request.host == '\0') {
                struct sockaddr_in sa;
                socklen_t salen = sizeof(sa);

                getsockname(server->clientSock, (struct sockaddr*)&sa, &salen);
                sprintf(host, "%s", inet_ntoa(sa.sin_addr));
            } else {
                char* p;
                strcpy(host, server->request.host);
                if ((p = strchr(host, ':'))) {
                    *p++ = '\0';
                }
            }
        } else {
            strcpy(host, inet_ntoa(addr.sin_addr));
        }
        char refer[256];
        memset(refer, 0, 256);
        //strcpy(refer, "http://www.baidu.com");
        sprintf(refer, "http://%s:%d", host, port);
        httpdPrintf(server, "<HTML>\n<HEAD>\n");
        httpdPrintf(server, "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0; URL=%s\">\n", refer);
        httpdPrintf(server, "</HEAD>\n");
        httpdPrintf(server, "</HTML>\n");
        return;
    }
    //httpdForceAuthenticate(server, ht_realm);
    if (!httpdAuthenticate(server, ht_realm) || (strcmp(server->request.authUser, ht_user) != 0) || (strcmp(server->request.authPassword, ht_pass) != 0)) {
        httpdForceAuthenticate(server, ht_realm);
        return;
    }
    res = sprintf(buf, "IP%u\n", inet_addr(server->clientAddr));
    if (write(radmsrv_ctl[1], buf, res) < 0) {
        fprintf(stderr, "write to control pipe error: %m\n");
        return;
    }
    //add_ip(inet_addr(server->clientAddr));
    //get_ip_data(inet_addr(server->clientAddr));
    if ((var = httpdGetVariableByName(server, "cfg"))) {
        FILE* fp;
        char buf[1024];
        char value[7][50];
        char tmpname[1024], tmpvalue[1024];
        int i;
        int rebootflag = 0;
        int tmpport = 0;
        if (!strcmp(var->value, "write")) {
            var = httpdGetVariableByName(server, "Submit");
            if (!strcmp(var->value, "Save") || !strcmp(var->value, "Save Reboot")) {
                if (!strcmp(var->value, "Save Reboot"))
                    rebootflag = 1;
                var = httpdGetVariableByName(server, cfgname[0]);
                tmpport = atoi(var->value);
                if (httpport != tmpport) {
                    struct sockaddr_in tmpaddr;
                    tmpaddr.sin_family = AF_INET;
                    tmpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
                    tmpaddr.sin_port = htons(tmpport);
                    int tmpsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (tmpsock < 0) {
                        fprintf(stderr, "cannot create socket: %m\n");
                        MYDLOG("%m");
                        return;
                    }

                    if (bind(tmpsock, (struct sockaddr*)&tmpaddr, sizeof(struct sockaddr_in)) < 0 || tmpport == 0) { //
                        httpdPrintf(server, "<CENTER> Cannot use port %s\n", var->value);
                        httpdPrintf(server, "<a href=\"http://%s\">BACK</a>", server->request.host);
                        close(tmpsock);
                        return;
                    }
                    close(tmpsock);
                }
                var = httpdGetVariableByName(server, cfgname[3]);
                if (srvport != atoi(var->value)) {
                    struct sockaddr_in tmpaddr;
                    tmpaddr.sin_family = AF_INET;
                    tmpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
                    tmpaddr.sin_port = htons(atoi(var->value));
                    int tmpsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (tmpsock < 0) {
                        fprintf(stderr, "cannot create socket: %m\n");
                        MYDLOG("cannot create socket: %m");
                        return;
                    }
                    //tmpsock;
                    if (bind(tmpsock, (struct sockaddr*)&tmpaddr, sizeof(struct sockaddr_in)) < 0 || tmpport == atoi(var->value) || 0 == atoi(var->value)) { //
                        httpdPrintf(server, "<CENTER> Cannot use port %s\n", var->value);
                        httpdPrintf(server, "<a href=\"http://%s\">BACK</a>", server->request.host);
                        close(tmpsock);
                        return;
                    }
                    close(tmpsock);
                }
                var = httpdGetVariableByName(server, cfgname[8]);
                if (var) {
                    res = sprintf(buf, "W%s\n", var->value);
                    if (write(radmsrv_ctl[1], buf, res) < 0) {
                        fprintf(stderr, "write to control pipe error: %m\n");
                        return;
                    }
                }
                if (!(fp = fopen(cfgfile, "w"))) {
                    httpdPrintf(server, "<CENTER> Cannot open file for write\n");
                    httpdPrintf(server, "<a href=\"http://%s\">BACK</a>", server->request.host);
                    return;
                }
                bzero(buf, 1024);
                for (i = 0; i < 9; i++)
                    if ((var = httpdGetVariableByName(server, cfgname[i])))
                        fprintf(fp, "%s='%s'\n", cfgname[i], var->value);
                fclose(fp);
                if (rebootflag)
                    system(startsh);
                httpdPrintf(server, "<CENTER> Has been saved%s!\n", rebootflag ? " and sent to restart command" : "");
                httpdPrintf(server, "<a href=\"http://%s\">BACK</a>", server->request.host);
            }
            //if($_POST['Submit'] == "Save" || $_POST['Submit'] == "Save Reboot")
        } else { //读配制
            if ((fp = fopen(cfgfile, "r")) <= 0) {
                httpdPrintf(server, "<CENTER> Cannot open file!%m\n");
                httpdPrintf(server, "<a href=\"http://%s\">BACK</a>", server->request.host);
                return;
            }

            while (!feof(fp)) {
                bzero(buf, 1024);
                bzero(tmpname, 1024);
                bzero(tmpvalue, 1024);
                if (fgets(buf, 1024, fp) < 0) {
                    fprintf(stderr, "fget error\n");
                    httpdPrintf(server, "<CENTER> read file error\n");
                    httpdPrintf(server, "<a href=\"http://%s\">返回</a>", server->request.host);
                    fclose(fp);
                    return;
                }
                //printf("%s\n", buf);
                sscanf(buf, "%[^=]='%[^']'", tmpname, tmpvalue);
                for (i = 0; i < 9; i++) {
                    if (!bcmp(tmpname, cfgname[i], 6)) {
                        //	printf("get %d %s=\"%s\"\n", i,tmpname,tmpvalue);
                        strncpy(value[i], tmpvalue, 50);
                        break;
                    }
                }

            } //end of read file
            fclose(fp);
            httpdPrintf(server, "<html><title>Remote Administration Server</title><body>\n");
            //httpdPrintf(server, "<table border=\"0\">\n");
            //httpdPrintf(server, "<tr><a href=\"http://%s/?cfg=read\">参数设置</a></tr>",server->request.host);
            httpdPrintf(server, "<form method=\"post\" action=\"?cfg=write\" name=\"form1\" onSubmit=\"javascript:return check();\">\n");
            httpdPrintf(server, "  <br>\n");
            httpdPrintf(server, "  <br>\n");
            httpdPrintf(server, "  <table wIdth=\"400\" border=\"0\" align=\"center\" cellpadding=\"2\" cellspacing=\"1\" class=\"border\" >\n");
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td height=\"22\" colspan=\"2\"> <div align=\"center\"><strong>Remote server's parameters</strong></div></td>\n");
            httpdPrintf(server, "    </tr>\n");
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td wIdth=\"200\" align=\"right\"><strong>Web Port:</strong></td>\n");
            httpdPrintf(server, "      <td><input type=\"input\" name=\"%s\" value=\"%s\"></td>\n", cfgname[0], value[0]);
            httpdPrintf(server, "    </tr>\n");

            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td wIdth=\"200\" align=\"right\"><strong>Web UserName:</strong></td>\n");
            httpdPrintf(server, "      <td><input type=\"input\" name=\"%s\" value=\"%s\"> </td>\n", cfgname[1], value[1]);
            httpdPrintf(server, "    </tr>\n");
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td wIdth=\"200\" align=\"right\"><strong>Web Password:</strong></td>\n");
            httpdPrintf(server, "      <td><input type=\"input\" name=\"%s\" value=\"%s\"> </td>\n", cfgname[2], value[2]);
            httpdPrintf(server, "    </tr>\n");
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td wIdth=\"200\" align=\"right\"><strong>Listen Port:</strong></td>\n");
            httpdPrintf(server, "      <td><input type=\"input\" name=\"%s\" value=\"%s\"> </td>\n", cfgname[3], value[3]);
            httpdPrintf(server, "    </tr>  \n");
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td wIdth=\"200\" align=\"right\"><strong>Server Key:</strong></td>\n");
            httpdPrintf(server, "      <td><input type=\"input\" name=\"%s\" value=\"%s\"> </td>\n", cfgname[4], value[4]);
            httpdPrintf(server, "    </tr>  \n");
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td wIdth=\"200\" align=\"right\"><strong>Port Range:</strong></td>\n");
            httpdPrintf(server, "      <td><input type=\"input\" name=\"%s\" value=\"%s\"> </td>\n", cfgname[5], value[5]);
            httpdPrintf(server, "    </tr>  \n");
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td wIdth=\"200\" align=\"right\"><strong>White List IP:</strong></td>\n");
            httpdPrintf(server, "      <td><input type=\"input\" name=\"%s\" value=\"%s\"> </td>\n", cfgname[8], value[8]);
            httpdPrintf(server, "    </tr>  \n");
#ifdef FILE
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td wIdth=\"200\" align=\"right\"><strong>Config File:</strong></td>\n");
            httpdPrintf(server, "      <td><input type=\"input\" name=\"%s\" value=\"%s\"> </td>\n", cfgname[6], value[6]);
            httpdPrintf(server, "    </tr>  \n");
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td wIdth=\"150\" align=\"right\"><strong>Start Shell:</strong></td>\n");
            httpdPrintf(server, "      <td><input type=\"input\" name=\"%s\" value=\"%s\"> </td>\n", cfgname[7], value[7]);
            httpdPrintf(server, "    </tr>  \n");
#endif

            httpdPrintf(server, "           \n");
            httpdPrintf(server, "    <tr> \n");
            httpdPrintf(server, "      <td height=\"40\" colspan=\"2\" align=\"center\"><input name=\"Action\" type=\"hIdden\" Id=\"Action\" value=\"Modify\"> \n");
            httpdPrintf(server, "        <input  type=\"submit\" name=\"Submit\" value=\"Save\" style=\"cursor:hand;\"> \n");
            httpdPrintf(server, "        &nbsp; <input type=\"submit\" name=\"Submit\" value=\"Save Reboot\" style=\"cursor:hand;\"> \n");
            httpdPrintf(server, "        &nbsp;<input name=\"Cancel\" type=\"button\" Id=\"Cancel\" value=\"Cancel\" onClick=\"window.location.href='http://%s'\" style=\"cursor:hand;\"></td>\n", server->request.host);
            httpdPrintf(server, "    </tr>\n");
            httpdPrintf(server, "  </table>\n");
            httpdPrintf(server, "  </form>\n");
            httpdPrintf(server, "</body> </html>");
        }
        return;
    }
    if ((var = httpdGetVariableByName(server, "del"))) {
        httpVar* var_del;
        char *id = var->value, *refer;
        char buf[1024];
        int res, type = 1;

        if ((var_del = httpdGetVariableByName(server, "type"))) {
            type = atoi(var_del->value);
        }
        fprintf(stderr, "del type:%d client: %s\n", type, id);
        res = sprintf(buf, "D%d%s\n", type, id);
        if (write(radmsrv_ctl[1], buf, res) < 0) {
            fprintf(stderr, "write to control pipe error: %m\n");
            return;
        }

        httpdSetResponse(server, "303 See Other\n");

        if ((var = httpdGetVariableByName(server, "refer"))) {
            refer = var->value;
        } else {
            //refer=server->request.host;
            refer = "/";
        }
        fprintf(stderr, "refer: %s\n", refer);

        snprintf(buf, sizeof(buf), "Location: %s", refer);
        httpdAddHeader(server, buf);

        httpdPrintf(server, "<HTML>\n<HEAD>\n");
        httpdPrintf(server, "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0; URL=%s\">\n", refer);
        httpdPrintf(server, "</HEAD>\n");
        httpdPrintf(server, "<BODY><a href=\"%s\">Click here</a></BODY>\n", refer);
        httpdPrintf(server, "</HTML>\n");

        return;
    }
    httpdPrintf(server, "<html><title>Remote Administration Server</title><body><CENTER> \n");
    httpdPrintf(server, "<tr><a href=\"http://%s/?cfg=read\">v1.06 Modify Options</a></tr>", server->request.host);
    httpdPrintf(server, "<table border=\"0\">\n");
    cli = radmsrv->clients;
    var = httpdGetVariableByName(server, "telnet");
    while (cli) {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int port;

        if (var && cli->is_radmin != TELNET) {
            cli = cli->next;
            continue;
        } else if (!var && cli->is_radmin == TELNET) {
            cli = cli->next;
            continue;
        }

        memset(&addr, 0, sizeof(addr));
        getsockname(cli->l_sock, (struct sockaddr*)&addr, &addrlen);
        port = ntohs(addr.sin_port);
        if (addr.sin_addr.s_addr == htonl(INADDR_ANY)) {
            if (*server->request.host == '\0') {
                struct sockaddr_in sa;
                socklen_t salen = sizeof(sa);

                getsockname(server->clientSock, (struct sockaddr*)&sa, &salen);
                sprintf(host, "%s", inet_ntoa(sa.sin_addr));
            } else {
                char* p;
                strcpy(host, server->request.host);
                if ((p = strchr(host, ':'))) {
                    *p++ = '\0';
                }
            }
        } else {
            strcpy(host, inet_ntoa(addr.sin_addr));
        }
        httpdPrintf(server, "<tr>\n");
        httpdPrintf(server, "\t<td>%s </td>\n", cli->id);
        httpdPrintf(server, "\t<td>%s </td>\n", inet_ntoa(cli->addr.sin_addr));

        if (cli->is_radmin == TELNET) {
            httpdPrintf(server, "\t<td><a href=\"telnet://%s:%d\">TELNET</a></td>\n", host, port);
            httpdPrintf(server, "\t<td><a href=\"http://%s/?del=%s&type=3&refer=%%3Ftelnet\">Del</a></td>\n", server->request.host, cli->id);
        } else if (cli->is_radmin == RADMIN) {
            httpdPrintf(server, "\t<td><a href=\"http://%s:%d\">RADMIN</a></td>\n", host, port);
            httpdPrintf(server, "\t<td><a href=\"http://%s/?del=%s&type=1\">Del</a></td>\n", server->request.host, cli->id);
        } else if (cli->is_radmin == RLOGIN) {
            httpdPrintf(server, "\t<td><a href=\"telnet://%s:%d\">TELNET</a></td>\n", host, port);
            httpdPrintf(server, "\t<td><a href=\"http://%s/?del=%s&type=2\">Del</a></td>\n", server->request.host, cli->id);
        }
        //		httpdPrintf(server, "\t<td>%s</td>\n", inet_ntoa(cli->addr.sin_addr));
        httpdPrintf(server, "</tr>\n");

        cli = cli->next;
    }
    httpdPrintf(server, "</table>\n");
    httpdPrintf(server, "</body></html>\n");
}

/*这是设备的xml列表，目前没有客户使用*/
static void show_radmclient_list_xml(httpd* server)
{
    struct radmsrv* radmsrv = g_radmsrv;
    struct radmcli* cli;
    char host[256];
    httpVar* var;

    if (!httpdAuthenticate(server, ht_realm) || (strcmp(server->request.authUser, ht_user) != 0) || (strcmp(server->request.authPassword, ht_pass) != 0)) {
        httpdForceAuthenticate(server, ht_realm);
        return;
    }

    if ((var = httpdGetVariableByName(server, "del"))) {
        httpVar* var_del;
        char *id = var->value, *refer;
        char buf[1024];
        int res, type = 1;

        if ((var_del = httpdGetVariableByName(server, "type"))) {
            type = atoi(var_del->value);
        }
        fprintf(stderr, "del type:%d client: %s\n", type, id);
        res = sprintf(buf, "D%d%s\n", type, id);
        if (write(radmsrv_ctl[1], buf, res) < 0) {
            fprintf(stderr, "write to control pipe error: %m\n");
            return;
        }

        httpdSetResponse(server, "303 See Other\n");

        if ((var = httpdGetVariableByName(server, "refer"))) {
            refer = var->value;
        } else {
            refer = "/clients.xml";
        }
        fprintf(stderr, "refer: %s\n", refer);

        snprintf(buf, sizeof(buf), "Location: %s", refer);
        httpdAddHeader(server, buf);

        httpdPrintf(server, "<HTML>\n<HEAD>\n");
        httpdPrintf(server, "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0; URL=%s\">\n", refer);
        httpdPrintf(server, "</HEAD>\n");
        httpdPrintf(server, "<BODY><a href=\"%s\">Click here</a></BODY>\n", refer);
        httpdPrintf(server, "</HTML>\n");

        return;
    }

    httpdSetContentType(server, "text/xml");
    httpdPrintf(server, "<?xml version=\"1.0\" ?>\n");
    httpdPrintf(server, "<clients>\n");
    cli = radmsrv->clients;
    while (cli) {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int port;

        memset(&addr, 0, sizeof(addr));
        getsockname(cli->l_sock, (struct sockaddr*)&addr, &addrlen);
        port = ntohs(addr.sin_port);
        if (addr.sin_addr.s_addr == htonl(INADDR_ANY)) {
            if (*server->request.host == '\0') {
                struct sockaddr_in sa;
                socklen_t salen = sizeof(sa);

                getsockname(server->clientSock, (struct sockaddr*)&sa, &salen);
                sprintf(host, "%s", inet_ntoa(sa.sin_addr));
            } else {
                char* p;
                strcpy(host, server->request.host);
                if ((p = strchr(host, ':'))) {
                    *p++ = '\0';
                }
            }
        } else {
            strcpy(host, inet_ntoa(addr.sin_addr));
        }

        if (cli->is_radmin == RADMIN) {
            httpdPrintf(server, "\t<client id=\"%s\" type=\"web\">\n", cli->id);
            httpdPrintf(server, "\t\t<addr>http://%s:%d</addr>\n", host, port);
            httpdPrintf(server, "\t</client>");
        } else {
            httpdPrintf(server, "\t<client id=\"%s\" type=\"telnet\">\n", cli->id);
            httpdPrintf(server, "\t\t<addr>telnet://%s:%d</addr>\n", host, port);
            httpdPrintf(server, "\t</client>");
        }

        cli = cli->next;
    }
    httpdPrintf(server, "</clients>\n");
}

void httpd_accept_callback(httpd* server, unsigned events, int sock)
{
    struct timeval tv;
    int result;

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    if (fork() == 0) {
        result = httpdGetConnection(server, &tv);
        fprintf(stderr, "httpdGetConnection\n");
        if (result <= 0) {
            fprintf(stderr, "False Alarm ... \n");
            MYDLOG("False ALarm: %m");
            exit(1);
        }

        //if(fork()==0){
        fprintf(stderr, "httpdReadRequest b\n");
        if (httpdReadRequest(server) >= 0) {
            fprintf(stderr, "httpdReadRequest g\n");
            fprintf(stderr, "httpdProcessRequest: %s\n", server->request.path);
            httpdProcessRequest(server);
        }
        fprintf(stderr, "httpdReadRequest e\n");
        printf("httpdEndRequest\n");
        httpdEndRequest(server);
        fprintf(stderr, "httpdReadRequest end\n");
        exit(0);
    }
    //httpdEndRequest(server);
}

int create_http_server(char* host, int port)
{
    //httpd *server;

    if (!host || strcmp(host, "") == 0 || strcmp(host, "*") == 0) {
        host = NULL;
    }

    if (!(server = httpdCreate(host, port))) {
        fprintf(stderr, "cannot create http server: %s:%d\n", host ? host : "*", port);
        return -1;
    }

    httpdSetAccessLog(server, stderr);
    httpdSetErrorLog(server, stderr);

    httpdAddCContent(server, "/", "index.html", HTTP_TRUE, NULL, show_radmclient_list);
    httpdAddCContent(server, "/", "clients.xml", HTTP_FALSE, NULL, show_radmclient_list_xml);

    register_sock(server->serverSock, SOCKIN, server, httpd_accept_callback);

    return 0;
}

/*处理客户在http上的操作*/
void radmsrv_ctl_callback(struct radmsrv* srv, unsigned events, int fd)
{
    char buf[4096], *p;
    int res;

    fprintf(stderr, "radmsrv_ctl_callback():\n");
    res = read(fd, buf, sizeof(buf));
    if (res < 0) {
        fprintf(stderr, "pipe read error: %m\n");
        MYDLOG("pipe read error: %m");
        exit(-1);
    }

    p = buf;
    if (*p == 'D') { //删除设备
        printf("%s\n", p);
        char *id, type;
        ++p;
        type = *p;
        id = ++p;
        p = buf + res;
        *p-- = '\0';
        while (isspace(*p))
            *p-- = '\0';
        fprintf(stderr, "del type:%c client: %s\n", type, id);
        if (type == '1')
            radmsrv_del(srv, id, 1);
        else if (type == '3')
            radmsrv_del(srv, id, 3);
    } else if (!strncmp(p, "IP", 2)) { //客户通过了登录，把他的IP加入白名单
        unsigned int ip;
        sscanf(p + 2, "%u", &ip);
        add_ip(ip);
    } else if (*p == 'W') { //客户进行了更新IP白名单的操作
        *(p + strlen(p) - 1) = 0;
        memset(white_list_ip, 0, sizeof(white_list_ip));
        strncpy(white_list_ip, p + 1, sizeof(white_list_ip));
        fprintf(stderr, "white_list_ip:%s\n", white_list_ip);
    }
}

int radmsrv_main(struct sockaddr_in addr, int httpport, char* key)
{
    struct radmsrv* radmsrv;
    //struct sockaddr_in addr;
    //char *key="dbl#admin";
    //char *host=NULL;
    //int httpport=8086, srvport=8085;
    //int i=1;

    signal(SIGCHLD, SIG_IGN);
    app_init(0, 0, NULL, "radmsrv");
    srand((unsigned)time(NULL));
    if (errlog)
        set_log_file_app("./err_radmsrv.log");
    //LOG_SYS_ERROR;
    MYDLOG("NEW PROCESS BEGIN\n");
    radmsrv = radmsrv_create(&addr, key); //启动TCP监,等待设备连接
    g_radmsrv = radmsrv;

    if (create_http_server(NULL, httpport) < 0) { //启动http服务
        fprintf(stderr, "cannot create http server\n");
        return -1;
    }
    //if(pipe(radmsrv_ctl)<0){
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, radmsrv_ctl) < 0) { //建立从http到后台的数据通信
        fprintf(stderr, "cannot create control pipe: %m\n");
        return -1;
    }

    register_sock(radmsrv_ctl[0], SOCKIN, radmsrv, radmsrv_ctl_callback);
    app_run();

    return 0;
}
