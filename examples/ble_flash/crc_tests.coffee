expect  = require( 'chai' ).expect
assert  = require 'assert'
crc     = require './crc.coffee'

describe 'crc', ->
    it 'calculating crc in two parts results in same result', ->
        buffer = new Buffer [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
        expect( crc.buf( buffer.slice( 5 ), crc.buf( buffer.slice( 0, 5 ) ) ) ).to.equal crc.buf( buffer )
