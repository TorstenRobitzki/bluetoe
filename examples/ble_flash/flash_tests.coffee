expect = require( 'chai' ).expect
assert = require( 'chai' ).assert
assert = require 'assert'
sinon  = require 'sinon'
flash  = require './flash.coffee'
util   = require 'util'

create_network_mock = ->
    {
        start_flash: sinon.spy(),
        send_data: sinon.spy(),
        register_progress_callback: sinon.spy()
    }

describe 'Network-Mock', ->
    network = null

    beforeEach ->
        network = create_network_mock()

    it 'implements start_flash', ->
        network.start_flash()

    it 'reports start_flash beeing called', ->
        network.start_flash()
        assert( network.start_flash.calledOnce )

describe 'FlashRange', ->

    network         = null

    beforeEach ->
        network = create_network_mock()

    describe 'start flashing', ->

        start_address   = 0x1000

        beforeEach ->
            new flash.FlashRange( network, start_address )

        it 'should start the flash once', ->
            assert( network.start_flash.calledOnce )

        it 'should send the start address', ->
            expect( network.start_flash.getCall( 0 ).args[ 0 ] ).to.equal start_address

        it 'should pass a callback as second parameter', ->
            expect( network.start_flash.getCall( 0 ).args.length ).to.equal 2


