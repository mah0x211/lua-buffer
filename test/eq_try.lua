local buffer = require('buffer');
local str = 'hello world!';
local a = ifNil( buffer( 100 ) );
local b = ifNil( buffer( 100 ) );
local enc, dec;

ifNotNil( a:set( str ) );
ifNotNil( b:set( str ) );
ifNotTrue( a == b and b == a );
ifNotNil( a:add( 'x' ) );
ifTrue( a == b and b == a );
