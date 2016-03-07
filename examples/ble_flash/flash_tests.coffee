util    = require 'util'
expect  = require( 'chai' ).expect
assert  = require( 'chai' ).assert
assert  = require 'assert'
sinon   = require 'sinon'
flash   = require './flash.coffee'
util    = require 'util'
adler32 = require 'adler-32'

create_network_mock = ->
    {
        start_flash: sinon.spy(),
        send_data: sinon.spy(),
        register_progress_callback: sinon.spy()
    }

last_data_being_send = ( network_mock )->
    network_mock.send_data.lastCall.args[ 0 ]

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
    data            = new Buffer( 3 * 1024 )
    address_size    = 4
    page_size       = 1024
    page_buffers    = 3
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
            new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback

        it 'should start to flash once', ->
            assert( network.start_flash.calledOnce )

        it 'should send the start address with the address_size', ->
            expect( network.start_flash.getCall( 0 ).args[ 0 ] ).to.deep.equal [ 0x78, 0x56, 0x34, 0x12 ]

        it 'should pass a callback as second parameter', ->
            expect( network.start_flash.getCall( 0 ).args.length ).to.equal 2

        it 'should anticipate errors', ->
            network.start_flash.getCall( 0 ).args[ 1 ]( "something got wrong" )
            expect( error_callback.getCall( 0 ).args[ 0 ] ).to.equal "something got wrong"

        it 'no errors no callback called', ->
            expect( error_callback.callCount ).to.equal 0

        it 'does not call send_data before the end of the procedure', ->
            expect( network.send_data.callCount ).to.equal 0

        describe 'start flashing have an 6 byte address length', ->

            beforeEach ->
                network         = create_network_mock()
                start_address   = 0x123456789abc
                address_size    = 6

                new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback

            it 'should send the start address with the address_size', ->
                expect( network.start_flash.getCall( 0 ).args[ 0 ] ).to.deep.equal(
                    [ 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12 ])

    describe 'running in a timeout', ->
        beforeEach ->
            new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback
            clock.tick 1000

        it 'will call the error callback', (done)->
            assert( error_callback.calledOnce )
            done()

    describe 'after receiving the start_address procedure result', ->

        mtu                 = 42
        receive_capacity    = page_size * page_buffers
        checksum            = adler32.buf [ 1, 2, 3, 4 ]

        beforeEach ->
            new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback
            network.start_flash.lastCall.args[ 1 ]( null, mtu, receive_capacity, checksum )

        it 'starts sending data', ->
            assert( network.send_data.calledOnce )

        it 'data size is MTU -3 ', ->
            expect( network.send_data.firstCall.args[ 0 ].length ).to.equal( mtu - 3 )

    describe 'after receiving the start_address procedure result with a wrong crc', ->

        it 'calls the error callback'

    describe 'sending data', ->

        describe 'start address equal to a page address', ->

            beforeEach ->
                start_address = 3 * page_size
                new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback

            it 'should send data up to the receive capacity'


        describe 'start address not beeign equal to a page address', ->

            it 'should send the remaining data up to the end of the page'

        describe 'writeing just a part of a page', ->

            describe 'start address equal to a page address', ->

                it 'should send the data'

            describe 'start address not beeign equal to a page address', ->

                it 'should send the data'


    describe 'receiving progress', ->

    describe 'continously reveivin progress', ->

