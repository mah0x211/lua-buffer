/*
 *  Copyright (C) 2014 Masatoshi Teruya
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 *
 *  buffer.c
 *  lua-buffer
 *
 *  Created by Masatoshi Teruya on 14/04/16.
 *
 */


#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/uio.h>
// lua
#include <lua.h>
#include <lauxlib.h>
#include "hexcodec.h"
#include "base64mix.h"


// memory alloc/dealloc
#define palloc(t)       (t*)malloc( sizeof(t) )
#define pnalloc(n,t)    (t*)malloc( (n) * sizeof(t) )
#define pcalloc(n,t)    (t*)calloc( n, sizeof(t) )
#define prealloc(n,t,p) (t*)realloc( p, (n) * sizeof(t) )
#define pdealloc(p)     free((void*)p)

// helper macros for lua_State
#define lstate_ref(L,idx) \
    (lua_pushvalue(L,idx),luaL_ref( L, LUA_REGISTRYINDEX ))

#define lstate_pushref(L,ref) \
    lua_rawgeti( L, LUA_REGISTRYINDEX, ref )

#define lstate_unref(L,ref) \
    luaL_unref( L, LUA_REGISTRYINDEX, ref )

#define lstate_fn2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushcfunction(L,v); \
    lua_rawset(L,-3); \
}while(0)

#define lstate_str2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushstring(L,v); \
    lua_rawset(L,-3); \
}while(0)

#define lstate_num2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushnumber(L,v); \
    lua_rawset(L,-3); \
}while(0)


// do not touch directly
typedef struct {
    int fd;
    int cloexec;
    size_t cur;
    // buffer
    size_t unit;
    size_t nmax;
    size_t nalloc;
    size_t used;
    size_t total;
    void *mem;
} buf_t;


#define MODULE_MT   "buffer"


#define checkudata(L) ({ \
    buf_t *_buf = (buf_t*)luaL_checkudata( L, 1, MODULE_MT ); \
    if( !_buf->mem ){ \
        return luaL_error( L, "attempted to access already freed memory" ); \
    } \
    _buf; \
})


static inline int buf_alloc( buf_t *b, size_t nalloc )
{
    if( nalloc > b->nmax ){
        errno = ENOMEM;
        return -1;
    }
    else if( nalloc > b->nalloc )
    {
        size_t total = nalloc * b->unit;
        void *buf = realloc( b->mem, total );
        
        if( !buf ){
            return -1;
        }
        b->nalloc = nalloc;
        b->total = total;
        b->mem = buf;
    }
    
    return 0;
}


static inline int buf_increase( buf_t *b, size_t from, size_t bytes )
{
    if( from > b->used ){
        errno = EINVAL;
        return -1;
    }
    // remain < bytes
    else if( ( b->total - from ) < bytes )
    {
        bytes -= ( b->total - from );
        
        if( bytes < b->unit ){
            return buf_alloc( b, b->nalloc + 1 );
        }
        
        return buf_alloc( b, b->nalloc + 
            ( bytes / b->unit + ( bytes % b->unit ? 1 : 0 ) ) 
        );
    }
    
    return 0;
}


static inline void buf_term( buf_t *b, size_t pos )
{
    b->used = pos;
    ((char*)b->mem)[b->used] = 0;
}


static inline ssize_t buf_read( buf_t *b, size_t pos, size_t bytes )
{
    ssize_t len = 0;
    
    // check arguments
    if( buf_increase( b, pos, bytes + 1 ) != 0 ){
        len = -1;
    }
    else if( ( len = read( b->fd, b->mem + pos, bytes ) ) > 0 ){
        buf_term( b, pos + (size_t)len );
    }
    
    return len;
}


static int raw_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    
    lua_pushlightuserdata( L, b->mem );
    lua_pushinteger( L, (lua_Integer)b->used );
    
    return 2;
}


static int byte_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    lua_Integer head = 1;
    lua_Integer tail = 1;
    lua_Integer ret = 0;
    
    // check arguments
    if( !lua_isnoneornil( L, 2 ) )
    {
        head = luaL_checkinteger( L, 2 );
        if( head < 1 || (size_t)head > b->used ){
            lua_pushnil( L );
            return 1;
        }
    }
    if( !lua_isnoneornil( L, 3 ) )
    {
        tail = luaL_checkinteger( L, 3 );
        // ignore negative index
        if( tail < 0 ){
            tail = head;
        }
        else if( tail < head ){
            lua_pushnil( L );
            return 1;
        }
        else if( (size_t)tail > b->used ){
            tail = (lua_Integer)b->used;
        }
    }
    else {
        tail = head;
    }
    
    lua_settop( L, 0 );
    head--;
    ret = tail - head;
    for(; head < tail; head++ ){
        lua_pushinteger( L, ((unsigned char*)b->mem)[head] );
    }
    
    return (int)ret;
}


static int total_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    
    lua_pushinteger( L, (lua_Integer)b->total );
    
    return 1;
}


#define upperlower_lua(L,range,op) ({ \
    int rc = 2; \
    buf_t *b = checkudata( L ); \
    unsigned char *lmem = pnalloc( (size_t)b->used, unsigned char ); \
    if( lmem ) { \
        unsigned char *ptr = (unsigned char*)b->mem; \
        size_t i = 0; \
        for(; i < b->used; i++ ) { \
            switch( ptr[i] ){ \
                case range: \
                    lmem[i] = ptr[i] op 0x20; \
                break; \
                default: \
                    lmem[i] = ptr[i]; \
            } \
        } \
        lua_pushlstring( L, (const char*)lmem, (size_t)b->used ); \
        rc = 1; \
    } \
    else { \
        lua_pushnil( L ); \
        lua_pushinteger( L, errno ); \
    } \
    rc; \
})

// A-Z + 0x20
static int lower_lua( lua_State *L )
{
    return upperlower_lua( L, 'A' ... 'Z', + );
}

// a-z - 0x20
static int upper_lua( lua_State *L )
{
    return upperlower_lua( L, 'a' ... 'z', - );
}


static int hex_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    size_t len = b->used * 2;
    char *enc = malloc( len );
    
    if( enc ){
        hex_encode( (unsigned char*)enc, (unsigned char*)b->mem, b->used );
        lua_pushlstring( L, enc, len );
        return 1;
    }
    
    // nomem error
    lua_pushnil( L );
    lua_pushinteger( L, errno );
    
    return 2;
}


#define base64_lua( L, fn ) ({ \
    buf_t *b = checkudata( L ); \
    size_t len = b->used; \
    char *enc = fn( (unsigned char*)b->mem, &len ); \
    int rc = 1; \
    if( !enc ){ \
        lua_pushnil( L ); \
        lua_pushinteger( L, errno ); \
        rc = 2; \
    } \
    lua_pushlstring( L, enc, len ); \
    rc; \
})

// base64 standard encoding
static int base64std_lua( lua_State *L )
{
    return base64_lua( L, b64m_encode_std );
}

// base64 url encoding
static int base64url_lua( lua_State *L )
{
    return base64_lua( L, b64m_encode_url );
}


static inline int buf_set( buf_t *b, size_t pos, const char *str, size_t len )
{
    int rc = 0;
    
    if( len > 0 )
    {
        rc = buf_increase( b, pos, len + 1 );
        if( rc == 0 ){
            memcpy( b->mem + pos, str, len );
            buf_term( b, pos + len );
        }
    }
    else {
        buf_term( b, pos );
    }
    
    return rc;
}


static int set_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    size_t len = 0;
    const char *str = luaL_checklstring( L, 2, &len );
    
    if( buf_set( b, 0, str, len ) == 0 ){
        b->cur = 0;
        return 0;
    }
    
    // got error
    lua_pushinteger( L, errno );
    
    return 1;
}


static int add_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    int argc = lua_gettop( L );
    
    if( argc > 1 )
    {
        size_t len = 0;
        const char *str = NULL;
        
        lua_concat( L, argc - 1 );
        str = lua_tolstring( L, 2, &len );
        if( buf_set( b, b->used, str, len ) != 0 ){
            // got error
            lua_pushinteger( L, errno );
            return 1;
        }
    }
    
    return 0;
}


static int insert_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    lua_Integer idx = luaL_checkinteger( L, 2 );
    size_t len = 0;
    const char *str = luaL_checklstring( L, 3, &len );
    
    // check arguments
    // check index
    if( len == 0 || idx > (lua_Integer)b->used ){
        return 0;
    }
    else if( idx > 0 ){
        idx--;
    }
    else if( idx < 0 )
    {
        if( ( idx + (lua_Integer)b->used ) < 0 ){
            idx = 0;
        }
        else {
            idx += b->used;
        }
    }
    
    if( buf_increase( b, b->used, len + 1 ) == 0 ){
        memmove( b->mem + (size_t)idx + len, b->mem + (size_t)idx, 
                 b->used - (size_t)idx + 1 );
        memcpy( b->mem + idx, str, len );
        buf_term( b, b->used + len );
        return 0;
    }
    
    // got error
    lua_pushinteger( L, errno );
    
    return 1;
}


static int sub_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    lua_Integer lhead = luaL_checkinteger( L, 2 );
    size_t head = 0;
    size_t tail = b->used;
    
    // check arguments
    // head
    if( lhead >= (lua_Integer)b->used ){
        goto EMPTY_STRING;
    }
    else if( lhead > 0 ){
        head = (size_t)lhead - 1;
    }
    else if( lhead < 0 && ( lhead + (lua_Integer)b->used ) > 0 ){
        head = (size_t)( lhead + (lua_Integer)b->used );
    }
    // tail
    if( !lua_isnoneornil( L, 3 ) )
    {
        lua_Integer ltail = luaL_checkinteger( L, 3 );
        
        if( ltail < 0 )
        {
            if( ( ltail + (lua_Integer)b->used ) > 0 ){
                tail = (size_t)( ltail + (lua_Integer)b->used + 1 );
            }
        }
        else if( ltail <= (lua_Integer)b->used ){
            tail = (size_t)ltail;
        }
        
        if( head >= tail ){
            goto EMPTY_STRING;
        }
    }
    
    lua_pushlstring( L, b->mem + head, tail - head );
    return 1;
    
EMPTY_STRING:
    lua_pushstring( L, "" );
    return 1;
}


static int substr_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    lua_Integer lhead = luaL_checkinteger( L, 2 );
    size_t head = 0;
    size_t tail = b->used;
    
    // check arguments
    // check index
    if( lhead > (lua_Integer)b->used ){
        goto EMPTY_STRING;
    }
    else if( lhead > 0 ){
        head = (size_t)lhead - 1;
    }
    else if( lhead < 0 && ( lhead + (lua_Integer)b->used ) > 0 ){
        head = (size_t)( lhead + (lua_Integer)b->used );
    }
    // length
    if( !lua_isnoneornil( L, 3 ) )
    {
        lua_Integer ltail = luaL_checkinteger( L, 3 );
        
        if( ltail < 1 ){
            goto EMPTY_STRING;
        }
        else if( (lua_Integer)b->used - (lua_Integer)head - ltail > 0 ){
            tail = (size_t)( ltail + (lua_Integer)head );
        }
    }
    
    lua_pushlstring( L, b->mem + head, (size_t)(tail - head) );
    return 1;
    
EMPTY_STRING:
    lua_pushstring( L, "" );
    
    return 1;
}


static int setfd_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    int fd = luaL_checkint( L, 2 );
    
    // check arguments
    if( fd < 0 ){
        return luaL_argerror( L, 2, "fd must be larger than 0" );
    }
    // cloexec
    else if( lua_gettop( L ) > 2 ){
        luaL_checktype( L, 3, LUA_TBOOLEAN );
        b->cloexec = lua_toboolean( L, 3 );
    }
    b->fd = fd;
    
    return 0;
}


static int cloexec_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    
    luaL_checktype( L, 2, LUA_TBOOLEAN );
    b->cloexec = lua_toboolean( L, 2 );
    
    return 0;
}


static inline int read2buf( lua_State *L, buf_t *b, size_t pos )
{
    size_t bytes = b->unit;
    ssize_t len = 0;
    lua_Integer rbytes = luaL_optinteger( L, 2, 0 );

    // check arguments
    // arg#2 bytes
    if( rbytes < 0 ){
        return luaL_argerror( L, 2, "bytes must be larger than 0" );
    }
    else if( rbytes ){
        bytes = (size_t)rbytes;
    }
    
    len = buf_read( b, pos, bytes );
    // set number of bytes read
    lua_pushinteger( L, (lua_Integer)len );
    // got error
    if( len == -1 ){
        lua_pushinteger( L, errno );
        lua_pushboolean( L, errno == EAGAIN || errno == EWOULDBLOCK );
        return 3;
    }
    // rewind the write cursor
    else if( pos == 0 ){
        b->cur = 0;
    }
    
    return 1;
}


static int read_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    return read2buf( L, b, 0 );
}


static int readadd_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    return read2buf( L, b, b->used );
}


static int write_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    struct iovec iov;
    
    iov.iov_base = (void*)luaL_checklstring( L, 2, &iov.iov_len );
    if( iov.iov_base )
    {
        ssize_t len = writev( b->fd, &iov, 1 );
        
        // set number of bytes read
        lua_pushinteger( L, (lua_Integer)len );
        if( len == -1 ){
            lua_pushinteger( L, errno );
            lua_pushboolean( L, errno == EAGAIN || errno == EWOULDBLOCK );
            return 3;
        }
    }
    else {
        lua_pushinteger( L, 0 );
    }
    
    return 1;
}


static int flush_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    ssize_t len = 0;
    struct iovec iov;
    
    if( b->cur > b->used ){
        b->cur = 0;
    }
    iov.iov_base = b->mem + b->cur;
    iov.iov_len = b->used - b->cur;
    
    len = writev( b->fd, &iov, 1 );
    if( len == -1 ){
        lua_pushinteger( L, (lua_Integer)len );
        lua_pushinteger( L, (lua_Integer)b->used );
        lua_pushinteger( L, errno );
        lua_pushboolean( L, errno == EAGAIN || errno == EWOULDBLOCK );
        return 4;
    }
    else
    {
        b->cur += (size_t)len;
        // set number of bytes write
        lua_pushinteger( L, (lua_Integer)b->cur );
        // set total number of bytes buffer
        lua_pushinteger( L, (lua_Integer)b->used );
        // reset buffer
        if( b->cur == b->used ){
            b->cur = 0;
            buf_term( b, 0 );
        }
    }
    
    return 2;
}


static int free_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    
    if( b->mem )
    {
        pdealloc( b->mem );
        b->mem = NULL;
        b->used = b->total = b->nalloc = 0;
        if( b->cloexec && b->fd != -1 ){
            close( b->fd );
        }
    }
    
    return 0;
}


static int gc_lua( lua_State *L )
{
    buf_t *b = (buf_t*)lua_touserdata( L, 1 );
    
    if( b->mem )
    {
        pdealloc( b->mem );
        if( b->cloexec && b->fd != -1 ){
            close( b->fd );
        }
    }
    
    return 0;
}


static int tostring_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    
    lua_pushlstring( L, b->mem, (size_t)b->used );
    
    return 1;
}


static int len_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    
    lua_pushinteger( L, (lua_Integer)b->used );
    
    return 1;
}


static int eq_lua( lua_State *L )
{
    buf_t *b = checkudata( L );
    size_t len = 0;
    const char *str = NULL;
    
    switch( lua_type( L, 2 ) ){
        case LUA_TSTRING:
            str = lua_tolstring( L, 2, &len );
        break;
        case LUA_TUSERDATA:
            if( lua_getmetatable( L, 2 ) )
            {
                lua_pop( L, 1 );
                if( luaL_callmeta( L, 2, "__tostring" ) ){
                    str = lua_tolstring( L, -1, &len );
                }
            }
        break;
    }
    
    lua_pushboolean( L, str && len == b->used && 
                     memcmp( str, b->mem, b->used ) == 0 );
    return 1;
}


static int alloc_lua( lua_State *L )
{
    lua_Integer lunit = luaL_checkinteger( L, 1 );
    buf_t *b = NULL;
    int fd = -1;
    int cloexec = 0;
    
    // check arguments
    // arg#1:unit
    if( lunit < 1 ){
        return luaL_argerror( L, 1, "size must be larger than 0" );
    }
    // arg#2:fd for read/write
    else if( !lua_isnoneornil( L, 2 ) )
    {
        fd = luaL_checkint( L, 2 );
        if( fd < 0 ){
            return luaL_argerror( L, 2, "fd must be larger than 0" );
        }
    }
    // arg#3:cloexec
    else if( !lua_isnoneornil( L, 3 ) ){
        luaL_checktype( L, 3, LUA_TBOOLEAN );
        cloexec = lua_toboolean( L, 3 );
    }

    if( ( b = lua_newuserdata( L, sizeof( buf_t ) ) ) )
    {
        size_t unit = (size_t)lunit;
        
        if( ( b->mem = pnalloc( unit, char ) ) ){
            b->fd = fd;
            b->cloexec = cloexec;
            b->cur = 0;
            b->total = b->unit = unit;
            b->nalloc = 1;
            b->nmax = SIZE_MAX / unit;
            buf_term( b, 0 );
            // set metatable
            luaL_getmetatable( L, MODULE_MT );
            lua_setmetatable( L, -2 );
            return 1;
        }
    }
    
    // got error
    lua_pushnil( L );
    lua_pushinteger( L, errno );
    
    return 2;
}


LUALIB_API int luaopen_buffer( lua_State *L )
{
    struct luaL_Reg mmethod[] = {
        { "__gc", gc_lua },
        { "__tostring", tostring_lua },
        { "__len", len_lua },
        { "__eq", eq_lua },
        { NULL, NULL }
    };
    struct luaL_Reg method[] = {
        // method
        { "raw", raw_lua },
        { "byte", byte_lua },
        { "total", total_lua },
        { "lower", lower_lua },
        { "upper", upper_lua },
        { "hex", hex_lua },
        { "base64", base64std_lua },
        { "base64url", base64url_lua },
        { "set", set_lua },
        { "add", add_lua },
        { "insert", insert_lua },
        { "sub", sub_lua },
        { "substr", substr_lua },
        { "setfd", setfd_lua },
        { "cloexec", cloexec_lua },
        { "read", read_lua },
        { "readadd", readadd_lua },
        { "write", write_lua },
        { "flush", flush_lua },
        { "free", free_lua },
        { NULL, NULL }
    };
    struct luaL_Reg *ptr = mmethod;
    
    // create table __metatable
    luaL_newmetatable( L, MODULE_MT );
    // metamethods
    while( ptr->name ){
        lstate_fn2tbl( L, ptr->name, ptr->func );
        ptr++;
    }
    // methods
    lua_pushstring( L, "__index" );
    lua_newtable( L );
    ptr = method;
    while( ptr->name ){
        lstate_fn2tbl( L, ptr->name, ptr->func );
        ptr++;
    }
    lua_rawset( L, -3 );
    lua_pop( L, 1 );
    
    // add alloc function
    lua_pushcfunction( L, alloc_lua );
    
    return 1;
}


