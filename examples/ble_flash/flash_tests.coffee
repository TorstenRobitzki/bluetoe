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

describe 'FlashMemory', ->

    # c'tor parameters to FlashMemory
    network         = null
    start_address   = 0x12345678
    data            = null
    address_size    = 4
    page_size       = 1024
    error_callback  = null

    clock           = null

    beforeEach ->
        network         = create_network_mock()
        clock           = sinon.useFakeTimers()
        error_callback  = sinon.spy()

    afterEach ->
        clock.restore()

    describe 'start flashing', ->

        beforeEach ->
            new flash.FlashMemory network, start_address, data, address_size, page_size, error_callback

        it 'should start the flash once', ->
            assert( network.start_flash.calledOnce )

        it 'should send the start address with the address_size', ->
            expect( ( f for f in network.start_flash.getCall( 0 ).args[ 0 ] ) ).to.deep.equal [ 0x78, 0x56, 0x34, 0x12 ]

        it 'should pass a callback as second parameter', ->
            expect( network.start_flash.getCall( 0 ).args.length ).to.equal 2

        describe 'start flashing have an 6 byte address length', ->

            beforeEach ->
                network         = create_network_mock()
                start_address   = 0x123456789abc
                address_size    = 6

                new flash.FlashMemory network, start_address, data, address_size, page_size, error_callback

            it 'should send the start address with the address_size', ->
                expect( network.start_flash.getCall( 0 ).args[ 0 ] ).to.deep.   equal(
                    [ 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12 ])

    describe 'running in a timeout', ->
        beforeEach ->
            new flash.FlashMemory( network, start_address )
            clock.tick 1000

        it 'will call the error callback', ->



    describe 'receiving progress', ->

