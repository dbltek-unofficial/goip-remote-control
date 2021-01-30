#include "sha1.h"

/* Initialize the SHS values */

void SHAInit(SHA_CTX* shsInfo)
{
    endianTest(&shsInfo->Endianness);
    /* Set the h-vars to their initial values */
    shsInfo->digest[0] = h0init;
    shsInfo->digest[1] = h1init;
    shsInfo->digest[2] = h2init;
    shsInfo->digest[3] = h3init;
    shsInfo->digest[4] = h4init;

    /* Initialise bit count */
    shsInfo->countLo = shsInfo->countHi = 0;
}

/* Perform the SHS transformation.  Note that this code, like SHA, seems to
   break some optimizing compilers due to the complexity of the expressions
   and the size of the basic block.  It may be necessary to split it into
   sections, e.g. based on the four subrounds

   Note that this corrupts the shsInfo->data area */

static void SHSTransform(digest, data)
    UINT4 *digest,
    *data;
{
    UINT4 A, B, C, D, E; /* Local vars */
    UINT4 eData[16]; /* Expanded data */

    /* Set up first buffer and local data buffer */
    A = digest[0];
    B = digest[1];
    C = digest[2];
    D = digest[3];
    E = digest[4];
    memcpy((POINTER)eData, (POINTER)data, SHS_DATASIZE);

    /* Heavy mangling, in 4 sub-rounds of 20 interations each. */
    subRound(A, B, C, D, E, f1, K1, eData[0]);
    subRound(E, A, B, C, D, f1, K1, eData[1]);
    subRound(D, E, A, B, C, f1, K1, eData[2]);
    subRound(C, D, E, A, B, f1, K1, eData[3]);
    subRound(B, C, D, E, A, f1, K1, eData[4]);
    subRound(A, B, C, D, E, f1, K1, eData[5]);
    subRound(E, A, B, C, D, f1, K1, eData[6]);
    subRound(D, E, A, B, C, f1, K1, eData[7]);
    subRound(C, D, E, A, B, f1, K1, eData[8]);
    subRound(B, C, D, E, A, f1, K1, eData[9]);
    subRound(A, B, C, D, E, f1, K1, eData[10]);
    subRound(E, A, B, C, D, f1, K1, eData[11]);
    subRound(D, E, A, B, C, f1, K1, eData[12]);
    subRound(C, D, E, A, B, f1, K1, eData[13]);
    subRound(B, C, D, E, A, f1, K1, eData[14]);
    subRound(A, B, C, D, E, f1, K1, eData[15]);
    subRound(E, A, B, C, D, f1, K1, expand(eData, 16));
    subRound(D, E, A, B, C, f1, K1, expand(eData, 17));
    subRound(C, D, E, A, B, f1, K1, expand(eData, 18));
    subRound(B, C, D, E, A, f1, K1, expand(eData, 19));

    subRound(A, B, C, D, E, f2, K2, expand(eData, 20));
    subRound(E, A, B, C, D, f2, K2, expand(eData, 21));
    subRound(D, E, A, B, C, f2, K2, expand(eData, 22));
    subRound(C, D, E, A, B, f2, K2, expand(eData, 23));
    subRound(B, C, D, E, A, f2, K2, expand(eData, 24));
    subRound(A, B, C, D, E, f2, K2, expand(eData, 25));
    subRound(E, A, B, C, D, f2, K2, expand(eData, 26));
    subRound(D, E, A, B, C, f2, K2, expand(eData, 27));
    subRound(C, D, E, A, B, f2, K2, expand(eData, 28));
    subRound(B, C, D, E, A, f2, K2, expand(eData, 29));
    subRound(A, B, C, D, E, f2, K2, expand(eData, 30));
    subRound(E, A, B, C, D, f2, K2, expand(eData, 31));
    subRound(D, E, A, B, C, f2, K2, expand(eData, 32));
    subRound(C, D, E, A, B, f2, K2, expand(eData, 33));
    subRound(B, C, D, E, A, f2, K2, expand(eData, 34));
    subRound(A, B, C, D, E, f2, K2, expand(eData, 35));
    subRound(E, A, B, C, D, f2, K2, expand(eData, 36));
    subRound(D, E, A, B, C, f2, K2, expand(eData, 37));
    subRound(C, D, E, A, B, f2, K2, expand(eData, 38));
    subRound(B, C, D, E, A, f2, K2, expand(eData, 39));

    subRound(A, B, C, D, E, f3, K3, expand(eData, 40));
    subRound(E, A, B, C, D, f3, K3, expand(eData, 41));
    subRound(D, E, A, B, C, f3, K3, expand(eData, 42));
    subRound(C, D, E, A, B, f3, K3, expand(eData, 43));
    subRound(B, C, D, E, A, f3, K3, expand(eData, 44));
    subRound(A, B, C, D, E, f3, K3, expand(eData, 45));
    subRound(E, A, B, C, D, f3, K3, expand(eData, 46));
    subRound(D, E, A, B, C, f3, K3, expand(eData, 47));
    subRound(C, D, E, A, B, f3, K3, expand(eData, 48));
    subRound(B, C, D, E, A, f3, K3, expand(eData, 49));
    subRound(A, B, C, D, E, f3, K3, expand(eData, 50));
    subRound(E, A, B, C, D, f3, K3, expand(eData, 51));
    subRound(D, E, A, B, C, f3, K3, expand(eData, 52));
    subRound(C, D, E, A, B, f3, K3, expand(eData, 53));
    subRound(B, C, D, E, A, f3, K3, expand(eData, 54));
    subRound(A, B, C, D, E, f3, K3, expand(eData, 55));
    subRound(E, A, B, C, D, f3, K3, expand(eData, 56));
    subRound(D, E, A, B, C, f3, K3, expand(eData, 57));
    subRound(C, D, E, A, B, f3, K3, expand(eData, 58));
    subRound(B, C, D, E, A, f3, K3, expand(eData, 59));

    subRound(A, B, C, D, E, f4, K4, expand(eData, 60));
    subRound(E, A, B, C, D, f4, K4, expand(eData, 61));
    subRound(D, E, A, B, C, f4, K4, expand(eData, 62));
    subRound(C, D, E, A, B, f4, K4, expand(eData, 63));
    subRound(B, C, D, E, A, f4, K4, expand(eData, 64));
    subRound(A, B, C, D, E, f4, K4, expand(eData, 65));
    subRound(E, A, B, C, D, f4, K4, expand(eData, 66));
    subRound(D, E, A, B, C, f4, K4, expand(eData, 67));
    subRound(C, D, E, A, B, f4, K4, expand(eData, 68));
    subRound(B, C, D, E, A, f4, K4, expand(eData, 69));
    subRound(A, B, C, D, E, f4, K4, expand(eData, 70));
    subRound(E, A, B, C, D, f4, K4, expand(eData, 71));
    subRound(D, E, A, B, C, f4, K4, expand(eData, 72));
    subRound(C, D, E, A, B, f4, K4, expand(eData, 73));
    subRound(B, C, D, E, A, f4, K4, expand(eData, 74));
    subRound(A, B, C, D, E, f4, K4, expand(eData, 75));
    subRound(E, A, B, C, D, f4, K4, expand(eData, 76));
    subRound(D, E, A, B, C, f4, K4, expand(eData, 77));
    subRound(C, D, E, A, B, f4, K4, expand(eData, 78));
    subRound(B, C, D, E, A, f4, K4, expand(eData, 79));

    /* Build message digest */
    digest[0] += A;
    digest[1] += B;
    digest[2] += C;
    digest[3] += D;
    digest[4] += E;
}

/* When run on a little-endian CPU we need to perform byte reversal on an
   array of long words. */

static void longReverse(UINT4* buffer, int byteCount, int Endianness)
{
    UINT4 value;

    if (Endianness == TRUE)
        return;
    byteCount /= sizeof(UINT4);
    while (byteCount--) {
        value = *buffer;
        value = ((value & 0xFF00FF00L) >> 8) | ((value & 0x00FF00FFL) << 8);
        *buffer++ = (value << 16) | (value >> 16);
    }
}

/* Update SHS for a block of data */

void SHAUpdate(SHA_CTX* shsInfo, BYTE* buffer, int count)
{
    UINT4 tmp;
    int dataCount;

    /* Update bitcount */
    tmp = shsInfo->countLo;
    if ((shsInfo->countLo = tmp + ((UINT4)count << 3)) < tmp)
        shsInfo->countHi++; /* Carry from low to high */
    shsInfo->countHi += count >> 29;

    /* Get count of bytes already in data */
    dataCount = (int)(tmp >> 3) & 0x3F;

    /* Handle any leading odd-sized chunks */
    if (dataCount) {
        BYTE* p = (BYTE*)shsInfo->data + dataCount;

        dataCount = SHS_DATASIZE - dataCount;
        if (count < dataCount) {
            memcpy(p, buffer, count);
            return;
        }
        memcpy(p, buffer, dataCount);
        longReverse(shsInfo->data, SHS_DATASIZE, shsInfo->Endianness);
        SHSTransform(shsInfo->digest, shsInfo->data);
        buffer += dataCount;
        count -= dataCount;
    }

    /* Process data in SHS_DATASIZE chunks */
    while (count >= SHS_DATASIZE) {
        memcpy((POINTER)shsInfo->data, (POINTER)buffer, SHS_DATASIZE);
        longReverse(shsInfo->data, SHS_DATASIZE, shsInfo->Endianness);
        SHSTransform(shsInfo->digest, shsInfo->data);
        buffer += SHS_DATASIZE;
        count -= SHS_DATASIZE;
    }

    /* Handle any remaining bytes of data. */
    memcpy((POINTER)shsInfo->data, (POINTER)buffer, count);
}

/* Final wrapup - pad to SHS_DATASIZE-byte boundary with the bit pattern
   1 0* (64-bit count of bits processed, MSB-first) */

void SHAFinal(BYTE* output, SHA_CTX* shsInfo)
{
    int count;
    BYTE* dataPtr;

    /* Compute number of bytes mod 64 */
    count = (int)shsInfo->countLo;
    count = (count >> 3) & 0x3F;

    /* Set the first char of padding to 0x80.  This is safe since there is
       always at least one byte free */
    dataPtr = (BYTE*)shsInfo->data + count;
    *dataPtr++ = 0x80;

    /* Bytes of padding needed to make 64 bytes */
    count = SHS_DATASIZE - 1 - count;

    /* Pad out to 56 mod 64 */
    if (count < 8) {
        /* Two lots of padding:  Pad the first block to 64 bytes */
        memset(dataPtr, 0, count);
        longReverse(shsInfo->data, SHS_DATASIZE, shsInfo->Endianness);
        SHSTransform(shsInfo->digest, shsInfo->data);

        /* Now fill the next block with 56 bytes */
        memset((POINTER)shsInfo->data, 0, SHS_DATASIZE - 8);
    } else
        /* Pad block to 56 bytes */
        memset(dataPtr, 0, count - 8);

    /* Append length in bits and transform */
    shsInfo->data[14] = shsInfo->countHi;
    shsInfo->data[15] = shsInfo->countLo;

    longReverse(shsInfo->data, SHS_DATASIZE - 8, shsInfo->Endianness);
    SHSTransform(shsInfo->digest, shsInfo->data);

    /* Output to an array of bytes */
    SHAtoByte(output, shsInfo->digest, SHS_DIGESTSIZE);

    /* Zeroise sensitive stuff */
    memset((POINTER)shsInfo, 0, sizeof(shsInfo));
}

static void SHAtoByte(BYTE* output, UINT4* input, unsigned int len)
{ /* Output SHA digest in byte array */
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j + 3] = (BYTE)(input[i] & 0xff);
        output[j + 2] = (BYTE)((input[i] >> 8) & 0xff);
        output[j + 1] = (BYTE)((input[i] >> 16) & 0xff);
        output[j] = (BYTE)((input[i] >> 24) & 0xff);
    }
}

void hmac_sha(unsigned char* text, int text_len, unsigned char* key, int key_len, unsigned char* digest)
{
    SHA_CTX context;
    unsigned char k_ipad[65]; /* inner padding -
									  * key XORd with ipad
									 *                                       */
    unsigned char k_opad[65]; /* outer padding -
									  * key XORd with opad
									 *                                       */
    unsigned char tk[20];
    int i;
    /*
		malloc(key, 16*sizeof(unsigned char));
		bzero(key, sizeof(key));
		for(i=0; i<16; i++){
			key=0x0b;
		}
*/

    /* if key is longer than 64 bytes reset it to key=SHA(key) */
    if (key_len > 64) {

        SHA_CTX tctx;

        SHAInit(&tctx);
        SHAUpdate(&tctx, key, key_len);
        SHAFinal(tk, &tctx);

        key = tk;
        key_len = 20;
    }

    /*
		*          * the HMAC_SHA transform looks like:
		*          *
		*   	   * SHA(K XOR opad, SHA(K XOR ipad, text))
		* 	       *
		*          * where K is an n byte key
		*          * ipad is the byte 0x36 repeated 64 times
		*
		*/

    /* opad is the byte 0x5c repeated 64 times
		 *          * and text is the data being protected
		 *                   */

    /* start out by storing key in pads */
    bzero(k_ipad, sizeof k_ipad);
    bzero(k_opad, sizeof k_opad);
    bcopy(key, k_ipad, key_len);
    bcopy(key, k_opad, key_len);

    /* XOR key with ipad and opad values */
    for (i = 0; i < 64; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }
    /*
		*          * perform inner SHA
		*                   */
    SHAInit(&context); /* init context for 1st
											  * pass */
    SHAUpdate(&context, k_ipad, 64); /* start with inner pad */
    SHAUpdate(&context, text, text_len); /* then text of datagram */
    SHAFinal(digest, &context); /* finish up 1st pass */
    /*
		*          * perform outer SHA
		*                   */
    SHAInit(&context); /* init context for 2nd
											  * pass */
    SHAUpdate(&context, k_opad, 64); /* start with outer pad */
    SHAUpdate(&context, digest, 20); /* then results of 1st
											  * hash */
    SHAFinal(digest, &context); /* finish up 2nd pass */
}

void hmac_sha2(struct datalist* texts, unsigned char* key, int key_len, unsigned char* digest)
{
    SHA_CTX context;
    unsigned char k_ipad[65]; /* inner padding -
									  * key XORd with ipad
									 *                                       */
    unsigned char k_opad[65]; /* outer padding -
									  * key XORd with opad
									 *                                       */
    unsigned char tk[20];
    int i;
    struct datalist_iterator* it;
    /*
		malloc(key, 16*sizeof(unsigned char));
		bzero(key, sizeof(key));
		for(i=0; i<16; i++){
			key=0x0b;
		}
*/

    /* if key is longer than 64 bytes reset it to key=SHA(key) */
    if (key_len > 64) {

        SHA_CTX tctx;

        SHAInit(&tctx);
        SHAUpdate(&tctx, key, key_len);
        SHAFinal(tk, &tctx);

        key = tk;
        key_len = 20;
    }

    /*
		*          * the HMAC_SHA transform looks like:
		*          *
		*   	   * SHA(K XOR opad, SHA(K XOR ipad, text))
		* 	       *
		*          * where K is an n byte key
		*          * ipad is the byte 0x36 repeated 64 times
		*
		*/

    /* opad is the byte 0x5c repeated 64 times
		 *          * and text is the data being protected
		 *                   */

    /* start out by storing key in pads */
    bzero(k_ipad, sizeof k_ipad);
    bzero(k_opad, sizeof k_opad);
    bcopy(key, k_ipad, key_len);
    bcopy(key, k_opad, key_len);

    /* XOR key with ipad and opad values */
    for (i = 0; i < 64; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }
    /*
		*          * perform inner SHA
		*                   */
    SHAInit(&context); /* init context for 1st
											  * pass */
    SHAUpdate(&context, k_ipad, 64); /* start with inner pad */

    for (it = datalist_first(texts); it; it = datalist_next(it)) {
        SHA_TEXT* text = datalist_itdata(it);
        SHAUpdate(&context, text->text, text->len); /* then text of datagram */
    }

    SHAFinal(digest, &context); /* finish up 1st pass */
    /*
		*          * perform outer SHA
		*                   */
    SHAInit(&context); /* init context for 2nd
											  * pass */
    SHAUpdate(&context, k_opad, 64); /* start with outer pad */
    SHAUpdate(&context, digest, 20); /* then results of 1st
											  * hash */
    SHAFinal(digest, &context); /* finish up 2nd pass */
}

/* endian.c */

void endianTest(int* endian_ness)
{
    if ((*(unsigned short*)("#S") >> 8) == '#') {
        /* printf("Big endian = no change\n"); */
        *endian_ness = !(0);
    } else {
        /* printf("Little endian = swap\n"); */
        *endian_ness = 0;
    }
}

unsigned char* SHA1(const unsigned char* d, unsigned long n, unsigned char* md)
{
    SHA_CTX c;
    static unsigned char m[SHA_DIGEST_LENGTH];

    if (md == NULL)
        md = m;
    SHAInit(&c);
    SHAUpdate(&c, d, n);
    SHAFinal(md, &c);
    memset(&c, 0, sizeof(c));
    return (md);
}
