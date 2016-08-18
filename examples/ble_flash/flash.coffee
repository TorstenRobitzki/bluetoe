crc32 = require './crc.coffee'

TIMEOUT_MS       = 500

class FlashRange
    ###
     @peripheral ble abstraction expected to have following functions
       start_flash( start_address, callback(error, mtu, checksum) )
       send_data( data )
       flush( callback( error, checksum, consecutive ) )
       register_progress_callback(callback(checksum, consecutive, mtu))
       unregister_progress_callback(callback)
     @start_address address to flash the data on the target
     @data Buffer containing the data to be flashed
     @address_size size of an address on the target in bytes
     @page_size size of flash page on the target in bytes
     @page_buffer number of page buffers within the targets bootloader
     @cb callback to be called, when the flashing is done or when an error occured
    ###
    constructor: (@peripheral, @start_address, @data, @address_size, @page_size, @page_buffer, @cb)->
        timer           = null
        calc_checksum   = null
        capacity        = @page_size * @page_buffer

        if @start_address % @page_size != 0
            capacity        = capacity - ( @start_address % @page_size )

        data_checksums  = []
        consecutive     = 0

        send_data = ( that, mtu )->
            while that.data.length > 0 && capacity > 0
                send_size = Math.min( mtu - 3, that.data.length, capacity )
                new_data  = that.data.slice( 0, send_size )

                that.peripheral.send_data( new_data )

                # if this is the last chunk of a page, that page will be freed again, if the bootloader
                # reports progress behind this chunk.
                if Math.floor( ( capacity - 1 ) % that.page_size ) + 1 <= send_size
                    length_till_block_end   = capacity % that.page_size

                    block_checksum = crc32.buf new_data.slice( 0, length_till_block_end ), calc_checksum
                    data_checksums.push [ consecutive, block_checksum ]
                    consecutive    = consecutive + 1

                that.data   = that.data.slice send_size
                capacity    = capacity - send_size

                calc_checksum  = crc32.buf new_data, calc_checksum

        check_progress_checksum = ( checksum, consecutive )->
            if data_checksums.length > 0
                [ con, crc_calculated ] = data_checksums.shift()

                if con == consecutive && crc_calculated == checksum
                    return true

            false

        flush_callback = ( that )->
            ( error, checksum, con )->
                if error
                    that.cb( error )
                else
                    if calc_checksum != checksum || consecutive != con
                        that.cb 'checksum error in flush response'
                    else
                        that.cb()

        progress_callback = (that)->
            (checksum, con, mtu )->
                if !check_progress_checksum( checksum, con )
                    that.cb 'checksum error in progress'
                else
                    capacity = capacity + that.page_size

                    if that.data.length > 0
                        send_data(that, mtu, capacity)
                    else
                        if data_checksums.length == 0
                            if capacity == that.page_size * that.page_buffer
                                that.cb()
                            else
                                that.peripheral.flush flush_callback( that )

        start_flash_handler = (that)->
            (error, mtu, checksum)->
                clearTimeout that.timer

                if error
                    that.cb( error )
                else
                    if checksum == calc_checksum
                        send_data(that, mtu, capacity)
                    else
                        that.cb 'checksum error'

        address = []
        for [0...@address_size]
            address.push @start_address & 0xff
            @start_address = @start_address / 256

        calc_checksum = crc32.buf address

        @peripheral.register_progress_callback progress_callback( @ )

        @timer = setTimeout( ( (that) ->
            -> that.cb("Timeout waiting for flash progress")
            )(@), TIMEOUT_MS )

        @peripheral.start_flash address, start_flash_handler( @ )


exports.FlashMemory = FlashRange
