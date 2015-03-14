/*
 *  base64mix.h
 *  Created by Masatoshi Teruya on 14/10/23.
 *
 *  Copyright 2014 Masatoshi Teruya. All rights reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a 
 *  copy of this software and associated documentation files (the "Software"), 
 *  to deal in the Software without restriction, including without limitation 
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 *  and/or sell copies of the Software, and to permit persons to whom the 
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL 
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 *  DEALINGS IN THE SOFTWARE.
 *
 */
 
#ifndef ___BASE64MIX_H___
#define ___BASE64MIX_H___

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

static const unsigned char BASE64MIX_STDENC[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
    '+', '/'
};

static const unsigned char BASE64MIX_URLENC[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
    '-', '_'
};


static inline char *b64m_encode( const unsigned char *src, size_t *len, 
                                 const unsigned char enctbl[] )
{
    unsigned char *res = NULL;
    size_t tail = *len;
    size_t bytes = ( 8.0 / 6.0 * (double)tail);
    size_t surplus = bytes % 4;
    
    // add padding bytes
    if( surplus ){
        bytes += 4 - surplus;
    }
    // no-space for null-term or wrap around
    if( bytes == SIZE_MAX || bytes < tail ){
        errno = ERANGE;
        return NULL;
    }
    
    if( ( res = malloc( bytes + 1 ) ) )
    {
        const unsigned char *cur = src;
        unsigned char *ptr = res;
        uint8_t c = -1;
        uint8_t state = 0;
        size_t i = 0;
        
        for(; i < tail; i++ )
        {
            switch( state ){
                case 0:
                    c = ( *cur >> 2 ) & 0x3f;
                    *ptr++ = enctbl[c];
                    c = ( *cur & 0x3 ) << 4;
                    state = 1;
                break;
                case 1:
                    c |= ( *cur >> 4 ) & 0xf;
                    *ptr++ = enctbl[c];
                    c = ( *cur & 0xf ) << 2;
                    state = 2;
                break;
                case 2:
                    c |= ( *cur >> 6 ) & 0x3;
                    *ptr++ = enctbl[c];
                    c = *cur & 0x3f;
                    *ptr++ = enctbl[c];
                    c = -1;
                    state = 0;
                break;
            }
            cur++;
        }
        
        // append last bit
        if( c != (uint8_t)-1 ){
            *ptr++ = enctbl[c];
        }
        // append padding if standard base64
        if( enctbl == BASE64MIX_STDENC )
        {
            for( i = ptr - res; i < bytes; i++ ){
                *ptr++ = '=';
            }
        }
        // set result length
        *len = ptr - res;
        *ptr = 0;
    }
    
    return (char*)res;
}
#define b64m_encode_std(src,len)   b64m_encode(src,len,BASE64MIX_STDENC)
#define b64m_encode_url(src,len)   b64m_encode(src,len,BASE64MIX_URLENC)



static const unsigned char BASE64MIX_STDDEC[255] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    
//  SP   !   "   #   $   %   &   '   (    )    *   +   ,    -    .   /
    -1, -1, -1, -1, -1, -1, -1, -1, -1,  -1,  -1, 62, -1,  -1,  -1, 63, 
//   0   1   2   3   4   5   6   7   8   9
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 
//   :   ;   <   =   >   ?   @
    -1, -1, -1, -1, -1, -1, -1,
//  A  B  C  D  E  F  G  H  I  J   K   L   M   N   O   P   Q   R   S   T   U
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
//   V   W   X   Y   Z
    21, 22, 23, 24, 25,
//   [   \   ]   ^   _   `
    -1, -1, -1, -1, -1, -1,
//   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o   p   q   r   s
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 
//   t   u   v   w   x   y   z
    45, 46, 47, 48, 49, 50, 51,
//   {   |   }   ~ 
    -1, -1, -1, -1,

    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const unsigned char BASE64MIX_URLDEC[255] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    
//  SP   !   "   #   $   %   &   '   (    )    *   +   ,    -    .   /
    -1, -1, -1, -1, -1, -1, -1, -1, -1,  -1,  -1, -1, -1,  62,  -1, -1, 
//   0   1   2   3   4   5   6   7   8   9
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 
//   :   ;   <   =   >   ?   @
    -1, -1, -1, -1, -1, -1, -1,
//  A  B  C  D  E  F  G  H  I  J   K   L   M   N   O   P   Q   R   S   T   U
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
//   V   W   X   Y   Z
    21, 22, 23, 24, 25,
//   [   \   ]   ^   _   `
    -1, -1, -1, -1, 63, -1,
//   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o   p   q   r   s
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 
//   t   u   v   w   x   y   z
    45, 46, 47, 48, 49, 50, 51,
//   {   |   }   ~ 
    -1, -1, -1, -1,

    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const unsigned char BASE64MIX_DEC[255] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    
//  SP   !   "   #   $   %   &   '   (    )    *   +   ,    -    .   /
    -1, -1, -1, -1, -1, -1, -1, -1, -1,  -1,  -1, 62, -1,  62,  -1, 63, 
//   0   1   2   3   4   5   6   7   8   9
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 
//   :   ;   <   =   >   ?   @
    -1, -1, -1, -1, -1, -1, -1,
//  A  B  C  D  E  F  G  H  I  J   K   L   M   N   O   P   Q   R   S   T   U
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
//   V   W   X   Y   Z
    21, 22, 23, 24, 25,
//   [   \   ]   ^   _   `
    -1, -1, -1, -1, 63, -1,
//   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o   p   q   r   s
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 
//   t   u   v   w   x   y   z
    45, 46, 47, 48, 49, 50, 51,
//   {   |   }   ~ 
    -1, -1, -1, -1,
    
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static inline char *b64m_decode( const unsigned char *src, size_t *len, 
                                 const unsigned char dectbl[] )
{
    unsigned char *res = NULL;
    size_t bytes = ((double)*len / ( 8.0 / 6.0 ));
    
    if( ( res = malloc( bytes + 1 ) ) )
    {
        const unsigned char *cur = src;
        unsigned char *ptr = res;
        uint8_t c = 0;
        uint32_t bit24 = 1;
        size_t tail = *len;
        size_t i = 0;
        
        for(; i < tail; i++ )
        {
            // ignore padding
            if( *cur == '=' )
            {
                // check remaining characters
                while( *(++cur) )
                {
                    // remaining characters must be '='
                    if( *cur != '=' ){
                        free( (void*)res );
                        errno = EINVAL;
                        return NULL;
                    }
                }
                break;
            }
            // invalid character
            else if( ( c = dectbl[*cur] ) > 63 ){
                free( (void*)res );
                errno = EINVAL;
                return NULL;
            }
            bit24 = bit24 << 6 | c;
            if( bit24 & 0x1000000 ){
                *ptr++ = bit24 >> 16;
                *ptr++ = bit24 >> 8;
                *ptr++ = bit24;
                bit24 = 1;
            }
            cur++;
        }
        
        if( bit24 & 0x40000 ){
            *ptr++ = bit24 >> 10;
            *ptr++ = bit24 >> 2;
        }
        else if( bit24 & 0x1000 ){
            *ptr++ = bit24 >> 4;
        }
        *ptr = 0;
        // set result length
        *len = ptr - res;
    }
    
    return (char*)res;
}

#define b64m_decode_std(src,len)   b64m_decode(src,len,BASE64MIX_STDDEC)
#define b64m_decode_url(src,len)   b64m_decode(src,len,BASE64MIX_URLDEC)
#define b64m_decode_mix(src,len)   b64m_decode(src,len,BASE64MIX_DEC)

#endif
