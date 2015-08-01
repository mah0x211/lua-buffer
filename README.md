lua-buffer
=========

buffer module.

## Installation

```sh
luarocks install buffer --from=http://mah0x211.github.io/rocks/
```


## Create Buffer Object

### buf, err = buffer.new( size [, fd [, cloexec]] )

**Parameters**

- `bytes:uint`: size of memory allocation.
- `fd:uint`: descriptor for read and write methods.
- `cloexec:boolean`: file descriptor to be automatically closed when freeing buffer.

**Returns**

1. `buf:userdata`: buffer object.
2. `err:string`: error message.

**Example**

```lua
local buffer = require('buffer');
local buf, err = buffer.new(128);
```


## Methods

### mem, bytes = buf:raw()

return raw memory pointer and number of bytes.

**Returns**

1. `mem:lightuserdata`: raw memory pointer.
2. `bytes:int`: number of bytes.


### buf:free()

this method will deallocate memory immediately.  
after calling this method, the buffer object can no longer be used.


### code, ... = buf:byte( [i [, j]] )

returns the internal numerical codes of the characters s[i], s[i+1], ..., s[j].

**Parameters**

- `i:uint`: index number. (default: 1)
- `j:uint`: index number. (default: same as i)

**Returns**

1. `code:uint`: numerical codes of the characters.


### bytes = buf:total()

return the bytes of allocated memory.

**Returns**

1. `bytes:uint`: the bytes of allocated memory.


### str, err = buf:upper()

returns the copy of string converted to uppercase.

**Returns**

1. `str:string`: the uppercase string.
2. `err:string`: error message of memory allocation failure.


### str, err = buf:lower()

returns the copy of string converted to lowercase.

**Returns**

1. `str:string`: the lowercase string.
2. `err:string`: error message of memory allocation failure.


### str, err = buf:hex()

returns the copy of string converted to hexadecimal encode.

**Returns**

1. `str:string`: the hexadecimal encoded string.
2. `err:string`: error message of memory allocation failure.


### str, err = buf:base64()

returns the copy of string converted to base64 encode.

**Returns**

1. `str:string`: the base64 encoded string.
2. `err:string`: error message of memory allocation failure(ENOMEM), or result too large(ERANGE).


### str, err = buf:base64url()

returns the copy of string converted to base64url encode.

**Returns**

1. `str:string`: the base64url encoded string.
2. `err:string`: error message of memory allocation failure(ENOMEM), or result too large(ERANGE).


### err = buf:set( str )

copy the specified string.

**Parameters**

- `str:string`: string.

**Returns**

1. `err:string`: error message of memory allocation failure.


### err = buf:add( str1 [, str2 [, ...]] )

append the all arguments at the tail of buffer.

**Parameters**

- `str1..strN:string`: target strings.

**Returns**

1. `err:string`: error message of memory allocation failure.


### err = buf:insert( idx, str )

insert the string at the idx position.

**Parameters**

- `idx:int`: position of insertion that starting from 1.
- `str:string`: insertion string.

**Returns**

1. `err:string`: error message of memory allocation failure.


### str = buf:sub( from [, to] )

returns a substring between the start position and the end position. or, through the end of the string from start position.

**Parameters**

- `from:int`: start position for string extraction that starting from 1.
- `to:int`: the end position for string extraction.

**Returns**

1. `str:string`: substring.


### str = buf:substr( from [, len] )

returns a substring between the start position and the start position + specified length. or, through the end of the string from start position.

**Parameters**

- `from:int`: start position for string extraction that starting from 1.
- `len:uint`: extraction length.

**Returns**

1. `str:string`: substring.


### buf:setfd( fd [, cloexec] )

set descriptor for read and write methods.

**Parameters**

- `fd:uint`: file descriptor.
- `cloexec:boolean`: file descriptor to be automatically closed when freeing buffer.

**Returns**

no return value.

### flag = buf:cloexec( [flag] )

returns a current closexec flag.
if a cloexec argument specified, set the flag for file descriptor automatic closing.

**Parameters**

- `cloexec:boolean`: file descriptor to be automatically closed when freeing buffer.

**Returns**

1. `flag:boolean`: current cloexec flag.


### bytes, err, again = buf:read( [bytes] )

read data into buffer from the descriptor and return the actual number of bytes read.

**Parameters**

- `bytes:uint`: number of bytes for read. (default: size of memory allocation)

**Returns**

1. `bytes:int`: number of bytes read.
2. `err:string`: error message of read failure.
3. `again:boolean`: true if errno was EAGAIN or EWOULDBLOCK.


### bytes, err, again = buf:readadd( [bytes] )

read data into last position of buffer from the descriptor and return the actual number of bytes read.

**Parameters**

- `bytes:uint`: number of bytes for read. (default: size of memory allocation)

**Returns**

1. `bytes:int`: number of bytes read.
2. `err:string`: error message of read failure.
3. `again:boolean`: true if errno was EAGAIN or EWOULDBLOCK.


### bytes, err, again = buf:write( str )

write str to the descriptor and return the actual number of bytes written.

**Parameters**

- `str:string`: target string.

**Returns**

1. `bytes:int`: number of bytes written.
2. `err:string`: error message of write failure.
3. `again:boolean`: true if errno was EAGAIN or EWOULDBLOCK.


### bytes, err, again = buf:flush()

write buffer data to the descriptor and return the actual number of bytes written.

**Returns**

1. `bytes:int`: number of bytes written.
2. `err:string`: error message of write failure.
3. `again:boolean`: true if errno was EAGAIN or EWOULDBLOCK.
