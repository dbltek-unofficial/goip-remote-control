#ifndef STUN_H
#define STUN_H

//#include <openssl/hmac.h>
//#include<openssl/evp.h>

#define getErrno() (errno)
#define true 1
#define false 0

// if you change this version, change in makefile too
#define STUN_VERSION_MAJOR 0
#define STUN_VERSION_MINOR 92

#define STUN_MAX_STRING 256
#define STUN_MAX_UNKNOWN_ATTRIBUTES 8
#define STUN_MAX_MESSAGE_SIZE 2048

#define STUN_PORT 3478

#define MAX_TTL 30
// define some basic types
typedef int Socket;
typedef unsigned char UInt8;
typedef unsigned short UInt16;
typedef unsigned int UInt32;
typedef unsigned long long UInt64;
typedef struct
{
    unsigned char octet[16];
} UInt128;

/// define a structure to hold a stun address
#define IPv4Family 0x01
#define IPv6Family 0x02

// define  flags
#define ChangeIpFlag 4
#define ChangePortFlag 2

// define  stun attribute
#define MappedAddress 0x0001
#define ResponseAddress 0x0002
#define ChangeRequest 0x0003
#define SourceAddress 0x0004
#define ChangedAddress 0x0005
#define Username 0x0006
#define Password 0x0007
#define MessageIntegrity 0x0008
#define ErrorCode 0x0009
#define UnknownAttribute 0x000A
#define ReflectedFrom 0x000B

// define types for a stun message
#define BindRequestMsg 0x0001
#define BindResponseMsg 0x0101
#define BindErrorResponseMsg 0x0111
#define SharedSecretRequestMsg 0x0002
#define SharedSecretResponseMsg 0x0102
#define SharedSecretErrorResponseMsg 0x0112

typedef struct
{
    UInt16 msgType;
    UInt16 msgLength;
    UInt128 id;
} StunMsgHdr;

typedef struct
{
    UInt16 type;
    UInt16 length;
} StunAtrHdr;

typedef struct
{
    UInt16 port;
    UInt32 addr;
} StunAddress4;

typedef struct
{
    UInt8 pad;
    UInt8 family;
    StunAddress4 ipv4;
} StunAtrAddress4;

typedef struct
{
    UInt32 value;
} StunAtrChangeRequest;

typedef struct
{
    UInt16 pad; // all 0
    UInt8 errorClass;
    UInt8 number;
    char reason[STUN_MAX_STRING];
    UInt16 sizeReason;
} StunAtrError;

typedef struct
{
    UInt16 attrType[STUN_MAX_UNKNOWN_ATTRIBUTES];
    UInt16 numAttributes;
} StunAtrUnknown;

typedef struct
{
    char value[STUN_MAX_STRING];
    UInt16 sizeValue;
} StunAtrString;

typedef struct
{
    char hash[20];
} StunAtrIntegrity;

typedef enum {
    HmacUnkown = 0,
    HmacOK,
    HmacBadUserName,
    HmacUnkownUserName,
    HmacFailed,
} StunHmacStatus;

typedef struct
{
    StunMsgHdr msgHdr;

    int hasMappedAddress;
    StunAtrAddress4 mappedAddress;

    int hasResponseAddress;
    StunAtrAddress4 responseAddress;

    int hasChangeRequest;
    StunAtrChangeRequest changeRequest;

    int hasSourceAddress;
    StunAtrAddress4 sourceAddress;

    int hasChangedAddress;
    StunAtrAddress4 changedAddress;

    int hasUsername;
    StunAtrString username;

    int hasPassword;
    StunAtrString password;

    int hasMessageIntegrity;
    StunAtrIntegrity messageIntegrity;

    int hasErrorCode;
    StunAtrError errorCode;

    int hasUnknownAttributes;
    StunAtrUnknown unknownAttributes;

    int hasReflectedFrom;
    StunAtrAddress4 reflectedFrom;
} StunMessage;

// Define enum with different types of NAT
typedef enum {
    StunTypeUnknown = 0,
    StunTypeOpen,
    StunTypeConeNat,
    StunTypeRestrictedNat,
    StunTypePortRestrictedNat,
    StunTypeSymNat,
    StunTypeSymFirewall,
    StunTypeBlocked,
    StunTypeFailure
} NatType;

//typedef int Socket;

typedef struct
{
    StunAddress4 myAddr;
    StunAddress4 altAddr;
    Socket myFd;
    Socket altPortFd;
    Socket altIpFd;
    Socket altIpPortFd;
} StunServerInfo;

void printStunAddress4(StunAddress4 stunAddr);

int stunParseMessage(char* buf,
    unsigned int bufLen,
    StunMessage* pmsg,
    int verbose);

void stunBuildReqSimple(StunMessage* msg,
    const StunAtrString* pusername,
    int changePort, int changeIp, unsigned int id);

unsigned int
stunEncodeMessage(const StunMessage* pmessage,
    char* buf,
    unsigned int bufLen,
    const StunAtrString* ppassword,
    int verbose);

void stunCreateUserName(const StunAddress4* paddr, StunAtrString* username);

void stunGetUserNameAndPassword(const StunAddress4* pdest,
    StunAtrString* username,
    StunAtrString* password);

void stunCreatePassword(const StunAtrString* pusername, StunAtrString* password);

int stunRand();

UInt64
stunGetSystemTimeSecs();

/// find the IP address of a the specified stun server - return false is fails parse
int stunParseServerName(char* serverName, StunAddress4* pstunServerAddr);

int stunParseHostName(char* peerName, UInt32* pip, UInt16* pportVal, UInt16 defaultPort);

#endif
