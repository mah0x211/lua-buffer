local hex = require('hex');
local buffer = require('buffer');
local str = 'hello world!';
local b = ifNil( buffer.new( 100 ) );
local enc, dec;

ifNotNil( b:add( str ) );
enc = ifNil( b:hex() );
dec = ifNil( hex.decode( enc ) );
ifNotEqual( str, dec );
