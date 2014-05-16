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


#define BUF_SIZE_MAX \
    ((~((lua_Integer)1 << (sizeof(lua_Integer) * CHAR_BIT - 1)))-1)


// do not touch directly
typedef struct {
    // buffer
    lua_Integer unit;
    lua_Integer nmax;
    lua_Integer nalloc;
    lua_Integer used;
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


static inline int buf_alloc( buf_t *b, lua_Integer nalloc )
{
    if( nalloc > b->nmax ){
        errno = ENOMEM;
        return -1;
    }
    else if( nalloc > b->nalloc )
    {
        void *buf = realloc( b->mem, (size_t)(nalloc * b->unit) );
        
        if( !buf ){
            return -1;
        }
        b->nalloc = nalloc;
        b->mem = buf;
    }
    
    return 0;
}


static inline int buf_increase( buf_t *b, lua_Integer from, lua_Integer bytes )
{
    if( bytes < 0 || from < 0 || from > b->used ){
        errno = EINVAL;
        return -1;
    }
    else
    {
        lua_Integer total = b->unit * b->nalloc;
        
        bytes = -( total - from - bytes );
        if( bytes > 0 ){
            return buf_alloc( 
                b, b->nalloc + 
                ( bytes / b->unit + ( ( bytes % b->unit ) ? 1 : 0 ) ) 
            );
        }
    }
    
    return 0;
}


static inline void buf_term( buf_t *b, lua_Integer pos )
{
    b->used = pos;
    ((char*)b->mem)[b->used] = 0;
}


static int raw_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    
    lua_pushlightuserdata( L, b->mem );
    lua_pushinteger( L, b->used );
    
    return 2;
}


static int byte_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    lua_Integer head = 1;
    lua_Integer tail = 1;
    lua_Integer ret = 0;
    
    // check arguments
    if( !lua_isnoneornil( L, 2 ) ){
        head = luaL_checkinteger( L, 2 );
        if( head < 1 || head > b->used ){
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
        else if( tail > b->used ){
            tail = b->used;
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
    
    lua_pushinteger( L, b->unit * b->nalloc );
    
    return 1;
}


#define upperlower_lua(L,range,op) ({ \
    int rc = 2; \
    buf_t *b = getudata( L ); \
    unsigned char *lmem = pnalloc( (size_t)b->used, unsigned char ); \
    if( lmem ) { \
        unsigned char *ptr = (unsigned char*)b->mem; \
        lua_Integer i = 0; \
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
        buf_term( b, (lua_Integer)len );
        return 0;
    }
    else if( buf_increase( b, 0, (lua_Integer)len + 1 ) == 0 ){
        memcpy( b->mem, str, len );
        buf_term( b, (lua_Integer)len );
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
            if( buf_increase( b, b->used, (lua_Integer)len + 1 ) == 0 ){
                memcpy( b->mem + b->used, str, len );
                buf_term( b, b->used + (lua_Integer)len );
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
    if( len == 0 || idx > b->used ){
        return 0;
    }
    else if( idx > 0 ){
        idx--;
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
    
    if( buf_increase( b, b->used, (lua_Integer)len + 1 ) == 0 ){
        memmove( b->mem + idx + len, b->mem + idx, b->used - idx + 1 );
        memcpy( b->mem + idx, str, len );
        buf_term( b, b->used + (lua_Integer)len );
        return 0;
    }
    
    // got error
    lua_pushinteger( L, errno );
    
    return 1;
}


static int sub_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    lua_Integer head = luaL_checkinteger( L, 2 );
    lua_Integer tail = b->used;
    
    // check arguments
    // head
    if( head >= b->used ){
        goto EMPTY_STRING;
    }
    else if( head > 0 ){
        head--;
    }
    else if( ( head + b->used ) < 0 ){
        head = 0;
    }
    else {
        head += b->used;
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
    lua_Integer head = luaL_checkinteger( L, 2 );
    lua_Integer tail = b->used;
    
    // check arguments
    // check index
    if( head > b->used ){
        goto EMPTY_STRING;
    }
    else if( head > 0 ){
        head--;
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
        else if( b->used - head - tail > 0 ){
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


static int read_lua( lua_State *L )
{
    buf_t *b = getudata( L );
    int fd = luaL_checkint( L, 2 );
    lua_Integer bytes = b->unit;
    ssize_t len = 0;
    
    // check arguments
    if( fd < 0 ){
        return luaL_argerror( L, 2, "fd must be larger than 0" );
    }
    else if( !lua_isnoneornil( L, 3 ) )
    {
        bytes = luaL_checkinteger( L, 3 );
        if( bytes < 1 ){
            return luaL_argerror( L, 3, "bytes must be larger than 0" );
        }
    }
    
    if( buf_increase( b, b->used, bytes ) != 0 ){
        len = -1;
    }
    else if( ( len = read( fd, b->mem + b->used, (size_t)bytes ) ) > 0 ){
        buf_term( b, b->used + (lua_Integer)len );
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
    lua_Integer bytes = b->used;
    ssize_t len = 0;
    
    // check arguments
    if( fd < 0 ){
        return luaL_argerror( L, 2, "fd must be larger than 0" );
    }
    else if( pos <= 0 || pos >= b->used ){
        return luaL_argerror( L, 3, "pos must be larger than 0 and less than the used size" );
    }
    else {
        pos--;
        bytes -= pos;
    }
    
    len = write( fd, b->mem + (size_t)pos, (size_t)bytes );
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
        b->used = 0;
        b->nalloc = 0;
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
    
    lua_pushinteger( L, b->used );
    
    return 1;
}


static int alloc_lua( lua_State *L )
{
    lua_Integer unit = luaL_checkinteger( L, 1 );
    buf_t *b = NULL;
    
    // check arguments
    if( unit < 1 ){
        return luaL_argerror( L, 1, "size must be larger than 0" );
    }
    else if( ( b = lua_newuserdata( L, sizeof( buf_t ) ) ) )
    {
        if( ( b->mem = pnalloc( (size_t)unit, char ) ) ){
            b->unit = unit;
            b->nalloc = 1;
            b->nmax = BUF_SIZE_MAX / unit;
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


