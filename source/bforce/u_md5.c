/*
 *	binkleyforce -- unix FTN mailer project
 *	
 *	Copyright (c) 1998-2000 Alexander Belkin, 2:5020/1398.11
 *	
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *	
 *	$Id$
 */

#include "includes.h"
#include "confread.h"
#include "logger.h"
#include "util.h"

/* Data structure for MD5 (Message-Digest) computation */
typedef struct {
	int chunks;                   /* Number of 64 byte chunks */
	unsigned int buf[4];
} s_md5;

/* F, G, H and I are basic MD5 functions */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4 */
/* Rotation is separate from addition to prevent recomputation */
#define FF(a, b, c, d, x, s, ac) \
  {(a) += F ((b), (c), (d)) + (x) + (unsigned int)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) \
  {(a) += G ((b), (c), (d)) + (x) + (unsigned int)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) \
  {(a) += H ((b), (c), (d)) + (x) + (unsigned int)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) \
  {(a) += I ((b), (c), (d)) + (x) + (unsigned int)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }

#ifdef __STDC__
#define UL(x)	x##U
#else
#define UL(x)	x
#endif

/*****************************************************************************
 * Initialise MD5 structure
 *
 * Arguments:
 * 	md        pointer to the MD5 structure
 *
 * Return value:
 * 	None
 */
static void md5_init(s_md5 *md)
{
	md->chunks = 0;
	
	md->buf[0] = (unsigned int)0x67452301;
	md->buf[1] = (unsigned int)0xefcdab89;
	md->buf[2] = (unsigned int)0x98badcfe;
	md->buf[3] = (unsigned int)0x10325476;
}

/*****************************************************************************
 * Process next data chunk
 *
 * Arguments:
 * 	md        pointer to the MD5 structure
 * 	buffer    pointer to the data chunk (must be 64 bytes)
 *
 * Return value:
 * 	None
 */
static void md5_update(s_md5 *md, const unsigned char *buffer)
{
	int i;
	unsigned int in[16];
	unsigned int a = md->buf[0];
	unsigned int b = md->buf[1];
	unsigned int c = md->buf[2];
	unsigned int d = md->buf[3];
	
	++md->chunks;
	
	/* Unpack buffer */
	for( i = 0; i < 16; i++ )
	{
		in[i] = ((unsigned int)(buffer[0])      )
		      | ((unsigned int)(buffer[1]) << 8 )
		      | ((unsigned int)(buffer[2]) << 16)
		      | ((unsigned int)(buffer[3]) << 24);
		buffer += 4;
	}
	
	/* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
	FF( a, b, c, d, in[ 0], S11, UL(3614090360)); /* 1 */
	FF( d, a, b, c, in[ 1], S12, UL(3905402710)); /* 2 */
	FF( c, d, a, b, in[ 2], S13, UL( 606105819)); /* 3 */
	FF( b, c, d, a, in[ 3], S14, UL(3250441966)); /* 4 */
	FF( a, b, c, d, in[ 4], S11, UL(4118548399)); /* 5 */
	FF( d, a, b, c, in[ 5], S12, UL(1200080426)); /* 6 */
	FF( c, d, a, b, in[ 6], S13, UL(2821735955)); /* 7 */
	FF( b, c, d, a, in[ 7], S14, UL(4249261313)); /* 8 */
	FF( a, b, c, d, in[ 8], S11, UL(1770035416)); /* 9 */
	FF( d, a, b, c, in[ 9], S12, UL(2336552879)); /* 10 */
	FF( c, d, a, b, in[10], S13, UL(4294925233)); /* 11 */
	FF( b, c, d, a, in[11], S14, UL(2304563134)); /* 12 */
	FF( a, b, c, d, in[12], S11, UL(1804603682)); /* 13 */
	FF( d, a, b, c, in[13], S12, UL(4254626195)); /* 14 */
	FF( c, d, a, b, in[14], S13, UL(2792965006)); /* 15 */
	FF( b, c, d, a, in[15], S14, UL(1236535329)); /* 16 */
	
	/* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
	GG( a, b, c, d, in[ 1], S21, UL(4129170786)); /* 17 */
	GG( d, a, b, c, in[ 6], S22, UL(3225465664)); /* 18 */
	GG( c, d, a, b, in[11], S23, UL( 643717713)); /* 19 */
	GG( b, c, d, a, in[ 0], S24, UL(3921069994)); /* 20 */
	GG( a, b, c, d, in[ 5], S21, UL(3593408605)); /* 21 */
	GG( d, a, b, c, in[10], S22, UL(  38016083)); /* 22 */
	GG( c, d, a, b, in[15], S23, UL(3634488961)); /* 23 */
	GG( b, c, d, a, in[ 4], S24, UL(3889429448)); /* 24 */
	GG( a, b, c, d, in[ 9], S21, UL( 568446438)); /* 25 */
	GG( d, a, b, c, in[14], S22, UL(3275163606)); /* 26 */
	GG( c, d, a, b, in[ 3], S23, UL(4107603335)); /* 27 */
	GG( b, c, d, a, in[ 8], S24, UL(1163531501)); /* 28 */
	GG( a, b, c, d, in[13], S21, UL(2850285829)); /* 29 */
	GG( d, a, b, c, in[ 2], S22, UL(4243563512)); /* 30 */
	GG( c, d, a, b, in[ 7], S23, UL(1735328473)); /* 31 */
	GG( b, c, d, a, in[12], S24, UL(2368359562)); /* 32 */
	
	/* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
	HH( a, b, c, d, in[ 5], S31, UL(4294588738)); /* 33 */
	HH( d, a, b, c, in[ 8], S32, UL(2272392833)); /* 34 */
	HH( c, d, a, b, in[11], S33, UL(1839030562)); /* 35 */
	HH( b, c, d, a, in[14], S34, UL(4259657740)); /* 36 */
	HH( a, b, c, d, in[ 1], S31, UL(2763975236)); /* 37 */
	HH( d, a, b, c, in[ 4], S32, UL(1272893353)); /* 38 */
	HH( c, d, a, b, in[ 7], S33, UL(4139469664)); /* 39 */
	HH( b, c, d, a, in[10], S34, UL(3200236656)); /* 40 */
	HH( a, b, c, d, in[13], S31, UL( 681279174)); /* 41 */
	HH( d, a, b, c, in[ 0], S32, UL(3936430074)); /* 42 */
	HH( c, d, a, b, in[ 3], S33, UL(3572445317)); /* 43 */
	HH( b, c, d, a, in[ 6], S34, UL(  76029189)); /* 44 */
	HH( a, b, c, d, in[ 9], S31, UL(3654602809)); /* 45 */
	HH( d, a, b, c, in[12], S32, UL(3873151461)); /* 46 */
	HH( c, d, a, b, in[15], S33, UL( 530742520)); /* 47 */
	HH( b, c, d, a, in[ 2], S34, UL(3299628645)); /* 48 */
	
	/* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
	II( a, b, c, d, in[ 0], S41, UL(4096336452)); /* 49 */
	II( d, a, b, c, in[ 7], S42, UL(1126891415)); /* 50 */
	II( c, d, a, b, in[14], S43, UL(2878612391)); /* 51 */
	II( b, c, d, a, in[ 5], S44, UL(4237533241)); /* 52 */
	II( a, b, c, d, in[12], S41, UL(1700485571)); /* 53 */
	II( d, a, b, c, in[ 3], S42, UL(2399980690)); /* 54 */
	II( c, d, a, b, in[10], S43, UL(4293915773)); /* 55 */
	II( b, c, d, a, in[ 1], S44, UL(2240044497)); /* 56 */
	II( a, b, c, d, in[ 8], S41, UL(1873313359)); /* 57 */
	II( d, a, b, c, in[15], S42, UL(4264355552)); /* 58 */
	II( c, d, a, b, in[ 6], S43, UL(2734768916)); /* 59 */
	II( b, c, d, a, in[13], S44, UL(1309151649)); /* 60 */
	II( a, b, c, d, in[ 4], S41, UL(4149444226)); /* 61 */
	II( d, a, b, c, in[11], S42, UL(3174756917)); /* 62 */
	II( c, d, a, b, in[ 2], S43, UL( 718787259)); /* 63 */
	II( b, c, d, a, in[ 9], S44, UL(3951481745)); /* 64 */
	
	md->buf[0] += a;
	md->buf[1] += b;
	md->buf[2] += c;
	md->buf[3] += d;
}

/*****************************************************************************
 * Compute MD5 digest (RFC-1321)
 *
 * Arguments:
 * 	md        pointer to the MD5 structure
 * 	digest    pointer to the resulting digest buffer (must be 16 bytes)
 * 	data      pointer to the source data
 * 	length    source data length
 *
 * Return value:
 * 	None
 */
static void md5_compute(s_md5 *md, const unsigned char *data, size_t length,
                        unsigned char *digest)
{
	unsigned char buffer[64];
	int i;
	
	while( length >= 64 )
	{
		md5_update(md, data);
		data += 64;
		length -= 64;
	}

	memcpy(buffer, data, length);
	buffer[length] = 0x80;
	memset(buffer + length + 1, '\0', 63 - length);

	if( length > 55 )
	{
		md5_update(md, buffer);
		memset(buffer, '\0', 64);
		--md->chunks;
	}

	length += (md->chunks * 64);
	length <<= 3;

	buffer[56] = (unsigned char)((length      ) & 0xff);
	buffer[57] = (unsigned char)((length >> 8 ) & 0xff);
	buffer[58] = (unsigned char)((length >> 16) & 0xff);
	buffer[59] = (unsigned char)((length >> 24) & 0xff);

	md5_update(md, buffer);

	for( i = 0; i < 4; i++ )
	{
		unsigned int x = md->buf[i];
		*digest++ = (unsigned char)((x      ) & 0xff);
		*digest++ = (unsigned char)((x >> 8 ) & 0xff);
		*digest++ = (unsigned char)((x >> 16) & 0xff);
		*digest++ = (unsigned char)((x >> 24) & 0xff);
	}
}

void md5_get(const unsigned char *data, size_t length, unsigned char *digest)
{
	s_md5 md;

	md5_init(&md);
	md5_compute(&md, data, length, digest);
}

/*****************************************************************************
 * Compute CRAM-MD5 digest (RFC-2195). It is produced by calculating
 *   
 *   MD5((secret XOR opad), MD5((secret XOR ipad), challengedata))
 *
 * where MD5 is chosen hash function, ipad and opad are 36 hex and 5C hex
 * and secret is null-padded to a length of 64 bytes. If the secret is
 * longer than 64 bytes, the hash-function digest of the secret is used as
 * an input
 * 
 * Arguments:
 * 	secret    pointer to the null-terminated secret string (e.g. password)
 * 	challenge pointer to the challenge data
 * 	challenge_length length of the challenge data
 * 	digest    pointer to the resulting digest buffer (must be 16 bytes)
 *
 * Return value:
 * 	None
 */
void md5_cram_get(const unsigned char *secret, const unsigned char *challenge,
                  int challenge_length, unsigned char *digest)
{
	int i;
	unsigned char secret_md5[16];
	int secret_length = strlen(secret);
	unsigned char osecret[64];
	unsigned char isecret[64];
	s_md5 md;
	
	/*
	 * If the secret length is longer 64 bytes
	 * use MD5 digest of the secret as input
	 */
	if( secret_length > 64 )
	{
		md5_get(secret, secret_length, secret_md5);
		secret = secret_md5;
		secret_length = 16;
	}

	memcpy(osecret, secret, secret_length);
	memcpy(isecret, secret, secret_length);
	memset(osecret + secret_length, '\0', 64 - secret_length);
	memset(isecret + secret_length, '\0', 64 - secret_length);
	
	for( i = 0; i < 64; i++ )
	{
		osecret[i] ^= 0x5c;
		isecret[i] ^= 0x36;
	}

	/*
	 * Compute HASH((secret XOR ipad), challenge)
	 */
	md5_init(&md);
	md5_update(&md, isecret);
	md5_compute(&md, challenge, challenge_length, secret_md5);

	/*
	 * Compute HASH((secret XOR opad), secret_md5)
	 */
	md5_init(&md);
	md5_update(&md, osecret);
	md5_compute(&md, secret_md5, 16, digest);
}

