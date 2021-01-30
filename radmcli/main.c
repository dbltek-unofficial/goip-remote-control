#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscfg.h>

#include "app.h"
#include "radmc.h"

int parse_addr(char* s, struct sockaddr_in* addr)
{
    char* p;
    struct hostent* hp;
    struct in_addr** pptr;

    if ((p = strchr(s, ':')) == NULL) {
        return -1;
    }

    *p++ = '\0';
    //p++;

    if ((hp = gethostbyname(s)) == NULL) {
        fprintf(stderr, "gethostbyname(%s):%m\n", s);
        return -1;
    }

    pptr = (struct in_addr**)hp->h_addr_list;
    if (*pptr == NULL) {
        fprintf(stderr, "can not get ip\n");
        return -1;
    }
    memcpy(&(addr->sin_addr), *pptr, sizeof(struct in_addr));
    /*
	if(!inet_aton(s, &addr->sin_addr)){
		return -1;
	}
	 */
    addr->sin_port = htons(atoi(p));
    addr->sin_family = AF_INET;

    return 0;
}

int parse_arg(char* s, struct sockaddr_in* addr)
{
    if (*s == '\0')
        return 0;

    if (strspn(s, "1234567890") == strlen(s)) {
        addr->sin_port = htons(atoi(s));
    } else if (strspn(s, "1234567890.") == strlen(s)) {
        int dot = 0;
        char* p = s;

        while (*p) {
            if (*p == '.') {
                if (dot == 3) {
                    *p++ = '\0';
                    break;
                }
                //*p='.';
                dot++;
            }
            p++;
        }
        if (!inet_aton(s, &addr->sin_addr)) {
            return -1;
        }
        addr->sin_family = AF_INET;
        if (*p) {
            addr->sin_port = htons(atoi(p));
        }
    } else {
        return -1;
    }

    return 0;
}

char* build_id(char* id)
{
    static char buf[256];
    char* p;

    p = buf;
    while (*id) {
        if (*id == '$' && *(id + 1) == '{') {
            char* var = p;
            id += 2;
            while (*id && *id != '}') {
                *p++ = *id++;
            }
            if (*id == '}')
                id++;
            *p++ = '\0';
            fprintf(stderr, "getsyscfg: %s\n", var);
            p = getsyscfg(var);
            if (p) {
                while (*p) {
                    *var++ = *p++;
                }
            }
            p = var;
        } else {
            *p++ = *id++;
        }
    }
    *p++ = '\0';

    fprintf(stderr, "build id=%s\n", buf);
    return buf;
}

int main(int argc, char* argv[])
{
    struct sockaddr_in raddr;
    struct sockaddr_in A_laddr; /*RADMIN*/
    struct sockaddr_in L_laddr; /*RLOGIN*/
    char *key, *id = NULL, *p;
    int i = 1, timeout = 0;

    memset(&raddr, 0, sizeof(raddr));
    memset(&A_laddr, 0, sizeof(A_laddr));
    memset(&L_laddr, 0, sizeof(L_laddr));

    /*	A_laddr.sin_family = L_laddr.sin_family = AF_INET;
	A_laddr.sin_addr.s_addr = L_laddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	A_laddr.sin_port=htons(80);
	L_laddr.sin_port=htons(23);
*/
    p = getsyscfg("RADMIN_KEY");
    if (!p || *p == '\0') {
        key = "dbl#admin";
    } else {
        key = strdup(p);
    }

    p = getsyscfg("RADMIN_ID");
    if (!p || *p == '\0') {
        p = getsyscfg("SN");
    } else {
        //p=getsyscfg(id);
        p = build_id(p);
    }
    if (p) {
        id = strdup(p);
    }

    p = getsyscfg("RADMIN_SERVER");
    if (p) {
        if (inet_aton(p, &raddr.sin_addr)) {
            raddr.sin_family = AF_INET;
        }
    }

    p = getsyscfg("RADMIN_PORT");
    if (p) {
        raddr.sin_port = htons(atoi(p));
    }

    fprintf(stderr, "radmcli: arg=%s\n", argv[1]);
    while (i < argc) {
        if (strcmp(argv[i], "-r") == 0) {
            if (parse_addr(argv[++i], &raddr) < 0) {
                fprintf(stderr, "invailid address: %s\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "-al") == 0) {
            if (parse_addr(argv[++i], &A_laddr)) {
                fprintf(stderr, "invailid address: %s\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "-ll") == 0) {
            if (parse_addr(argv[++i], &L_laddr)) {
                fprintf(stderr, "invailid address: %s\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "-k") == 0) {
            key = argv[++i];
        } else if (strcmp(argv[i], "-i") == 0) {
            id = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            timeout = atoi(argv[++i]);
        } else {
            //if(parse_arg(argv[i], &raddr)<0){
            fprintf(stderr, "cannot parse arg: %s\n", argv[i]);
            //	return -1;
            //}
        }

        i++;
    }

    if (!raddr.sin_family || (!A_laddr.sin_family && !L_laddr.sin_family) || !id) {
        fprintf(stderr, "usage: radmc -r <remote_ip>:<port> -al <RADMIN_local_ip>:<port> -ll <RLOGIN_local_ip>:<port> -k <encrypt_key> -i <id> -t <timeout>\n");
        return -1;
    }

    fprintf(stderr, "version:2.0\nremote addr: %s:%d\n", inet_ntoa(raddr.sin_addr), ntohs(raddr.sin_port));

    app_init(argc, argv, NULL, "radmc");

    if (!radmcs_create(&raddr, &A_laddr, &L_laddr, key, id, timeout)) {
        fprintf(stderr, "cannot create radmc\n");
        return -1;
    }

    /*
	if(A_laddr.sin_family){
		fprintf(stderr, "RADMIN local addr: %s:%d\n", inet_ntoa(A_laddr.sin_addr), ntohs(A_laddr.sin_port));
		if(!radmc_create(&raddr, &A_laddr, key, id, timeout, RADMIN)){
			fprintf(stderr, "cannot create radmc\n");
			return -1;
		}
	}
	
	if(L_laddr.sin_family){
		fprintf(stderr, "RLOGIN local addr: %s:%d\n", inet_ntoa(L_laddr.sin_addr), ntohs(L_laddr.sin_port));
        	if(!radmc_create(&raddr, &L_laddr, key, id, timeout, RLOGIN)){
                	fprintf(stderr, "cannot create radmc\n");
                	return -1;
        	}
	}
*/
    app_run();

    return 0;
}
