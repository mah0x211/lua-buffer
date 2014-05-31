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
// lualib
#include <lauxlib.h>
#include <lualib.h>


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
    // buffer
    size_t unit;
    size_t nmax;
    size_t nalloc;
    size_t used;
    size_t total;
    void *mem;
} buf_t;


#define MODULE_MT   "buffer"


#define getudata(L) ({ \
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


static int raw_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    
    lua_pushlightuserdata( L, b->mem );
    lua_pushinteger( L, (lua_Integer)b->used );
    
    return 2;
}


static int byte_lua( lua_State *L )
{
    buf_t *b = getudata( L );
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
    buf_t *b = getudata( L );
    
    lua_pushinteger( L, (lua_Integer)b->total );
    
    return 1;
}


#define upperlower_lua(L,range,op) ({ \
    int rc = 2; \
    buf_t *b = getudata( L ); \
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


static int set_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    size_t len = 0;
    const char *str = luaL_checklstring( L, 2, &len );
    
    if( len == 0 ){
        buf_term( b, len );
        return 0;
    }
    else if( buf_increase( b, 0, len + 1 ) == 0 ){
        memcpy( b->mem, str, len );
        buf_term( b, len );
        return 0;
    }
    
    // got error
    lua_pushinteger( L, errno );
    
    return 1;
}


static int add_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    int argc = lua_gettop( L );
    
    if( argc > 1 )
    {
        size_t len = 0;
        const char *str = NULL;
        
        lua_concat( L, argc - 1 );
        str = lua_tolstring( L, 2, &len );
        if( len )
        {
            if( buf_increase( b, b->used, len + 1 ) == 0 ){
                memcpy( b->mem + b->used, str, len );
                buf_term( b, b->used + len );
                return 0;
            }
            
            // got error
            lua_pushinteger( L, errno );
            return 1;
        }
    }
    
    return 0;
}


static int insert_lua( lua_State *L )
{
    buf_t *b = getudata( L );
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
    buf_t *b = getudata( L );
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
    buf_t *b = getudata( L );
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



static int read_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    int fd = luaL_checkint( L, 2 );
    size_t bytes = b->unit;
    ssize_t len = 0;
    
    // check arguments
    if( fd < 0 ){
        return luaL_argerror( L, 2, "fd must be larger than 0" );
    }
    else if( !lua_isnoneornil( L, 3 ) )
    {
        lua_Integer lbytes = luaL_checkinteger( L, 3 );
        if( lbytes < 1 ){
            return luaL_argerror( L, 3, "bytes must be larger than 0" );
        }
        bytes = (size_t)lbytes;
    }
    
    if( buf_increase( b, b->used, bytes + 1 ) != 0 ){
        len = -1;
    }
    else if( ( len = read( fd, b->mem + b->used, bytes ) ) > 0 ){
        buf_term( b, b->used + (size_t)len );
    }
    
    // set number of bytes read
    lua_pushinteger( L, (lua_Integer)len );
    // got error
    if( len == -1 ){
        lua_pushinteger( L, errno );
        lua_pushboolean( L, errno == EAGAIN || errno == EWOULDBLOCK );
        return 3;
    }
    
    return 1;
}


static int write_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    int fd = luaL_checkint( L, 2 );
    lua_Integer pos = luaL_checkinteger( L, 3 );
    size_t bytes = b->used;
    ssize_t len = 0;
    
    // check arguments
    if( fd < 0 ){
        return luaL_argerror( L, 2, "fd must be larger than 0" );
    }
    else if( pos <= 0 || pos >= (lua_Integer)b->used ){
        return luaL_argerror( L, 3, "pos must be larger than 0 and less than the used size" );
    }
    else {
        pos--;
        bytes -= (size_t)pos;
    }
    
    len = write( fd, b->mem + (size_t)pos, bytes );
    // set number of bytes write
    lua_pushinteger( L, (lua_Integer)len );
    if( len == -1 ){
        lua_pushinteger( L, errno );
        lua_pushboolean( L, errno == EAGAIN || errno == EWOULDBLOCK );
        return 3;
    }
    
    return 1;
}


static int free_lua( lua_State *L )
{
    buf_t *b = (buf_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    if( b->mem ){
        pdealloc( b->mem );
        b->mem = NULL;
        b->used = b->total = b->nalloc = 0;
    }
    
    return 0;
}


static int dealloc_gc( lua_State *L )
{
    buf_t *b = (buf_t*)lua_touserdata( L, 1 );
    
    if( b->mem ){
        pdealloc( b->mem );
    }
    
    return 0;
}


static int tostring_lua( lua_State *L )
{
    buf_t *b = luaL_checkudata( L, 1, MODULE_MT );
    
    lua_pushlstring( L, b->mem, (size_t)b->used );
    
    return 1;
}


static int len_lua( lua_State *L )
{
    buf_t *b = luaL_checkudata( L, 1, MODULE_MT );
    
    lua_pushinteger( L, (lua_Integer)b->used );
    
    return 1;
}


static int alloc_lua( lua_State *L )
{
    lua_Integer lunit = luaL_checkinteger( L, 1 );
    buf_t *b = NULL;
    
    // check arguments
    if( lunit < 1 ){
        return luaL_argerror( L, 1, "size must be larger than 0" );
    }
    else if( ( b = lua_newuserdata( L, sizeof( buf_t ) ) ) )
    {
        size_t unit = (size_t)lunit;
        
        if( ( b->mem = pnalloc( unit, char ) ) ){
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


// metanames
// module definition register
static void define_mt( lua_State *L, struct luaL_Reg mmethod[], 
                       struct luaL_Reg method[] )
{
    int i = 0;
    
    // create table __metatable
    luaL_newmetatable( L, MODULE_MT );
    // metamethods
    while( mmethod[i].name ){
        lstate_fn2tbl( L, mmethod[i].name, mmethod[i].func );
        i++;
    }
    // methods
    lua_pushstring( L, "__index" );
    lua_newtable( L );
    i = 0;
    while( method[i].name ){
        lstate_fn2tbl( L, method[i].name, method[i].func );
        i++;
    }
    lua_rawset( L, -3 );
    lua_pop( L, 1 );
}


LUALIB_API int luaopen_buffer( lua_State *L )
{
    struct luaL_Reg mmethod[] = {
        { "__gc", dealloc_gc },
        { "__tostring", tostring_lua },
        { "__len", len_lua },
        { NULL, NULL }
    };
    struct luaL_Reg method[] = {
        // method
        { "raw", raw_lua },
        { "byte", byte_lua },
        { "total", total_lua },
        { "lower", lower_lua },
        { "upper", upper_lua },
        { "set", set_lua },
        { "add", add_lua },
        { "insert", insert_lua },
        { "sub", sub_lua },
        { "substr", substr_lua },
        { "read", read_lua },
        { "write", write_lua },
        { "free", free_lua },
        { NULL, NULL }
    };
    
    define_mt( L, mmethod, method );
    // option method
    lua_pushcfunction( L, alloc_lua );
    
    return 1;
}


