expect  = require( 'chai' ).expect
assert  = require 'assert'
sinon   = require 'sinon'
flash   = require './flash.coffee'
util    = require 'util'
crc     = require './crc.coffee'

create_network_mock = ( data_cb )->
    {
        start_flash: sinon.spy(),
        send_data: if data_cb then data_cb else sinon.spy(),
        flush: sinon.spy()
        register_progress_callback: sinon.spy()
    }

collect_data_send = ( network )->
    send_data = network.send_data
    result    = new Buffer(0)

    for n in [ 0...send_data.callCount ]
        result = Buffer.concat([result, send_data.getCall(n).args[0]])

    result

random_buffer = ( size )->
    result = new Buffer size

    result[n] = Math.floor(Math.random() * 255) for n in [0...size]
    result

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
    network             = null
    start_address       = 0x12345678
    data                = random_buffer( 3 * 1024 )
    address_size        = 4
    page_size           = 1024
    page_buffers        = 3
    error_callback      = null
    mtu                 = 42
    receive_capacity    = page_size * page_buffers

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

        beforeEach ->
            network         = create_network_mock()
            start_address   = 0x12345678
            address_size    = 4

        checksum = crc.buf [ 0x78, 0x56, 0x34, 0x12 ]

        beforeEach ->
            new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback
            network.start_flash.lastCall.args[ 1 ]( null, mtu, checksum )

        it 'starts sending data', ->
            expect( network.send_data.callCount ).to.not.equal 0

        it 'data size is MTU -3 ', ->
            expect( network.send_data.firstCall.args[ 0 ].length ).to.equal( mtu - 3 )

    describe 'after receiving the start_address procedure response with a wrong crc', ->

        beforeEach ->
            new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback
            network.start_flash.lastCall.args[ 1 ]( null, mtu, 0xdeadbeef )

        it 'calls the error callback', ->
            assert( error_callback.calledOnce )
            expect( error_callback.lastCall.args[ 0 ] ).to.equal 'checksum error'

    describe 'sending data', ->

        beforeEach ->
            data          = random_buffer( receive_capacity + page_size )
            address_size  = 4
            mtu           = 42

        describe 'start address equal to a page address', ->

            beforeEach ->
                start_address = 3 * page_size
                checksum      = crc.buf [ 0x00, 0x0C, 0x00, 0x00 ]

                new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback
                network.start_flash.lastCall.args[ 1 ]( null, mtu, checksum )

            it 'should not call the error_callback', ->
                expect( error_callback.callCount ).to.equal 0

            it 'should send data up to the receive capacity', ->
                expect( collect_data_send( network ).length ).to.equal receive_capacity

            it 'should send the first data', ->
                expect( collect_data_send( network ) ).to.deep.equal( data.slice( 0, receive_capacity ) )

        describe 'start address not beeign equal to a page address', ->

            checksum    = null

            beforeEach ->
                start_address   = 3 * page_size + 0x123
                checksum        = crc.buf [ 0x23, 0x0D, 0x00, 0x00 ]
                receive_capacity= page_size * ( page_buffers - 1 ) + ( page_size - 0x123 )

                new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback
                network.start_flash.lastCall.args[ 1 ]( null, mtu, checksum )

            it 'should not call the error_callback', ->
                expect( error_callback.callCount ).to.equal 0

            it 'should send data up to the receive capacity', ->
                expect( collect_data_send( network ).length ).to.equal receive_capacity

            it 'should send the first data', ->
                expect( collect_data_send( network ) ).to.deep.equal( data.slice( 0, receive_capacity ) )

            it 'sends more data when progress indicates a freed block received', ->
                # Simulate that the bootloader sends a progress message, right after the first page
                data_send = collect_data_send( network )
                checksum  = crc.buf data_send.slice( 0, ( page_size - 0x123 ) ), checksum

                progress_callback = network.register_progress_callback.lastCall.args[ 0 ]
                progress_callback( checksum, 0, mtu )

                expect( error_callback.called ).to.be.false
                expect( collect_data_send( network ).length ).to.be.least data_send.length

        describe 'writeing just a part of a page', ->

            checksum = 0

            beforeEach ->
                start_address   = 3 * page_size
                checksum        = crc.buf [ 0x00, 0x0C, 0x00, 0x00 ]
                data            = random_buffer( 42 )

                new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback

                start_flash_callback = network.start_flash.lastCall.args[ 1 ]
                start_flash_callback( null, mtu, checksum )

            describe 'start address equal to a page address', ->

                it 'should send the data', ->
                    expect( collect_data_send( network ) ).to.deep.equal( data )

                it 'should wait for a handshake', ->
                    expect( error_callback.called ).to.be.false
                    network.register_progress_callback.lastCall.args[ 0 ](checksum, 0, mtu )
                    expect( error_callback.called ).to.be.true

    describe 'receiving progress', ->
        beforeEach ->
            data          = random_buffer( 3 * receive_capacity )
            address_size  = 4
            mtu           = 42

        describe 'fails when', ->

            beforeEach ->
                start_address = 3 * page_size
                checksum      = crc.buf [ 0x00, 0x0C, 0x00, 0x00 ]

                new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback

                start_flash_callback = network.start_flash.lastCall.args[ 1 ]
                start_flash_callback( null, mtu, checksum )

            it 'checksum error is detected', ->
                consecutive = 3
                expect( error_callback.called ).to.be.false

                progress_callback = network.register_progress_callback.lastCall.args[ 0 ]
                progress_callback( 0xdeadbeef, consecutive, mtu )

                expect( error_callback.called ).to.be.true

    describe 'continously reveiving progress', ->

        checksum      = null

        beforeEach ->
            receive_capacity = page_size * page_buffers
            network          = create_network_mock()
            data             = random_buffer( 3 * receive_capacity )
            address_size     = 4
            mtu              = 42

            start_address    = 3 * page_size
            checksum         = crc.buf [ 0x00, 0x0C, 0x00, 0x00 ]

            new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback

            start_flash_callback = network.start_flash.lastCall.args[ 1 ]
            start_flash_callback( null, mtu, checksum )

        it 'sends data until receive capacity is reached', ->
            expect( collect_data_send( network ).length ).to.equal receive_capacity

        it 'sends more data when progress indicates a freed block received', ->
            # Simulate that the bootloader sends a progress message, right after the first page
            data_send = collect_data_send( network )

            checksum  = crc.buf data_send.slice( 0, page_size ), checksum

            progress_callback = network.register_progress_callback.lastCall.args[ 0 ]
            progress_callback( checksum, 0, mtu )

            expect( error_callback.called ).to.be.false
            expect( collect_data_send( network ).length ).to.be.least receive_capacity + page_size

        it 'sends new data with the new mtu size', ->
            # Simulate that the bootloader sends a progress message, right after the first page
            data_send = collect_data_send( network )

            checksum  = crc.buf data_send.slice( 0, page_size ), checksum
            new_mtu   = mtu - 4

            progress_callback = network.register_progress_callback.lastCall.args[ 0 ]
            progress_callback( checksum, 0, new_mtu )

            send_data = network.send_data
            expect( send_data.getCall( send_data.callCount - 2 ).args[0].length ).to.equal new_mtu - 3

        it 'indicates error when progress indicates all data received at once', ->
            # Simulate that the bootloader sends a progress message, right after the first page
            data_send = collect_data_send( network )

            checksum  = crc.buf data_send, checksum

            progress_callback = network.register_progress_callback.lastCall.args[ 0 ]
            progress_callback( checksum, 2, mtu )

            expect( error_callback.called ).to.be.true

    describe 'end of flash', ->

        checksum          = null
        progress_callback = null
        consecutive       = null

        beforeEach ->
            page_buffers     = 2
            receive_capacity = page_size * page_buffers
            network          = create_network_mock()
            data             = random_buffer( 2 * receive_capacity )
            address_size     = 3
            mtu              = 42
            consecutive      = 0

            start_address    = 3 * page_size
            checksum         = crc.buf [ 0x00, 0x0C, 0x00 ]

            new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback

            start_flash_callback = network.start_flash.lastCall.args[ 1 ]
            start_flash_callback( null, mtu, checksum )

            progress_callback = network.register_progress_callback.lastCall.args[ 0 ]

        progress_for_next_buffer = ->
            data_send = collect_data_send( network )

            checksum  = crc.buf data_send.slice( consecutive * page_size, ( consecutive + 1 ) * page_size ), checksum

            progress_callback( checksum, consecutive, mtu )

            consecutive = consecutive + 1

        it 'does not send indicates the end of flash, after sending all data', ->
            progress_for_next_buffer()
            progress_for_next_buffer()
            progress_for_next_buffer()

            expect( error_callback.called ).to.be.false
            expect( collect_data_send( network ).length ).to.be.least data.length

        it 'callback called, after all data was received', ->
            for [ 0...4 ]
                progress_for_next_buffer()

            expect( error_callback.called ).to.be.true
            expect( error_callback.lastCall.args[ 0 ] ).to.not.exist

    describe 'flashing not full block', ->
        checksum          = null
        progress_callback = null
        consecutive       = null

        beforeEach ->
            page_buffers     = 2
            receive_capacity = page_size * page_buffers
            network          = create_network_mock()
            data             = random_buffer( 2 * receive_capacity - 1 )
            address_size     = 3
            mtu              = 42
            consecutive      = 0

            start_address    = 3 * page_size
            checksum         = crc.buf [ 0x00, 0x0C, 0x00 ]

            new flash.FlashMemory network, start_address, data, address_size, page_size, page_buffers, error_callback

            start_flash_callback = network.start_flash.lastCall.args[ 1 ]
            start_flash_callback( null, mtu, checksum )

            progress_callback = network.register_progress_callback.lastCall.args[ 0 ]

        progress_for_next_buffer = ->
            data_send = collect_data_send( network )

            checksum  = crc.buf data_send.slice( consecutive * page_size, ( consecutive + 1 ) * page_size ), checksum

            progress_callback( checksum, consecutive, mtu )

            consecutive = consecutive + 1

        it 'does not call flush if not the last full buffer was flushed', ->
            progress_for_next_buffer()
            progress_for_next_buffer()

            expect( error_callback.called ).to.be.false
            expect( network.flush.called ).to.be.false

        it 'calls flush if the last full buffer was flushed', ->
            progress_for_next_buffer()
            progress_for_next_buffer()
            progress_for_next_buffer()

            expect( error_callback.called ).to.be.false
            expect( network.flush.called ).to.be.true

            data_send = collect_data_send( network )
            expect( data_send ).to.deep.equal data

        it 'error in flush response is anticipated', ->
            progress_for_next_buffer()
            progress_for_next_buffer()
            progress_for_next_buffer()

            flash_callback = network.flush.lastCall.args[ 0 ]
            flash_callback "error!!!!111"

            expect( error_callback.called ).to.be.true
            expect( error_callback.lastCall.args[ 0 ] ).to.equal "error!!!!111"

        it 'crc error in flush response is anticipated', ->
            progress_for_next_buffer()
            progress_for_next_buffer()
            progress_for_next_buffer()

            flash_callback = network.flush.lastCall.args[ 0 ]
            flash_callback( null, 0x12345678, 3 )

            expect( error_callback.called ).to.be.true
            expect( error_callback.lastCall.args[ 0 ] ).to.equal 'checksum error in flush response'

        it 'callback called after flush response', ->
            progress_for_next_buffer()
            progress_for_next_buffer()
            progress_for_next_buffer()

            checksum  = crc.buf data.slice( consecutive * page_size, ( consecutive + 1 ) * page_size ), checksum

            flash_callback = network.flush.lastCall.args[ 0 ]
            flash_callback( null, checksum, 3 )

            expect( error_callback.called ).to.be.true
            expect( error_callback.lastCall.args[ 0 ] ).to.not.exist
