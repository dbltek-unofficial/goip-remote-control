/*
** Copyright (c) 2002  Hughes Technologies Pty Ltd.  All rights
** reserved.
**
** Terms under which this software may be used or copied are
** provided in the  specific license associated with this product.
**
** Hughes Technologies disclaims all warranties with regard to this
** software, including all implied warranties of merchantability and
** fitness, in no event shall Hughes Technologies be liable for any
** special, indirect or consequential damages or any damages whatsoever
** resulting from loss of use, data or profits, whether in an action of
** contract, negligence or other tortious action, arising out of or in
** connection with the use or performance of this software.
**
**
** $Id: protocol.c,v 1.1.1.1 2012/03/02 02:33:53 elwin Exp $
**
*/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#if defined(_WIN32)
#else
#include <sys/file.h>
#include <unistd.h>
#endif

#include "config.h"
#include "httpd.h"
#include "httpd_priv.h"

int _httpd_net_read(sock, buf, len) int sock;
char* buf;
int len;
{

    struct timeval tm;
    fd_set rset;
    int rlen = 0;
    int err;

    //printf("e1\n");
    tm.tv_sec = 3;
    tm.tv_usec = 0;
    FD_ZERO(&rset);
    FD_SET(sock, &rset);
    err = select(sock + 1, &rset, NULL, NULL, &tm);

    if (err < 0) {
        fprintf(stderr, "%s:%s:%d %s \n", __FILE__, __FUNCTION__, __LINE__, strerror(errno));
    }

    //printf("e2\n");
    if (err == 0) {

        //printf("e3\n");
        return 0;
    }

    //printf("e4\n");
    if (FD_ISSET(sock, &rset)) {

        //printf("e5\n");
#if defined(_WIN32)
        rlen = read(sock, buf, len, 0);
#else
        rlen = read(sock, buf, len);
#endif

        //printf("e6\n");
        if (rlen < 0) {

            //printf("e7\n");
            fprintf(stderr, "%s:%s:%d %s \n", __FILE__, __FUNCTION__, __LINE__, strerror(errno));
            return -1;
        }

        //printf("e8\n");
        return rlen;
    }

    //printf("e9\n");
    return rlen; // orginal read(sock, buf, len);

#if 0
#if defined(_WIN32) 
	return( recv(sock, buf, len, 0));
#else
	return( read(sock, buf, len));
#endif
#endif
}

int _httpd_net_write(sock, buf, len) int sock;
char* buf;
int len;
{
#if defined(_WIN32)
    return (send(sock, buf, len, 0));
#else
    return (write(sock, buf, len));
#endif
}

int _httpd_readChar(server, cp)
    httpd* server;
char* cp;
{

    //printf("d2\n");
    if (server->readBufRemain == 0) {

        //printf("d5\n");
        bzero(server->readBuf, HTTP_READ_BUF_LEN + 1);

        //printf("d6\n");
        server->readBufRemain = _httpd_net_read(server->clientSock,
            server->readBuf, HTTP_READ_BUF_LEN);

        //printf("d7\n");
        if (server->readBufRemain < 1) {

            //printf("d4\n");
            return (0);
        }
        server->readBuf[server->readBufRemain] = 0;
        server->readBufPtr = server->readBuf;
    }
    *cp = *server->readBufPtr++;
    server->readBufRemain--;

    //printf("d3\n");
    return (1);
}

int _httpd_readLine(server, destBuf, len)
    httpd* server;
char* destBuf;
int len;
{
    char curChar,
        *dst;
    int count;

    //printf("c1\n");
    count = 0;
    dst = destBuf;
    while (count < len) {

        //printf("d1\n");
        if (_httpd_readChar(server, &curChar) < 1) {

            //printf("c2\n");
            return (0);
        }
        if (curChar == '\n') {
            *dst = 0;

            //printf("c3\n");
            return (1);
        }
        if (curChar == '\r') {

            //printf("c3\n");
            continue;
        } else {
            *dst++ = curChar;
            count++;

            //printf("c4\n");
        }
    }

    //printf("c5\n");
    *dst = 0;
    return (1);
}

int _httpd_readBuf(server, destBuf, len)
    httpd* server;
char* destBuf;
int len;
{
    char curChar,
        *dst;
    int count;

    count = 0;
    dst = destBuf;
    while (count < len) {
        if (_httpd_readChar(server, &curChar) < 1)
            return (0);
        *dst++ = curChar;
        count++;
    }
    return (1);
}

int _httpd_sendExpandedText(server, buf, bufLen)
    httpd* server;
char* buf;
int bufLen;
{
    char *textStart,
        *textEnd,
        *varStart,
        *varEnd,
        varName[HTTP_MAX_VAR_NAME_LEN + 1];
    int length,
        offset;
    httpVar* var;

    length = offset = 0;
    textStart = buf;
    while (offset < bufLen) {
        /*
		** Look for the start of a variable name 
		*/
        textEnd = strchr(textStart, '$');
        if (!textEnd) {
            /* 
			** Nope.  Write the remainder and bail
			*/
            _httpd_net_write(server->clientSock,
                textStart, bufLen - offset);
            length += bufLen - offset;
            offset += bufLen - offset;
            break;
        }

        /*
		** Looks like there could be a variable.  Send the
		** preceeding text and check it out
		*/
        _httpd_net_write(server->clientSock, textStart,
            textEnd - textStart);
        length += textEnd - textStart;
        offset += textEnd - textStart;
        varEnd = strchr(textEnd, '}');
        if (*(textEnd + 1) != '{' || varEnd == NULL) {
            /*
			** Nope, false alarm.
			*/
            _httpd_net_write(server->clientSock, "$", 1);
            length += 1;
            offset += 1;
            textStart = textEnd + 1;
            continue;
        }

        /*
		** OK, looks like an embedded variable
		*/
        varStart = textEnd + 2;
        varEnd = varEnd - 1;
        if (varEnd - varStart > HTTP_MAX_VAR_NAME_LEN) {
            /*
			** Variable name is too long
			*/
            _httpd_net_write(server->clientSock, "$", 1);
            length += 1;
            offset += 1;
            textStart = textEnd + 1;
            continue;
        }

        /*
		** Is this a known variable?
		*/
        bzero(varName, HTTP_MAX_VAR_NAME_LEN);
        strncpy(varName, varStart, varEnd - varStart + 1);
        offset += strlen(varName) + 3;
        var = httpdGetVariableByName(server, varName);
        if (!var) {
            /*
			** Nope.  It's undefined.  Ignore it
			*/
            textStart = varEnd + 2;
            continue;
        }

        /*
		** Write the variables value and continue
		*/
        _httpd_net_write(server->clientSock, var->value,
            strlen(var->value));
        length += strlen(var->value);
        textStart = varEnd + 2;
    }
    return (length);
}

void _httpd_writeAccessLog(server)
    httpd* server;
{
    char dateBuf[30];
    struct tm* timePtr;
    time_t clock;
    int responseCode;

    if (server->accessLog == NULL)
        return;
    clock = time(NULL);
    timePtr = localtime(&clock);
    strftime(dateBuf, 30, "%d/%b/%Y:%T %Z", timePtr);
    responseCode = atoi(server->response.response);
    fprintf(server->accessLog, "%s - - [%s] %s \"%s\" %d %d\n",
        server->clientAddr, dateBuf, httpdRequestMethodName(server),
        httpdRequestPath(server), responseCode,
        server->response.responseLength);
}

void _httpd_writeErrorLog(server, level, message)
    httpd* server;
char *level,
    *message;
{
    char dateBuf[30];
    struct tm* timePtr;
    time_t clock;

    if (server->errorLog == NULL)
        return;
    clock = time(NULL);
    timePtr = localtime(&clock);
    strftime(dateBuf, 30, "%a %b %d %T %Y", timePtr);
    if (*server->clientAddr != 0) {
        fprintf(server->errorLog, "[%s] [%s] [client %s] %s\n",
            dateBuf, level, server->clientAddr, message);
    } else {
        fprintf(server->errorLog, "[%s] [%s] %s\n",
            dateBuf, level, message);
    }
}

#if 0

int _httpd_decode (bufcoded, bufplain, outbufsize)
	char *		bufcoded;
	char *		bufplain;
	int		outbufsize;
{
	static char six2pr[64] = {
    		'A','B','C','D','E','F','G','H','I','J','K','L','M',
    		'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    		'a','b','c','d','e','f','g','h','i','j','k','l','m',
    		'n','o','p','q','r','s','t','u','v','w','x','y','z',
    		'0','1','2','3','4','5','6','7','8','9','+','/'   
	};
  
	static unsigned char pr2six[256];

	/* single character decode */
#define DEC(c) pr2six[(int)c]
#define _DECODE_MAXVAL 63

	static int first = 1;

	int nbytesdecoded, j;
	register char *bufin = bufcoded;
	register unsigned char *bufout = bufplain;
	register int nprbytes;

	/*
	** If this is the first call, initialize the mapping table.
	** This code should work even on non-ASCII machines.
	*/
	if(first) 
	{
		first = 0;
		for(j=0; j<256; j++) pr2six[j] = _DECODE_MAXVAL+1;
		for(j=0; j<64; j++) pr2six[(int)six2pr[j]] = (unsigned char)j;
	}

   	/* Strip leading whitespace. */

   	while(*bufcoded==' ' || *bufcoded == '\t') bufcoded++;

	/*
	** Figure out how many characters are in the input buffer.
	** If this would decode into more bytes than would fit into
	** the output buffer, adjust the number of input bytes downwards.
	*/
	bufin = bufcoded;
	while(pr2six[(int)*(bufin++)] <= _DECODE_MAXVAL);
	nprbytes = bufin - bufcoded - 1;
	nbytesdecoded = ((nprbytes+3)/4) * 3;
	if(nbytesdecoded > outbufsize) 
	{
		nprbytes = (outbufsize*4)/3;
	}
	bufin = bufcoded;
   
	while (nprbytes > 0) 
	{
		*(bufout++)=(unsigned char)(DEC(*bufin)<<2|DEC(bufin[1])>>4);
		*(bufout++)=(unsigned char)(DEC(bufin[1])<<4|DEC(bufin[2])>>2);
		*(bufout++)=(unsigned char)(DEC(bufin[2])<<6|DEC(bufin[3]));
		bufin += 4;
		nprbytes -= 4;
	}
	if(nprbytes & 3)// original (nprbytes & 03) 
	{
		if(pr2six[(int)bufin[-2]] > _DECODE_MAXVAL) 
		{
			//nbytesdecoded -= 2;
			nbytesdecoded -= 3;
		}
		else 
		{
			nbytesdecoded -= 1;
		}
	}
	bufplain[nbytesdecoded] = 0;
	return(nbytesdecoded);
}

#endif

/* Base-64 decoding.  This represents binary data as printable ASCII
** characters.  Three 8-bit binary bytes are turned into four 6-bit
** values, like so:
**
**   [11111111]  [22222222]  [33333333]
**
**   [111111] [112222] [222233] [333333]
**
** Then the 6-bit values are represented using the characters "A-Za-z0-9+/".
*/

static int b64_decode_table[256] = {
    //  add by zsk
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 00-0F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 10-1F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, /* 20-2F */
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, /* 30-3F */
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, /* 40-4F */
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, /* 50-5F */
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 60-6F */
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, /* 70-7F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 80-8F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 90-9F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* A0-AF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* B0-BF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* C0-CF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* D0-DF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* E0-EF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 /* F0-FF */
};

/* Do base-64 decoding on a string.  Ignore any non-base64 bytes.
** Return the actual number of bytes generated.  The decoded size will
** be at most 3/4 the size of the encoded, and may be smaller if there
** are padding characters (blanks, newlines).
*/

//int _httpd_decode (bufcoded, bufplain, outbufsize)
//b64_decode( const char* str, unsigned char* space, int size )

int _httpd_decode(const char* str, unsigned char* space, int size) //  add by zsk
{
    const char* cp;
    int space_idx, phase;
    int d, prev_d = 0;
    unsigned char c;

    space_idx = 0;
    phase = 0;
    for (cp = str; *cp != '\0'; ++cp) {
        d = b64_decode_table[(int)*cp];
        if (d != -1) {
            switch (phase) {
            case 0:
                ++phase;
                break;
            case 1:
                c = ((prev_d << 2) | ((d & 0x30) >> 4));
                if (space_idx < size)
                    space[space_idx++] = c;
                ++phase;
                break;
            case 2:
                c = (((prev_d & 0xf) << 4) | ((d & 0x3c) >> 2));
                if (space_idx < size)
                    space[space_idx++] = c;
                ++phase;
                break;
            case 3:
                c = (((prev_d & 0x03) << 6) | d);
                if (space_idx < size)
                    space[space_idx++] = c;
                phase = 0;
                break;
            }
            prev_d = d;
        }
    }
    return space_idx;
}

char _httpd_from_hex(c) char c;
{
    return c >= '0' && c <= '9' ? c - '0'
                                : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                       : c - 'a' + 10; /* accept small letters just in case */
}

char* _httpd_unescape(str) char* str;
{
    char* p = str;
    char* q = str;
    static char blank[] = "";

    if (!str)
        return (blank);
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p)
                *q = _httpd_from_hex(*p++) * 16;
            if (*p)
                *q = (*q + _httpd_from_hex(*p++));
            q++;
        } else {
            if (*p == '+') {
                *q++ = ' ';
                p++;
            } else {
                *q++ = *p++;
            }
        }
    }

    *q++ = 0;
    return str;
}

void _httpd_freeVariables(var)
    httpVar* var;
{
    httpVar *curVar, *lastVar;

    if (var == NULL)
        return;
    _httpd_freeVariables(var->nextVariable);
    var->nextVariable = NULL;
    curVar = var;
    while (curVar) {
        lastVar = curVar;
        curVar = curVar->nextValue;
        free(lastVar->name);
        free(lastVar->value);
        free(lastVar);
    }
    return;
}

void _httpd_storeData(server, query)
    httpd* server;
char* query;
{
    char *cp,
        *cp2,
        var[50],
        *val,
        *tmpVal;

    if (!query)
        return;

    cp = query;
    cp2 = var;
    bzero(var, sizeof(var));
    val = NULL;
    while (*cp) {
        if (*cp == '=') {
            cp++;
            *cp2 = 0;
            val = cp;
            continue;
        }
        if (*cp == '&') {
            *cp = 0;
            tmpVal = _httpd_unescape(val);
            httpdAddVariable(server, var, tmpVal);
            cp++;
            cp2 = var;
            val = NULL;
            continue;
        }
        if (val) {
            cp++;
        } else {
            *cp2 = *cp++;
            if (*cp2 == '.') {
                strcpy(cp2, "_dot_");
                cp2 += 5;
            } else {
                cp2++;
            }
        }
    }
    *cp = 0;
    tmpVal = _httpd_unescape(val);
    httpdAddVariable(server, var, tmpVal);
}

void _httpd_formatTimeString(server, ptr, clock)
    httpd* server;
char* ptr;
int clock;
{
    struct tm tmbuf;

    if (clock == 0)
        clock = time(NULL);
    gmtime_r((time_t*)&clock, &tmbuf);
    strftime(ptr, HTTP_TIME_STRING_LEN, "%a, %d %b %Y %T GMT", &tmbuf);
}

void _httpd_sendHeaders(server, contentLength, modTime)
    httpd* server;
int contentLength,
    modTime;
{
    char tmpBuf[80],
        timeBuf[HTTP_TIME_STRING_LEN];

    if (server->response.headersSent)
        return;

    server->response.headersSent = 1;
    _httpd_net_write(server->clientSock, "HTTP/1.0 ", 9);
    _httpd_net_write(server->clientSock, server->response.response,
        strlen(server->response.response));
    _httpd_net_write(server->clientSock, server->response.headers,
        strlen(server->response.headers));

    _httpd_formatTimeString(server, timeBuf, 0);
    _httpd_net_write(server->clientSock, "Date: ", 6);
    _httpd_net_write(server->clientSock, timeBuf, strlen(timeBuf));
    _httpd_net_write(server->clientSock, "\n", 1);

    _httpd_net_write(server->clientSock, "Connection: close\n", 18);
    _httpd_net_write(server->clientSock, "Content-Type: ", 14);
    _httpd_net_write(server->clientSock, server->response.contentType,
        strlen(server->response.contentType));
    _httpd_net_write(server->clientSock, "\n", 1);

    if (contentLength > 0) {
        _httpd_net_write(server->clientSock, "Content-Length: ", 16);
        snprintf(tmpBuf, sizeof(tmpBuf), "%d", contentLength);
        _httpd_net_write(server->clientSock, tmpBuf, strlen(tmpBuf));
        _httpd_net_write(server->clientSock, "\n", 1);

        _httpd_formatTimeString(server, timeBuf, modTime);
        _httpd_net_write(server->clientSock, "Last-Modified: ", 15);
        _httpd_net_write(server->clientSock, timeBuf, strlen(timeBuf));
        _httpd_net_write(server->clientSock, "\n", 1);
    }
    _httpd_net_write(server->clientSock, "\n", 1);
}

httpDir* _httpd_findContentDir(server, dir, createFlag)
    httpd* server;
char* dir;
int createFlag;
{
    char buffer[HTTP_MAX_URL],
        *curDir, *next_start;
    httpDir *curItem,
        *curChild;

    strncpy(buffer, dir, HTTP_MAX_URL);
    curItem = server->content;
    curDir = strtok_r(buffer, "/", &next_start);
    while (curDir) {
        curChild = curItem->children;
        while (curChild) {
            if (strcmp(curChild->name, curDir) == 0)
                break;
            curChild = curChild->next;
        }
        if (curChild == NULL) {
            if (createFlag == HTTP_TRUE) {
                curChild = malloc(sizeof(httpDir));
                bzero(curChild, sizeof(httpDir));
                curChild->name = strdup(curDir);
                curChild->next = curItem->children;
                curItem->children = curChild;
            } else {
                return (NULL);
            }
        }
        curItem = curChild;
        curDir = strtok_r(NULL, "/", &next_start);
    }
    return (curItem);
}

httpContent* _httpd_findContentEntry(server, dir, entryName)
    httpd* server;
httpDir* dir;
char* entryName;
{
    httpContent* curEntry;

    curEntry = dir->entries;
    while (curEntry) {
        if (curEntry->type == HTTP_WILDCARD || curEntry->type == HTTP_C_WILDCARD)
            break;
        if (*entryName == 0 && curEntry->indexFlag)
            break;
        if (strcmp(curEntry->name, entryName) == 0)
            break;
        curEntry = curEntry->next;
    }
    if (curEntry)
        server->response.content = curEntry;
    return (curEntry);
}

void _httpd_send304(server)
    httpd* server;
{
    httpdSetResponse(server, "304 Not Modified\n");
    if (server->errorFunction304) {
        (server->errorFunction304)(server, 304);
    } else {
        _httpd_sendHeaders(server, 0, 0);
    }
}

void _httpd_send403(server)
    httpd* server;
{
    httpdSetResponse(server, "403 Permission Denied\n");
    if (server->errorFunction403) {
        (server->errorFunction403)(server, 403);
    } else {
        _httpd_sendHeaders(server, 0, 0);
        _httpd_sendText(server,
            "<HTML><HEAD><TITLE>403 Permission Denied</TITLE></HEAD>\n");
        _httpd_sendText(server,
            "<BODY><H1>Access to the request URL was denied!</H1>\n");
    }
}

void _httpd_send404(server)
    httpd* server;
{
    char msg[HTTP_MAX_URL];

    snprintf(msg, HTTP_MAX_URL,
        "File does not exist: %s", server->request.path);
    _httpd_writeErrorLog(server, LEVEL_ERROR, msg);
    httpdSetResponse(server, "404 Not Found\n");
    if (server->errorFunction404) {
        (server->errorFunction404)(server, 404);
    } else {
        _httpd_sendHeaders(server, 0, 0);
        _httpd_sendText(server,
            "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n");
        _httpd_sendText(server,
            "<BODY><H1>The request URL was not found!</H1>\n");
        _httpd_sendText(server, "</BODY></HTML>\n");
    }
}

void _httpd_catFile(server, path, mode)
    httpd* server;
char* path;
int mode;
{
    int fd,
        readLen,
        writeLen;
    char buf[HTTP_MAX_LEN];

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return;
    readLen = read(fd, buf, HTTP_MAX_LEN - 1);
    while (readLen > 0) {
        if (mode == HTTP_RAW_DATA) {
            server->response.responseLength += readLen;
            _httpd_net_write(server->clientSock, buf, readLen);
        } else {
            buf[readLen] = 0;
            writeLen = _httpd_sendExpandedText(server, buf, readLen);
            server->response.responseLength += writeLen;
        }
        readLen = read(fd, buf, HTTP_MAX_LEN - 1);
    }
    close(fd);
}

void _httpd_sendStatic(server, data)
    httpd* server;
char* data;
{
    if (_httpd_checkLastModified(server, server->startTime) == 0) {
        _httpd_send304(server);
    }
    _httpd_sendHeaders(server, server->startTime, strlen(data));
    httpdOutput(server, data);
}

void _httpd_sendFile(server, path)
    httpd* server;
char* path;
{
    char* suffix;
    struct stat sbuf;

    suffix = strrchr(path, '.');
    if (suffix != NULL) {
        if (strcasecmp(suffix, ".gif") == 0)
            strcpy(server->response.contentType, "image/gif");
        else if (strcasecmp(suffix, ".jpg") == 0)
            strcpy(server->response.contentType, "image/jpeg");
        else if (strcasecmp(suffix, ".xbm") == 0)
            strcpy(server->response.contentType, "image/xbm");
        else if (strcasecmp(suffix, ".png") == 0)
            strcpy(server->response.contentType, "image/png");
        else if (strcasecmp(suffix, ".css") == 0)
            strcpy(server->response.contentType, "text/css");
        else if (strcasecmp(suffix, ".htm") == 0 || strcasecmp(suffix, ".html") == 0)
            strcpy(server->response.contentType, "text/html");
        else if (strcasecmp(suffix, ".txt") == 0)
            strcpy(server->response.contentType, "text/plain");
        else
            strcpy(server->response.contentType, "application/octet-stream");
    }
    if (stat(path, &sbuf) < 0) {
        _httpd_send404(server);
        return;
    }
    if (_httpd_checkLastModified(server, sbuf.st_mtime) == 0) {
        _httpd_send304(server);
    } else {
        _httpd_sendHeaders(server, sbuf.st_size, sbuf.st_mtime);

        //if (strncmp(server->response.contentType,"text/",5) == 0)
        //	_httpd_catFile(server, path, HTTP_EXPAND_TEXT);
        //else
        _httpd_catFile(server, path, HTTP_RAW_DATA);
    }
}

int _httpd_sendDirectoryEntry(server, entry, entryName)
    httpd* server;
httpContent* entry;
char* entryName;
{
    char path[HTTP_MAX_URL];

    snprintf(path, HTTP_MAX_URL, "%s/%s", entry->path, entryName);
    _httpd_sendFile(server, path);
    return (0);
}

void _httpd_sendText(server, msg)
    httpd* server;
char* msg;
{
    server->response.responseLength += strlen(msg);
    _httpd_net_write(server->clientSock, msg, strlen(msg));
}

int _httpd_checkLastModified(server, modTime)
    httpd* server;
int modTime;
{
    char timeBuf[HTTP_TIME_STRING_LEN];

    _httpd_formatTimeString(server, timeBuf, modTime);
    if (strcmp(timeBuf, server->request.ifModified) == 0)
        return (0);
    return (1);
}

static unsigned char isAcceptable[96] =

/* Overencodes */
#define URL_XALPHAS (unsigned char)1
#define URL_XPALPHAS (unsigned char)2

    /*      Bit 0           xalpha          -- see HTFile.h
**      Bit 1           xpalpha         -- as xalpha but with plus.
**      Bit 2 ...       path            -- as xpalpha but with /
*/
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    { 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 7, 7, 7, /* 2x   !"#$%&'()*+,-./ */
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0, 0, 0, /* 3x  0123456789:;<=>?  */
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, /* 4x  @ABCDEFGHIJKLMNO */
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0, 7, /* 5X  PQRSTUVWXYZ[\]^_ */
        0, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, /* 6x  `abcdefghijklmno */
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0, 0 }; /* 7X  pqrstuvwxyz{\}~ DEL */

#define ACCEPTABLE(a) (a >= 32 && a < 128 && ((isAcceptable[a - 32]) & mask))

static char* hex = "0123456789ABCDEF";

char* _httpd_escape(str) char* str;
{
    unsigned char mask = URL_XPALPHAS;
    char* p;
    char* q;
    char* result;
    int unacceptable = 0;
    for (p = str; *p; p++)
        if (!ACCEPTABLE((unsigned char)*p))
            unacceptable += 2;
    result = (char*)malloc(p - str + unacceptable + 1);
    bzero(result, (p - str + unacceptable + 1));

    if (result == NULL) {
        return (NULL);
    }
    for (q = result, p = str; *p; p++) {
        unsigned char a = *p;
        if (!ACCEPTABLE(a)) {
            *q++ = '%'; /* Means hex commming */
            *q++ = hex[a >> 4];
            *q++ = hex[a & 15];
        } else
            *q++ = *p;
    }
    *q++ = 0; /* Terminate */
    return result;
}

void _httpd_sanitiseUrl(url) char* url;
{
    char *from,
        *to,
        *last;

    /*
	** Remove multiple slashes
	*/
    from = to = url;
    while (*from) {
        if (*from == '/' && *(from + 1) == '/') {
            from++;
            continue;
        }
        *to = *from;
        to++;
        from++;
    }
    *to = 0;

    /*
	** Get rid of ./ sequences
	*/
    from = to = url;
    while (*from) {
        if (*from == '/' && *(from + 1) == '.' && *(from + 2) == '/') {
            from += 2;
            continue;
        }
        *to = *from;
        to++;
        from++;
    }
    *to = 0;

    /*
	** Catch use of /../ sequences and remove them.  Must track the
	** path structure and remove the previous path element.
	*/
    from = to = last = url;
    while (*from) {
        if (*from == '/' && *(from + 1) == '.' && *(from + 2) == '.' && *(from + 3) == '/') {
            to = last;
            from += 3;
            continue;
        }
        if (*from == '/') {
            last = to;
        }
        *to = *from;
        to++;
        from++;
    }
    *to = 0;
}
