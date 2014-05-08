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
    int64_t unit;
    int64_t used;
    int64_t total;
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


static inline int buf_increase( buf_t *b, int64_t bytes )
{
    if( bytes > b->total )
    {
        int64_t mod = bytes % b->unit;
        void *buf = realloc( b->mem, (size_t)(( mod ) ? bytes - mod + b->unit : bytes) );
        
        if( !buf ){
            return -1;
        }
        b->mem = buf;
        b->total = bytes;
    }
    
    return 0;
}


static int buf_shift( buf_t *b, int64_t from, int64_t idx )
{
    int64_t len = b->used - from;
    
    // right shift
    if( from < idx )
    {
        if( buf_increase( b, idx + len + 1 ) != 0 ){
            return -1;
        }
        memmove( b->mem + idx, b->mem + from, len );
    }
    // left shift
    else if( from > idx ){
        memcpy( b->mem + idx, b->mem + from, len );
    }
    else {
        return 0;
    }
    
    b->used = idx + len;
    ((char*)b->mem)[b->used] = 0;
    
    return 0;
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
        int64_t i = 0; \
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
        return 0;
    }
    else if( buf_increase( b, (int64_t)len + 1 ) == 0 ){
        memcpy( b->mem, str, len );
        ((char*)b->mem)[len] = 0;
        b->used = (int64_t)len;
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
            if( buf_increase( b, b->used + (int64_t)len + 1 ) == 0 ){
                memcpy( b->mem + b->used, str, len );
                b->used += len;
                ((char*)b->mem)[b->used] = 0;
            }
            // got error
            else {
                lua_pushinteger( L, errno );
                return 1;
            }
        }
    }
    
    return 0;
}


static int insert_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    int64_t idx = luaL_checkinteger( L, 2 );
    size_t len = 0;
    const char *str = luaL_checklstring( L, 3, &len );
    
    // check arguments
    // check index
    if( idx > b->used ){
        return 0;
    }
    else if( idx < 0 )
    {
        if( ( idx + b->used ) < 0 ){
            idx = 0;
        }
        else {
            idx += b->used;
        }
    }
    
    if( buf_shift( b, idx, idx + (int64_t)len ) == 0 ){
        memcpy( b->mem + idx, str, len );
        return 0;
    }
    
    // got error
    lua_pushinteger( L, errno );
    
    return 1;
}


static int read_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    int fd = luaL_checkint( L, 2 );
    lua_Integer bytes = luaL_checkinteger( L, 3 );
    int64_t incr = bytes - ( b->total - b->used );
    ssize_t len;
    
    // check arguments
    if( fd < 0 ){
        return luaL_argerror( L, 2, "fd must be larger than 0" );
    }
    else if( bytes < 1 ){
        return luaL_argerror( L, 3, "bytes must be larger than 0" );
    }
    else if( incr > 0 && buf_increase( b, b->total + incr ) != 0 ){
        lua_pushinteger( L, -1 );
        lua_pushinteger( L, errno );
        return 2;
    }
    
    len = read( fd, b->mem + b->used, (size_t)bytes );
    lua_pushinteger( L, len );
    if( len > 0 ){
        b->used += len;
        ((char*)b->mem)[b->used] = 0;
    }
    else if( len == -1 ){
        lua_pushinteger( L, errno );
        return 2;
    }
    
    return 1;
}


static int sub_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    int64_t head = luaL_checkinteger( L, 2 );
    int64_t tail = b->used;
    
    // check arguments
    // head
    if( head >= b->used ){
        goto EMPTY_STRING;
    }
    else if( head < 0 )
    {
        if( ( head + b->used ) < 0 ){
            head = 0;
        }
        else {
            head += b->used;
        }
    }
    // tail
    if( !lua_isnoneornil( L, 3 ) )
    {
        tail = luaL_checkinteger( L, 3 );
        if( tail > b->used ){
            tail = b->used;
        }
        else if( tail < 0 )
        {
            if( ( tail + b->used ) < 0 ){
                tail = 0;
            }
            else {
                tail += b->used + 1;
            }
        }
        
        if( head >= tail ){
            goto EMPTY_STRING;
        }
    }
    
    lua_pushlstring( L, b->mem + head, (size_t)(tail - head) );
    return 1;
    
EMPTY_STRING:
    lua_pushstring( L, "" );
    return 1;
}


static int substr_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    int64_t head = luaL_checkinteger( L, 2 );
    int64_t tail = b->used;
    
    // check arguments
    // check index
    if( head > b->used ){
        goto EMPTY_STRING;
    }
    else if( head < 0 )
    {
        if( ( head + b->used ) < 0 ){
            head = 0;
        }
        else {
            head += b->used;
        }
    }
    // length
    if( !lua_isnoneornil( L, 3 ) )
    {
        tail = luaL_checkinteger( L, 3 );
        if( tail < 1 ){
            goto EMPTY_STRING;
        }
        else if( head + tail < b->used ){
            tail += head;
        }
        else {
            tail = b->used;
        }

    }
    
    lua_pushlstring( L, b->mem + head, (size_t)(tail - head) );
    return 1;
    
EMPTY_STRING:
    lua_pushstring( L, "" );
    
    return 1;
}



static int free_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    
    pdealloc( b->mem );
    b->mem = NULL;
    b->used = 0;
    b->total = 0;
    
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
    int64_t unit = (int64_t)luaL_checkinteger( L, 1 );
    buf_t *b = NULL;
    
    // check arguments
    if( unit < 1 ){
        return luaL_argerror( L, 1, "unit size must be larger than 0" );
    }
    else if( ( b = lua_newuserdata( L, sizeof( buf_t ) ) ) )
    {
        if( ( b->mem = pnalloc( (size_t)unit, char ) ) ){
            b->unit = b->total = unit;
            b->used = 0;
            ((char*)b->mem)[0] = 0;
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
        { "free", free_lua },
        { "total", total_lua },
        { "upper", upper_lua },
        { "lower", lower_lua },
        { "set", set_lua },
        { "add", add_lua },
        { "insert", insert_lua },
        { "read", read_lua },
        { "sub", sub_lua },
        { "substr", substr_lua },
        { NULL, NULL }
    };
    
    define_mt( L, mmethod, method );
    // option method
    lua_pushcfunction( L, alloc_lua );
    
    return 1;
}


