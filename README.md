lua-buffer
=========

buffer module.

# Create Buffer Object

## buf, errno = buffer( size )

**Parameters**

- bytes: the number of bytes.

**Returns**

1. buf: buffer object.
2. errno: depend on a system.

**Example**

```lua
local buffer = require('buffer');
local buf, err = buffer(128);
```


# Methods

## mem, bytes = buf:raw()

return raw memory pointer and number of bytes.

**Returns**

1. mem: raw memory pointer (lightuserdata)
2. bytes: number of bytes.

## buf:free()

this method will deallocate memory immediately.  
after calling this method, the buffer object can no longer be used.


## bytes = buf:total()

return the bytes of allocated memory.

**Returns**

1. bytes: the bytes of allocated memory.


## str, errno = buf:upper()

returns the copy of string converted to uppercase.

**Returns**

1. str: the uppercase string.
2. errno: errno of memory allocation failure.


## str, errno = buf:lower()

returns the copy of string converted to lowercase.

**Returns**

1. str: the lowercase string.
2. errno: errno of memory allocation failure.


## errno = buf:set( str )

copy the specified string.

**Returns**

1. errno: errno of memory allocation failure.


## errno = buf:add( str1[, str2[, ...]] )

append the all arguments at the tail of buffer.

**Returns**

1. errno: errno of memory allocation failure.


## errno = buf:insert( idx, str )

insert the string at the idx position.

**Returns**

1. errno: errno of memory allocation failure.


## errno = buf:sub( from[, to] )

returns a string between the start index and the end index. or, through the end of the string from start index.

**Returns**

1. errno: errno of memory allocation failure.


## errno = buf:substr( from[, len] )

returns a string between the start index and the start index + specified length. or, through the end of the string from start index.

**Returns**

1. errno: errno of memory allocation failure.


## bytes, errno = buf:read( fd, bytes )

read specified number of bytes from the specified descriptor and return the actual number of bytes read.

**Returns**

1. bytes: number of bytes read.
2. errno: errno of read failure.

