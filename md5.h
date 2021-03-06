/*
  Copyright (C) 1999, 2000, 2002 Aladdin Enterprises.  All rights reserved.

 Portions modified by Coverity, Inc.
 (c) 2008-2009,2011-2012 Coverity, Inc. All rights reserved worldwide.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

*/
#ifndef MD5_C_HEADER
#define MD5_C_HEADER

#include "libs/macros.hpp" // BEGIN_C_INTERFACE

BEGIN_C_INTERFACE;

typedef unsigned char md5_byte_t; /* 8-bit byte */
typedef unsigned int md5_word_t; /* 32-bit word */

/* Define the state of the MD5 Algorithm. */
typedef struct md5_state_s {
    md5_word_t count[2];        /* message length in bits, lsw first */
    md5_word_t abcd[4];         /* digest buffer */
    md5_byte_t buf[64];         /* accumulate block */
} md5_state_t;

// Raw underlying routines.
void md5_init(md5_state_t *pms);
void md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes);
void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);

END_C_INTERFACE;

#endif
