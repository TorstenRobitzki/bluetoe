adler32 = require './adler32.js'

TIMEOUT_MS       = 500

class FlashRange
    ###
     @peripheral ble abstraction expected to have following functions
       start_flash( start_address, callback(error, mtu, receive_capacity, checksum) )
       send_data( data )
       register_progress_callback(callback(checksum, consecutive, mtu, receive_capacity))
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
        capacity        = 0
        data_checksums  = []
        consecutive     = 0

        send_data = ( that, mtu )->
            while that.data.length > 0 && that.capacity > 0
                send_size = Math.min( mtu - 3, that.data.length, that.capacity )
                send_data = that.data.slice( 0, send_size )

                that.peripheral.send_data( send_data )

                that.data     = that.data.slice send_size
                that.capacity = that.capacity - send_size
                console.log "calc_checksum: #{calc_checksum}"
                console.log "data: #{JSON.stringify send_data }"
                calc_checksum = adler32.adler32_buf send_data, calc_checksum
                console.log "result: #{calc_checksum}"
                data_checksums.push [ consecutive, calc_checksum ]
                consecutive = consecutive + 1

        check_progress_checksum = ( checksum, consecutive )->
            console.log "crc: #{checksum}"
            while data_checksums.length > 0
                console.log "data_checksums #{data_checksums}"
                [ con, crc ] = data_checksums.shift()

                if con == consecutive && crc == checksum
                    return true

            return false

        progress_callback = (that)->
            (checksum, consecutive, mtu, receive_capacity)->
                if !check_progress_checksum checksum, consecutive
                    that.cb 'checksum error in progress'
                else
                    if that.data.length > 0
                        send_data(that, mtu, receive_capacity)
                    else
                        @cb()

        start_flash_handler = (that)->
            (error, mtu, receive_capacity, checksum)->
                clearTimeout that.timer
                that.capacity = receive_capacity

                if error
                    that.cb( error )
                else
                    if checksum == calc_checksum
                        send_data(that, mtu, receive_capacity)
                    else
                        that.cb 'checksum error'

        address = []
        for [0...@address_size]
            address.push @start_address & 0xff
            @start_address = @start_address / 256

        calc_checksum = adler32.adler32_buf address

        @peripheral.register_progress_callback progress_callback @

        @timer = setTimeout( ( (that) ->
            -> that.cb("Timeout waiting for flash progress")
            )(@), TIMEOUT_MS )

        @peripheral.start_flash address, start_flash_handler @


exports.FlashMemory = FlashRange
