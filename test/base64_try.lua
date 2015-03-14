local base64 = require('base64mix');
local buffer = require('buffer');
local str = 'ab?cd~';
local b = ifNil( buffer( 100 ) );
local enc, dec;

ifNotNil( b:add( str ) );
enc = ifNil( b:base64() );
dec = ifNil( base64.decode( enc ) );
ifNotEqual( str, dec );
ifNotNil( base64.decodeURL( enc ) );

enc = ifNil( b:base64url() );
dec = ifNil( base64.decodeURL( enc ) );
ifNotEqual( str, dec );
ifNotNil( base64.decode( enc ) );
