lua-buffer
=========

buffer module.

## Installation

```sh
luarocks install --from=http://mah0x211.github.io/rocks/ buffer
```

or 

```sh
git clone https://github.com/mah0x211/lua-buffer.git
cd lua-buffer
luarocks make
```


## Create Buffer Object

### buf, errno = buffer( size [, fd] )

**Parameters**

- bytes: size of memory allocation.
- fd: descriptor for read and write methods.

**Returns**

1. buf: buffer object.
2. errno: depend on a system.

**Example**

```lua
local buffer = require('buffer');
local buf, err = buffer(128);
```


## Methods

### mem, bytes = buf:raw()

return raw memory pointer and number of bytes.

**Returns**

1. mem: raw memory pointer (lightuserdata)
2. bytes: number of bytes.

### buf:free()

this method will deallocate memory immediately.  
after calling this method, the buffer object can no longer be used.

### code, ... = buf:byte( [i [, j]] )

returns the internal numerical codes of the characters s[i], s[i+1], ..., s[j].

**Parameters**

- i: index number. (default: 1)
- j: index number. (default: same as i)

**Returns**

1. code: numerical codes of the characters.


### bytes = buf:total()

return the bytes of allocated memory.

**Returns**

1. bytes: the bytes of allocated memory.


### str, errno = buf:upper()

returns the copy of string converted to uppercase.

**Returns**

1. str: the uppercase string.
2. errno: errno of memory allocation failure.


### str, errno = buf:lower()

returns the copy of string converted to lowercase.

**Returns**

1. str: the lowercase string.
2. errno: errno of memory allocation failure.


### errno = buf:set( str )

copy the specified string.

**Parameters**

- str: string.

**Returns**

1. errno: errno of memory allocation failure.


### errno = buf:add( str1 [, str2 [, ...]] )

append the all arguments at the tail of buffer.

**Parameters**

- str1..strN: string.

**Returns**

1. errno: errno of memory allocation failure.


### errno = buf:insert( idx, str )

insert the string at the idx position.

**Parameters**

- idx: position of insertion that starting from 1.
- str: insertion string.


**Returns**

1. errno: errno of memory allocation failure.


### str = buf:sub( from [, to] )

returns a substring between the start position and the end position. or, through the end of the string from start position.

**Parameters**

- from: start position for string extraction that starting from 1.
- to: the end position for string extraction.

**Returns**

1. str: substring.


### str = buf:substr( from [, len] )

returns a substring between the start position and the start position + specified length. or, through the end of the string from start position.

**Parameters**

- from: start position for string extraction that starting from 1.
- to: the end position for string extraction.

**Returns**

1. str: substring.


### buf:setfd( fd )

set descriptor for read and write methods.

**Parameters**

- fd: descriptor.

**Returns**

no return value.


### bytes, errno, again = buf:read( [append [, bytes]] )

read data into buffer from the descriptor and return the actual number of bytes read.

**Parameters**

- append: append mode. (default: false)
- bytes: number of bytes for read. (default: size of memory allocation)

**Returns**

1. bytes: number of bytes read.
2. errno: errno of read failure.
3. again: true if errno was EAGAIN or EWOULDBLOCK.


### bytes, errno, again = buf:write( str )

write buffer data to the descriptor and return the actual number of bytes written.

**Parameters**

- str: string.

**Returns**

1. bytes: number of bytes written.
2. errno: errno of write failure.
3. again: true if errno was EAGAIN or EWOULDBLOCK.



