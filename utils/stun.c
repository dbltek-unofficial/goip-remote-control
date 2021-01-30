#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
//#include <openssl/hmac.h>
//#include<openssl/evp.h>

#ifdef WIN32
#include <io.h>
#include <stdlib.h>
#include <time.h>
#include <winsock2.h>
#else

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
//#include <resolv.h>
#include <net/if.h>

#endif

#if defined(__sparc__) || defined(WIN32)
#define NOSSL
#endif
#define NOSSL

#include "stun.h"
//#include "types.h"

#define ERROR(str) fprintf(stderr, str);

/*
typedef short bool;
#define true 1
#define false 0
*/

//using namespace std;

#define true 1
#define false 0

void printStunAddress4(StunAddress4 stunAddr)
{
    printf("%d.%d.%d.%d:%d\n",
        (stunAddr.addr & 0xff000000) >> 24,
        (stunAddr.addr & 0x00ff0000) >> 16,
        (stunAddr.addr & 0x0000ff00) >> 8,
        (stunAddr.addr & 0x000000ff) >> 0,
        stunAddr.port);
}

#if 0
int stun_get_metrix(struct sockaddr* pstunSrv){
	int natNum;
	int res, len;
	int probe, done;
	int ttl;
	short sport, dport;
	int sendfd, recvfd;
	char *sendbuf = "lyc";
	char recvbuf[1024 * 4];
	int addrlen;
	struct sockaddr_in localAddr, recvAddr, lastAddr, destAddr;

	sport = (getpid() & 0xffff) | 0x8000;
	sendfd = socket(AF_INET, SOCK_DGRAM, 0);
	localAddr.sin_family = AF_INET;
	localAddr.sin_port = htons(sport);
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	addrlen = sizeof(struct sockaddr_in );
	res = bind(sendfd, (struct sockaddr*)&localAddr, addrlen);
	if(res < 0){
		return -1;
	}

	if ((recvfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP))==-1){
//		LOG_FUNCTION_FAILED;
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}
	setuid(getuid());

//	bzero(&destAddr, sizeof(struct sockaddr_in ));
//	destAddr.sin_family = AF_INET;
//	memcpy(&destAddr.sin_addr,
//			&((struct sockaddr_in *)pstunSrv)->sin_addr,
//			sizeof(struct in_addr));
//	destAddr.sin_addr.s_addr = htonl(((struct sockaddr_in *)pstunSrv)->sin_addr.s_addr);

	if(memcmp(pstunSrv,
				(struct sockaddr* )&localAddr,
				sizeof(struct sockaddr_in)) == 0){
		natNum = 0;
		return natNum;
	}
	
	done = 0;
	natNum = 1;
	bzero(&lastAddr, sizeof(struct sockaddr_in ));

	for(ttl = 1; ttl <= MAX_TTL && done == 0; ttl++){
		setsockopt(sendfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(int ));
		
		for(probe = 0; probe < 3; probe++){
			int maxlen;
			fd_set rset;
			struct timeval tv;

			dport = 32768 + 666 + probe;
			bzero(&destAddr, sizeof(struct sockaddr_in));
			destAddr.sin_family = AF_INET;
			destAddr.sin_port = htons(dport);
//			memcpy(&destAddr.sin_addr,
//					&((struct sockaddr_in *)pstunSrv)->sin_addr,
//					sizeof(struct in_addr));
			destAddr.sin_addr.s_addr = htonl(((struct sockaddr_in *)pstunSrv)->sin_addr.s_addr);
			addrlen = sizeof(struct sockaddr_in );
//			fprintf(stderr, "destAddr=%s:%d\n", inet_ntoa(destAddr.sin_addr), ntohs(destAddr.sin_port));
			len = sendto(sendfd,
							sendbuf,
							sizeof(sendbuf),
							0,
							(struct sockaddr* )&destAddr,
							addrlen);
			if(len <= 0){
				return -1;
			}

			FD_ZERO(&rset);
			FD_SET(recvfd, &rset);
			maxlen = recvfd;
			tv.tv_sec  = 1;
			tv.tv_usec = 0;

			res = select(maxlen + 1, &rset, NULL, NULL, &tv);

			if(res <= 0){
//				LOG_FUNCTION_FAILED;
//				fprintf(stderr, "ttl=%d recvfd=%d error=%s\n", ttl, recvfd, strerror(errno));
				continue;
			}
			if(FD_ISSET(recvfd, &rset)){
				bzero(&recvAddr, sizeof(struct sockaddr_in ));
				addrlen = sizeof(struct sockaddr_in );
				len = recvfrom(recvfd,
								recvbuf,
								sizeof(recvbuf),
								0,
								(struct sockaddr* )&recvAddr,
								&addrlen);
				if(len <= 0){
//					if(errno == EINTR)
//						return -3;
//					else
					fprintf(stderr, "*");
						continue;
				}
				else{
					int hlen1, hlen2, icmplen;
					struct ip *ip,*hip;
					struct icmp *icmp;
					struct udphdr *udp;

					ip = (struct ip* )recvbuf;
					hlen1 = ip->ip_hl << 2;
					icmp = (struct icmp* )(recvbuf + hlen1);
					icmplen = len - hlen1;
					if(icmplen < 8){
						fprintf(stderr, "icmp len = %d < 8", icmplen);
						continue;
					}
					if(icmplen < 8 + 20 + 8){
						fprintf(stderr, "icmp len = %d < 8 + 20 + 8", icmplen);
						continue;
					}
					
					if(icmp->icmp_type == ICMP_TIMXCEED &&
							icmp->icmp_code == ICMP_TIMXCEED_INTRANS){
						hip = (struct ip* )(recvbuf + hlen1 + 8);
						hlen2 = hip->ip_hl << 2;
						udp = (struct udphdr* )(recvbuf + hlen1 + 8 + hlen2);
	
						if(hip->ip_p == IPPROTO_UDP &&
								udp->source == htons(sport) &&
								udp->dest == htons(dport)){
//							return -2;
							if(memcmp((struct sockaddr* )&recvAddr,
										(struct sockaddr* )&lastAddr,
										sizeof(struct sockaddr_in )) != 0){
								natNum++;
								memcpy((struct sockaddr* )&lastAddr,
										(struct sockaddr* )&recvAddr,
										sizeof(struct sockaddr_in ));
								continue;
							}
						}
					}
					else if(icmp->icmp_type == ICMP_UNREACH){
						hip = (struct ip* )(recvbuf + hlen1 + 8);
						hlen2 = hip->ip_hl << 2;
						udp = (struct udphdr* )(recvbuf + hlen1 + 8 + hlen2);

						if(hip->ip_p == IPPROTO_UDP &&
								udp->source == htons(sport) &&
								udp->dest == htons(dport)){
							if(icmp->icmp_code == ICMP_UNREACH_PORT){
//								return -1;
								done = 1;
							}
							else{
								fprintf(stderr,"ICMP errno=%d",icmp->icmp_code);
							}
						}
					}
				}
			}
		}
	}
	return natNum;
}

#endif

static void
computeHmac(char* hmac, const char* input, int length, const char* key, int keySize);

static int
stunParseAtrAddress(char* body, unsigned int hdrLen, StunAtrAddress4* presult)
{
    if (hdrLen != 8) {
        printf("hdrlen wrong for Address\n");
        return false;
    }
    presult->pad = *body++;
    presult->family = *body++;
    if (presult->family == IPv4Family) {
        UInt16 nport;
        UInt32 naddr;
        memcpy(&nport, body, 2);
        body += 2;
        presult->ipv4.port = ntohs(nport);

        memcpy(&naddr, body, 4);
        body += 4;
        presult->ipv4.addr = ntohl(naddr);
        return true;
    } else if (presult->family == IPv6Family) {
        printf("ipv6 not supported\n");
    } else {
        printf("bad address family: %x\n", presult->family);
    }

    return false;
}

static int
stunParseAtrChangeRequest(char* body, unsigned int hdrLen, StunAtrChangeRequest* presult)
{
    if (hdrLen != 4) {
        printf("hdr length = %d expecting %d\n", hdrLen, sizeof(*presult));
        printf("Incorrect size for ChangRequest\n");
        return false;
    } else {
        memcpy(&presult->value, body, 4);
        presult->value = ntohl(presult->value);
        return true;
    }
}

static int
stunParseAtrError(char* body, unsigned int hdrLen, StunAtrError* presult)
{
    if (hdrLen >= sizeof(*presult)) {
        printf("head on Error too large\n");
        return false;
    } else {
        memcpy(&presult->pad, body, 2);
        body += 2;
        presult->pad = ntohs(presult->pad);
        presult->errorClass = *body++;
        presult->number = *body++;

        presult->sizeReason = hdrLen - 4;
        memcpy(&presult->reason, body, presult->sizeReason);
        presult->reason[presult->sizeReason] = 0;
        return true;
    }
}

static int
stunParseAtrUnknown(char* body, unsigned int hdrLen, StunAtrUnknown* presult)
{
    int i;
    if (hdrLen >= sizeof(*presult)) {
        return false;
    } else {
        if (hdrLen % 4 != 0)
            return false;
        presult->numAttributes = hdrLen / 4;
        for (i = 0; i < presult->numAttributes; i++) {
            memcpy(&presult->attrType[i], body, 2);
            body += 2;
            presult->attrType[i] = ntohs(presult->attrType[i]);
        }
        return true;
    }
}

static int
stunParseAtrString(char* body, unsigned int hdrLen, StunAtrString* presult)
{
    if (hdrLen >= STUN_MAX_STRING) {
        printf("String is too large\n");
        return false;
    } else {
        if (hdrLen % 4 != 0) {
            printf("Bad length string %d\n", hdrLen);
            return false;
        }

        presult->sizeValue = hdrLen;
        memcpy(&(presult->value), body, hdrLen);
        presult->value[hdrLen] = 0;
        return true;
    }
}

static int
stunParseAtrIntegrity(char* body, unsigned int hdrLen, StunAtrIntegrity* presult)
{
    if (hdrLen != 20) {
        printf("MessageIntegrity must be 20 bytes\n");
        return false;
    } else {
        memcpy(&(presult->hash), body, hdrLen);
        return true;
    }
}

int stunParseMessage(char* buf, unsigned int bufLen, StunMessage* pmsg, int verbose)
{
    char* body;
    unsigned int size;
    if (verbose)
        printf("Received stun message: %d bytes\n", bufLen);
    memset(pmsg, 0, sizeof(StunMessage));

    if (sizeof(StunMsgHdr) > bufLen) {
        printf("Bad message\n");
        return false;
    }

    memcpy(&(pmsg->msgHdr), buf, sizeof(StunMsgHdr));
    pmsg->msgHdr.msgType = ntohs(pmsg->msgHdr.msgType);
    pmsg->msgHdr.msgLength = ntohs(pmsg->msgHdr.msgLength);

    if (pmsg->msgHdr.msgLength + sizeof(StunMsgHdr) != bufLen) {
        printf("Message header length doesn't match message size: %d - %d\n",
            pmsg->msgHdr.msgLength, bufLen);
        return false;
    }

    body = buf + sizeof(StunMsgHdr);
    size = pmsg->msgHdr.msgLength;

    while (size > 0) {

        StunAtrHdr* attr = (StunAtrHdr*)(body);

        unsigned int attrLen = ntohs(attr->length);
        UInt16 atrType = ntohs(attr->type);

        if (attrLen + 4 > size) {
            printf("claims attribute is larger than size of message (attribute type=%x)\n",
                atrType);
            return false;
        }

        body += 4; // skip the length and type in attribute header
        size -= 4;

        switch (atrType) {
        case 1 /*MappedAddress*/:
            pmsg->hasMappedAddress = true;
            if (stunParseAtrAddress(body, attrLen, &(pmsg->mappedAddress)) == false) {
                printf("problem parsing MappedAaddress\n");
                return false;
            } else {
                if (verbose) {
                    printf("mappedAddress is ");
                    printStunAddress4(pmsg->mappedAddress.ipv4);
                }
            }

            break;

        case 2 /*ResponseAddress*/:
            pmsg->hasResponseAddress = true;
            if (stunParseAtrAddress(body, attrLen, &(pmsg->responseAddress)) == false) {
                printf("problem parsing ResponseAddress\n");
                return false;
            } else {
                if (verbose) {
                    printf("responseAddress is ");
                    printStunAddress4(pmsg->responseAddress.ipv4);
                }
            }
            break;

        case 3 /*ChangeRequest*/:
            pmsg->hasChangeRequest = true;
            if (stunParseAtrChangeRequest(body, attrLen, &(pmsg->changeRequest)) == false) {
                printf("problem parsing ChangeRequest\n");
                return false;
            } else {
                if (verbose)
                    printf("ChangeRequest = %x\n", pmsg->changeRequest.value);
            }
            break;

        case 4 /*SourceAddress*/:
            pmsg->hasSourceAddress = true;
            if (stunParseAtrAddress(body, attrLen, &(pmsg->sourceAddress)) == false) {
                printf("problem parsing SourceAddress\n");
                return false;
            } else {
                if (verbose) {
                    printf("sourceAddress is ");
                    printStunAddress4(pmsg->sourceAddress.ipv4);
                }
            }
            break;

        case 5 /*ChangedAddress*/:
            pmsg->hasChangedAddress = true;
            if (stunParseAtrAddress(body, attrLen, &(pmsg->changedAddress)) == false) {
                printf("problem parsing ChangeAddress\n");
                return false;
            } else {
                if (verbose) {
                    printf("changedAddress is ");
                    printStunAddress4(pmsg->changedAddress.ipv4);
                }
            }
            break;

        case 6 /*Username*/:
            pmsg->hasUsername = true;
            if (stunParseAtrString(body, attrLen, &(pmsg->username)) == false) {
                //               clog << "problem parsing Username" << endl;
                printf("problem parsing Username\n");
                return false;
            } else {
                if (verbose) {
                    printf("Username = %s\n", pmsg->username.value);
                }
            }

            break;

        case 7 /*Password*/:
            pmsg->hasPassword = true;
            if (stunParseAtrString(body, attrLen, &(pmsg->password)) == false) {
                printf("problem parsing Password\n");
                return false;
            } else {
                if (verbose) {
                    printf("Password = %s\n", pmsg->password.value);
                }
            }
            break;

        case 8 /*MessageIntegrity*/:
            pmsg->hasMessageIntegrity = true;
            if (stunParseAtrIntegrity(body, attrLen, &(pmsg->messageIntegrity)) == false) {
                printf("problem parsing MessageIntegrity\n");
                return false;
            }
            // read the current HMAC
            // look up the password given the user of given the transaction id
            // compute the HMAC on the buffer
            // decide if they match or not
            break;

        case 9 /*ErrorCode*/:
            pmsg->hasErrorCode = true;
            if (stunParseAtrError(body, attrLen, &(pmsg->errorCode)) == false) {
                printf("problem parsing ErrorCode\n");
                return false;
            } else {
                if (verbose)
                    printf("Error = %d %d %s\n",
                        (int)pmsg->errorCode.errorClass,
                        (int)pmsg->errorCode.number,
                        pmsg->errorCode.reason);
            }

            break;

        case 10 /*UnknownAttribute*/:
            pmsg->hasUnknownAttributes = true;
            if (stunParseAtrUnknown(body, attrLen, &(pmsg->unknownAttributes)) == false) {
                printf("problem parsing UnknownAttribute\n");
                return false;
            }
            break;

        case 11 /*ReflectedFrom*/:
            pmsg->hasReflectedFrom = true;
            if (stunParseAtrAddress(body, attrLen, &(pmsg->reflectedFrom)) == false) {
                printf("problem parsing ReflectedFrom\n");
                return false;
            }
            break;

        default:
            if (verbose) {
                printf("Unknown attribute: %x\n", atrType);
            }
            if (atrType <= 0x7FFF)
                return false;
        }

        body += attrLen;
        size -= attrLen;
    }

    return true;
}

static char*
encode16(char* buf, UInt16 data)
{
    UInt16 ndata = htons(data);
    //   memcpy(buf, reinterpret_cast<void*>(&ndata), sizeof(UInt16));
    memcpy(buf, (void*)(&ndata), sizeof(UInt16));
    return buf + sizeof(UInt16);
}

static char*
encode32(char* buf, UInt32 data)
{
    UInt32 ndata = htonl(data);
    memcpy(buf, (void*)(&ndata), sizeof(UInt32));
    return buf + sizeof(UInt32);
}

static char*
encode(char* buf, const char* data, unsigned int length)
{
    memcpy(buf, data, length);
    return buf + length;
}

static char*
encodeAtrAddress4(char* ptr, UInt16 type, const StunAtrAddress4* patr)
{
    ptr = encode16(ptr, type);
    ptr = encode16(ptr, 8);
    *ptr++ = patr->pad;
    *ptr++ = IPv4Family;
    ptr = encode16(ptr, patr->ipv4.port);
    ptr = encode32(ptr, patr->ipv4.addr);

    return ptr;
}

static char*
encodeAtrChangeRequest(char* ptr, const StunAtrChangeRequest* patr)
{
    ptr = encode16(ptr, ChangeRequest);
    ptr = encode16(ptr, 4);
    ptr = encode32(ptr, patr->value);
    return ptr;
}

static char*
encodeAtrError(char* ptr, const StunAtrError* patr)
{
    ptr = encode16(ptr, ErrorCode);
    ptr = encode16(ptr, 6 + patr->sizeReason);
    ptr = encode16(ptr, patr->pad);
    *ptr++ = patr->errorClass;
    *ptr++ = patr->number;
    ptr = encode(ptr, patr->reason, patr->sizeReason);
    return ptr;
}

static char*
encodeAtrUnknown(char* ptr, const StunAtrUnknown* patr)
{
    int i;
    ptr = encode16(ptr, UnknownAttribute);
    ptr = encode16(ptr, 2 + 2 * (patr->numAttributes));
    for (i = 0; i < patr->numAttributes; i++) {
        ptr = encode16(ptr, patr->attrType[i]);
    }
    return ptr;
}

static char*
encodeAtrString(char* ptr, UInt16 type, const StunAtrString* patr)
{
    assert(patr->sizeValue % 4 == 0);

    ptr = encode16(ptr, type);
    ptr = encode16(ptr, patr->sizeValue);
    ptr = encode(ptr, patr->value, patr->sizeValue);
    return ptr;
}

static char*
encodeAtrIntegrity(char* ptr, const StunAtrIntegrity* patr)
{
    ptr = encode16(ptr, MessageIntegrity);
    ptr = encode16(ptr, 20);
    ptr = encode(ptr, patr->hash, sizeof(patr->hash));
    return ptr;
}

unsigned int
stunEncodeMessage(const StunMessage* pmsg,
    char* buf,
    unsigned int bufLen,
    const StunAtrString* ppassword,
    int verbose)
{

    char* lengthp;
    char* ptr = buf;
    assert(bufLen >= sizeof(StunMsgHdr));

    ptr = encode16(ptr, pmsg->msgHdr.msgType);
    lengthp = ptr;
    ptr = encode16(ptr, 0);
    //   ptr = encode(ptr, reinterpret_cast<const char*>(msg.msgHdr.id.octet), sizeof(msg.msgHdr.id));
    ptr = encode(ptr, (const char*)pmsg->msgHdr.id.octet, sizeof(pmsg->msgHdr.id));

    if (verbose)
        printf("Encoding stun message: \n");
    if (pmsg->hasMappedAddress) {
        if (verbose) {
            printf("mappedAddress is ");
            printStunAddress4(pmsg->mappedAddress.ipv4);
        }
        ptr = encodeAtrAddress4(ptr, MappedAddress, &(pmsg->mappedAddress));
    }
    if (pmsg->hasResponseAddress) {
        if (verbose) {
            printf("reponseAddress is ");
            printStunAddress4(pmsg->responseAddress.ipv4);
        }
        ptr = encodeAtrAddress4(ptr, ResponseAddress, &(pmsg->responseAddress));
    }
    if (pmsg->hasChangeRequest) {
        if (verbose)
            printf("Encoding ChangeRequest: %x\n", pmsg->changeRequest.value);
        ptr = encodeAtrChangeRequest(ptr, &(pmsg->changeRequest));
    }
    if (pmsg->hasSourceAddress) {
        if (verbose) {
            printf("sourceAddress is ");
            printStunAddress4(pmsg->sourceAddress.ipv4);
        }
        ptr = encodeAtrAddress4(ptr, SourceAddress, &(pmsg->sourceAddress));
    }
    if (pmsg->hasChangedAddress) {
        if (verbose) {
            printf("changedAddress is ");
            printStunAddress4(pmsg->changedAddress.ipv4);
        }
        ptr = encodeAtrAddress4(ptr, ChangedAddress, &(pmsg->changedAddress));
    }
    if (pmsg->hasUsername) {
        if (verbose) {
            printf("Encoding Username: %s\n", pmsg->username.value);
        }
        ptr = encodeAtrString(ptr, Username, &(pmsg->username));
    }
    if (pmsg->hasPassword) {
        if (verbose)
            printf("Encoding Password: %s\n", pmsg->password.value);
        ptr = encodeAtrString(ptr, Password, &(pmsg->password));
    }
    if (pmsg->hasErrorCode) {
        if (verbose)
            printf("Encoding ErrorCode: class=%d number=%d reason=%s\n",
                (int)(pmsg->errorCode.errorClass),
                (int)(pmsg->errorCode.number),
                pmsg->errorCode.reason);

        ptr = encodeAtrError(ptr, &(pmsg->errorCode));
    }
    if (pmsg->hasUnknownAttributes) {
        if (verbose)
            printf("Encoding UnknownAttribute: ???\n");
        ptr = encodeAtrUnknown(ptr, &(pmsg->unknownAttributes));
    }
    if (pmsg->hasReflectedFrom) {
        if (verbose) {
            printf("reflectedFrom is ");
            printStunAddress4(pmsg->reflectedFrom.ipv4);
        }
        ptr = encodeAtrAddress4(ptr, ReflectedFrom, &(pmsg->reflectedFrom));
    }
    if (ppassword->sizeValue > 0) {
        StunAtrIntegrity integrity;

        if (verbose) {
            printf("HMAC with password: %s\n", ppassword->value);
        }

        computeHmac(integrity.hash, buf, (int)(ptr - buf), ppassword->value, ppassword->sizeValue);
        ptr = encodeAtrIntegrity(ptr, &integrity);
    }
    if (verbose) {
        printf("\n");
    }

    encode16(lengthp, (UInt16)(ptr - buf - sizeof(StunMsgHdr)));
    return (int)(ptr - buf);
}

int stunRand()
{
    unsigned int seed;
    struct timeval tv;
    assert(sizeof(int) == 4);

    gettimeofday(&tv, NULL);

    seed = tv.tv_sec % 1000000000;

    srand(seed);
    return rand();
}

#if 0
/// return a random number to use as a port 
static int
randomPort()
{
   int port;

   port = random() % 65535 ;
   if( port < 1024 )
   {
     port+=1024;
   }
	
   return port;
}
#endif

#ifdef NOSSL
static void
computeHmac(char* hmac, const char* input, int length, const char* key, int sizeKey)
{
    strncpy(hmac, "hmac-not-implemented", 20);
}
#else
#include <openssl/hmac.h>

static void
computeHmac(char* hmac, const char* input, int length, const char* key, int sizeKey)
{
    unsigned int resultSize = 0;
    //   HMAC(EVP_sha1(),
    //        key, sizeKey,
    //        reinterpret_cast<const unsigned char*>(input), length,
    //        reinterpret_cast<unsigned char*>(hmac), &resultSize);
    HMAC(EVP_sha1(),
        key, sizeKey,
        (const unsigned char*)(input), length,
        (unsigned char*)(hmac), &resultSize);
    assert(resultSize == 20);
}
#endif

static void
toHex(const char* buffer, int bufferSize, char* output)
{
    int i;
    static char hexmap[] = "0123456789abcdef";

    const char* p = buffer;
    char* r = output;
    for (i = 0; i < bufferSize; i++) {
        unsigned char temp = *p++;

        int hi = (temp & 0xf0) >> 4;
        int low = (temp & 0xf);

        *r++ = hexmap[hi];
        *r++ = hexmap[low];
    }
    *r = 0;
}

#if 0
void
stunCreateUserName(const StunAddress4* psource, StunAtrString* username)
{
   UInt64 time = stunGetSystemTimeSecs();
   UInt64 lotime = time & 0xFFFFFFFF;
   char key[] = "Jason";
   char hmac[20];
   char buffer[1024];
   char hmacHex[41];
   int l;
   time -= (time % 20*60);
   //UInt64 hitime = time >> 32;
	
   sprintf(buffer,
           "%08x:%08x:%08x:", 
           (UInt32)(psource->addr),
           (UInt32)(stunRand()),
           (UInt32)(lotime));
   assert( strlen(buffer) < 1024 );
	
   assert(strlen(buffer) + 41 < STUN_MAX_STRING);
	
      computeHmac(hmac, buffer, strlen(buffer), key, strlen(key) );
   toHex(hmac, 20, hmacHex );
   hmacHex[40] =0;
	
   strcat(buffer,hmacHex);
	
   l = strlen(buffer);
   assert( l+1 < STUN_MAX_STRING );
   username->sizeValue = l;
   memcpy(username->value,buffer,l);
   username->value[l]=0;
}

void
stunCreatePassword(const StunAtrString* pusername, StunAtrString* password)
{
   char hmac[20];
   char key[] = "Fluffy";
   //char buffer[STUN_MAX_STRING];
   computeHmac(hmac, pusername->value, strlen(pusername->value), key, strlen(key));
   toHex(hmac, 20, password->value);
   password->sizeValue = 40;
   password->value[40]=0;
	
}

UInt64
stunGetSystemTimeSecs()
{
   UInt64 time=0;
#if defined(WIN32)  
   SYSTEMTIME t;
   GetSystemTime( &t );
   time = (t.wHour*60+t.wMinute)*60+t.wSecond;
#else
   struct timeval now;
   gettimeofday( &now , NULL );
   time = now.tv_sec;
#endif
   return time;
}

// returns true if it scucceeded
int 
stunParseHostName( char* peerName,
               UInt32* pip,
               UInt16* pportVal,
			   UInt16 defaultPort )
{
   struct in_addr sin_addr;
    
   struct hostent* h;
   int portNum = defaultPort;
   char* port = NULL;
   char* endPtr=NULL;
   char* sep=NULL;
   char host[512];

   strncpy(host,peerName,512);
   host[512-1]='\0';
	
	
   // pull out the port part if present.
   sep = strchr(host,':');
	
   if ( sep == NULL )
   {
      portNum = defaultPort;
   }
   else
   {
      *sep = '\0';
      port = sep + 1;
      // set port part
      portNum = strtol(port,&endPtr,10);
		
      if ( endPtr != NULL )
      {
         if ( *endPtr != '\0' )
         {
            portNum = defaultPort;
         }
      }
   }
    
   if ( portNum < 1024 ) return false;
   if ( portNum >= 0xFFFF ) return false;
	
   // figure out the host part 
   h = gethostbyname( host );
   if ( h == NULL )
   {
      int err = getErrno();
      fprintf(stderr, "error was %d\n", err);
	  *pip = ntohl( 0x7F000001L );
      return false;
   }
   else
   {
      sin_addr = *(struct in_addr*)h->h_addr;
      *pip = ntohl( sin_addr.s_addr );
   }
   *pportVal = portNum;
	
   return true;
}

#endif

void stunBuildReqSimple(StunMessage* msg,
    const StunAtrString* pusername,
    int changePort, int changeIp, unsigned int id)
{
    int i, r;
    assert(msg);
    memset(msg, 0, sizeof(*msg));

    msg->msgHdr.msgType = BindRequestMsg;

    for (i = 0; i < 16; i = i + 4) {
        assert(i + 3 < 16);
        r = stunRand();
        msg->msgHdr.id.octet[i + 0] = r >> 0;
        msg->msgHdr.id.octet[i + 1] = r >> 8;
        msg->msgHdr.id.octet[i + 2] = r >> 16;
        msg->msgHdr.id.octet[i + 3] = r >> 24;
    }

    if (id != 0) {
        msg->msgHdr.id.octet[0] = id;
    }

    msg->hasChangeRequest = true;
    msg->changeRequest.value = (changeIp ? ChangeIpFlag : 0) | (changePort ? ChangePortFlag : 0);

    if (pusername->sizeValue > 0) {
        msg->hasUsername = true;
        msg->username = *pusername;
    }
}

#if 0
void 
stunGetUserNameAndPassword(  const StunAddress4* dest, 
                             StunAtrString* username,
                             StunAtrString* password)
{ 
   // !cj! This is totally bogus - need to make TLS connection to dest and get a
   // username and password to use 
   stunCreateUserName(dest, username);
   stunCreatePassword(username, password);
}
#endif
