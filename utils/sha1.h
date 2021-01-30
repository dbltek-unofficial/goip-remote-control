/* sha1.c : Implementation of the Secure Hash Algorithm */

/* SHA: NIST's Secure Hash Algorithm */

/*	This version written November 2000 by David Ireland of 
	DI Management Services Pty Limited <code@di-mgt.com.au>

	Adapted from code in the Python Cryptography Toolkit, 
	version 1.0.0 by A.M. Kuchling 1995.
*/

/* AM Kuchling's posting:- 
   Based on SHA code originally posted to sci.crypt by Peter Gutmann
   in message <30ajo5$oe8@ccu2.auckland.ac.nz>.
   Modified to test for endianness on creation of SHA objects by AMK.
   Also, the original specification of SHA was found to have a weakness
   by NSA/NIST.  This code implements the fixed version of SHA.
*/

/* Here's the first paragraph of Peter Gutmann's posting:
   
The following is my SHA (FIPS 180) code updated to allow use of the "fixed"
SHA, thanks to Jim Gillogly and an anonymous contributor for the information on
what's changed in the new version.  The fix is a simple change which involves
adding a single rotate in the initial expansion function.  It is unknown
whether this is an optimal solution to the problem which was discovered in the
SHA or whether it's simply a bandaid which fixes the problem with a minimum of
effort (for example the reengineering of a great many Capstone chips).
*/

/* h files included here to make this just one file ... */

/* global.h */

#ifndef __SHA1_H
#define __SHA1_H

#include "datalist.h"

#ifndef _GLOBAL_H_
#define _GLOBAL_H_ 1

/* POINTER defines a generic pointer type */
typedef unsigned char* POINTER;

/* UINT4 defines a four byte word */
typedef unsigned long int UINT4;

/* BYTE defines a unsigned character */
typedef unsigned char BYTE;

#ifndef TRUE
#define FALSE 0
#define TRUE (!FALSE)
#endif /* TRUE */

#endif /* end _GLOBAL_H_ */

/* sha.h */

#ifndef _SHA_H_
#define _SHA_H_ 1

/* #include "global.h" */

/* The structure for storing SHS info */

typedef struct
{
    UINT4 digest[5]; /* Message digest */
    UINT4 countLo, countHi; /* 64-bit bit count */
    UINT4 data[16]; /* SHS data buffer */
    int Endianness;
} SHA_CTX;

typedef struct
{
    unsigned char* text;
    int len;
} SHA_TEXT;

/* Message digest functions */

void SHAInit(SHA_CTX*);
void SHAUpdate(SHA_CTX*, BYTE* buffer, int count);
void SHAFinal(BYTE* output, SHA_CTX*);
void hmac_sha2(struct datalist* texts, unsigned char* key, int key_len, unsigned char* digest);

#endif /* end _SHA_H_ */

/* endian.h */

#ifndef _ENDIAN_H_
#define _ENDIAN_H_ 1

void endianTest(int* endianness);

#endif /* end _ENDIAN_H_ */

/* sha.c */

#include <stdio.h>
#include <string.h>

static void SHAtoByte(BYTE* output, UINT4* input, unsigned int len);

/* The SHS block size and message digest sizes, in bytes */

#define SHS_DATASIZE 64
#define SHS_DIGESTSIZE 20

/* The SHS f()-functions.  The f1 and f3 functions can be optimized to
   save one boolean operation each - thanks to Rich Schroeppel,
   rcs@cs.arizona.edu for discovering this */

/*#define f1(x,y,z) ( ( x & y ) | ( ~x & z ) )          // Rounds  0-19 */
#define f1(x, y, z) (z ^ (x & (y ^ z))) /* Rounds  0-19 */
#define f2(x, y, z) (x ^ y ^ z) /* Rounds 20-39 */
/*#define f3(x,y,z) ( ( x & y ) | ( x & z ) | ( y & z ) )   // Rounds 40-59 */
#define f3(x, y, z) ((x & y) | (z & (x | y))) /* Rounds 40-59 */
#define f4(x, y, z) (x ^ y ^ z) /* Rounds 60-79 */

/* The SHS Mysterious Constants */

#define K1 0x5A827999L /* Rounds  0-19 */
#define K2 0x6ED9EBA1L /* Rounds 20-39 */
#define K3 0x8F1BBCDCL /* Rounds 40-59 */
#define K4 0xCA62C1D6L /* Rounds 60-79 */

/* SHS initial values */

#define h0init 0x67452301L
#define h1init 0xEFCDAB89L
#define h2init 0x98BADCFEL
#define h3init 0x10325476L
#define h4init 0xC3D2E1F0L

/* Note that it may be necessary to add parentheses to these macros if they
   are to be called with expressions as arguments */
/* 32-bit rotate left - kludged with shifts */

#define ROTL(n, X) (((X) << n) | ((X) >> (32 - n)))

/* The initial expanding function.  The hash function is defined over an
   80-UINT2 expanded input array W, where the first 16 are copies of the input
   data, and the remaining 64 are defined by

        W[ i ] = W[ i - 16 ] ^ W[ i - 14 ] ^ W[ i - 8 ] ^ W[ i - 3 ]

   This implementation generates these values on the fly in a circular
   buffer - thanks to Colin Plumb, colin@nyx10.cs.du.edu for this
   optimization.

   The updated SHS changes the expanding function by adding a rotate of 1
   bit.  Thanks to Jim Gillogly, jim@rand.org, and an anonymous contributor
   for this information */

#define expand(W, i) (W[i & 15] = ROTL(1, (W[i & 15] ^ W[(i - 14) & 15] ^ W[(i - 8) & 15] ^ W[(i - 3) & 15])))

/* The prototype SHS sub-round.  The fundamental sub-round is:

        a' = e + ROTL( 5, a ) + f( b, c, d ) + k + data;
        b' = a;
        c' = ROTL( 30, b );
        d' = c;
        e' = d;

   but this is implemented by unrolling the loop 5 times and renaming the
   variables ( e, a, b, c, d ) = ( a', b', c', d', e' ) each iteration.
   This code is then replicated 20 times for each of the 4 functions, using
   the next 20 values from the W[] array each time */

#define subRound(a, b, c, d, e, f, k, data) (e += ROTL(5, a) + f(b, c, d) + k + data, b = ROTL(30, b))

#define SHA_DIGEST_LENGTH 20
#endif
