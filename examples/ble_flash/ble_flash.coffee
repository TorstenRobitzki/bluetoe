raise = (text)->
    console.log text
    process.exit 1

FlashMemory = require( './flash.coffee' ).FlashMemory
noble       = require 'noble'
util        = require 'util'
fs          = require 'fs'
options     = require('minimist')(
    process.argv.slice(2), {
        boolean: ['help', 'version', 'list', 'verbose'],
        string: ['address', 'device', 'flash'],
        alias: {
            'help': ['h'],
            'address': ['a'],
            'device' : ['d'],
            'version': ['v'],
            'list'   : ['l'],
            'flash'  : ['f'],
            'verbose': ['V']
        }
        unknown: ( field )->
            raise "Unrecognizes argument \"#{field}\""
    })

VERBOSE                     = options[ 'verbose' ]

BOOTLOADER_SERVICE_UUID     = '7d295f4d28504f57b595837f5753f8a9'
CONTROL_POINT_UUID          = '7d295f4d28504f57b595837f5753f8a9'
DATA_UUID                   = '7d295f4d28504f57b595837f5753f8aa'
PROGRESS_UUID               = '7d295f4d28504f57b595837f5753f8ab'
ALL_CHARACTERISTIC_UUIDS    = [ CONTROL_POINT_UUID, DATA_UUID, PROGRESS_UUID ]

control_point_char          = null
data_char                   = null
progress_char               = null

OPC_GET_VERSION = 0
OPC_GET_CRC     = 1
OPC_GET_SIZES   = 2
OPC_START_FLASH = 3
OPC_STOP_FLASH  = 4
OPC_FLUSH       = 5
OPC_START       = 6
OPC_RESET       = 7

control_point_callback = (data, is_notification)->
    raise "Unexpected control point notification"

default_progress_callback = ->
    raise "Unexpected progress notification"

progress_callback      = default_progress_callback

scan_devices = (mac, cb)->
    devices = {}
    timer   = null

    noble.on 'discover', ( peripheral )->
        if BOOTLOADER_SERVICE_UUID in peripheral.advertisement.serviceUuids
            if mac
                if mac == peripheral.address
                    clearTimeout timer
                    cb(peripheral)
            else
                devices[ peripheral.id ] = peripheral

    noble.on 'stateChange', ( state )->

        if state == 'poweredOn'
            timer = setTimeout (()->
                    noble.stopScanning()
                    if mac then cb(null) else cb(devices)
                ), 3000

            noble.startScanning []

connect_device = (peripheral, cb)->
    peripheral.connect (error)->
        if error
            cb(null, error)
        else
            peripheral.discoverSomeServicesAndCharacteristics [BOOTLOADER_SERVICE_UUID], ALL_CHARACTERISTIC_UUIDS, ( error, services, characteristics )->
                if ( error )
                    console.log "Error in discoverSomeServicesAndCharacteristics: #{error}"
                    cb(null, error)
                else
                    all = {}
                    all[characteristic.uuid] = characteristic for characteristic in characteristics

                    control_point_char  = all[CONTROL_POINT_UUID]
                    data_char           = all[DATA_UUID]
                    progress_char       = all[PROGRESS_UUID]

                    control_point_char.on 'data', (data, is_notification)->
                        control_point_callback( data, is_notification )

                    progress_char.on 'data', (data, is_notification)->
                        progress_callback( data, is_notification )

                    control_point_char.notify true, (error)->
                        if error
                            cb(null, error)
                        else
                            progress_char.notify true, (error)->
                                if error
                                    cb(null, error)
                                else
                                    cb(peripheral, null)

parse_control_point_response = (response_code, data, cb)->
    expected_size = (size)->
        if data.length != size
            raise "while parsing response (code=#{response_code}), the response expected to be #{size} in size, but was #{data.length}"

    switch response_code
        when OPC_GET_VERSION
            cb(null, data.toString())
        when OPC_GET_CRC
            expected_size 4
            cb(null, data.readUInt32LE(0))
        when OPC_GET_SIZES
            expected_size 9
            cb(null, data.readUInt8(0), data.readUInt32LE(1), data.readUInt32LE(5))
        when OPC_START_FLASH
            expected_size 5
            cb(null, data.readUInt8(0), data.readUInt32LE(1))
        when OPC_STOP_FLASH
            expected_size 0
            cb(null)
        when OPC_FLUSH
            expected_size 6
            cb(null, data.readUInt32LE(0), data.readUInt16LE(4))
        else
            cb("invalid response code #{response_code}; expected #{opcode}")

PAD = '                                                                                                      '

left = (text, width)->
    text = "#{text}"

    if text.length > width
        text.slice(0, width)
    else
        text + PAD.slice 0, width - text.length

right = (text, width)->
    text = "#{text}"

    if text.length > width
        text.slice(0, width)
    else
        PAD.slice( 0, width - text.length ) + text

execute = ( opcode, arg1, arg2 )->
    cb     = if arguments.length == 2 then arg1 else arg2
    params = if arguments.length == 2 then null else arg1

    control_point_callback = (data)->
        response_code = data.readUInt8()
        data = data.slice 1

        if response_code != opcode
            cb("invalid response code #{response_code}; expected #{opcode}")
        else
            parse_control_point_response response_code, data, cb

    buffer = new Buffer( [ opcode ] )
    buffer = Buffer.concat( [ buffer, params ] ) if params

    control_point_char.write buffer, false, (error)->
        if error
            cb( error )

device_address = ->
    if !options['device']
        console.log "device address (--device) reqired!"
        process.exit 1

    options['device']

start_address = ->
    if !options['address']
        console.log "start address (--address) required!"
        process.exit 1

    result = Number.parseInt options['address']

    if isNaN result
        console.log "argument for start address (--address #{options['address']}) is not a number"
        process.exit 1

    if result < 0
        console.log "argument for start address (--address #{options['address']}) is a negatic number"
        process.exit 1

    result

print_usage = ->
    console.log "usage: ble_flash [options]"
    console.log "options:"
    console.log "  --help, -h                   this help"
    console.log "  --address <addr>, -a <addr>  device start address if <input-file> is a binary file"
    console.log "  --device <mac>, -d <mac>     48 bit MAC address of the device to be flashed"
    console.log "  --version, -v                request version string from device"
    console.log "  --list, -l                   scan for a list of bootloaders"
    console.log "  --flash, -f <file>           flash the given file to the given address (--address)"
    console.log "  --verbose, -V                a lot of debug printouts"

calc_crc = ( data, start_address, size )->
    42

address_to_buffer = (address, address_size)->
    result = new Buffer( address_size )
    for pos in [0...address_size]
        result[ pos ] = address & 0xff
        address = address / 256

    result

upload_range = ( peripheral, start_address, data, address_size, page_size, page_buffers, cb )->
    console.log "upload #{data.length} starting at: #{start_address}..." if VERBOSE

    data_on_the_fly   = 5
    queued_data       = []

    send_function = ->
        while queued_data.length != 0 and data_on_the_fly != 0
            data_on_the_fly = data_on_the_fly - 1
            data = queued_data.shift()

            data_char.write data, true, (error)->
                data_on_the_fly = data_on_the_fly + 1

                if error
                    cb( error )
                else
                    send_function()

    flash_cb = {
        start_flash: ( start_address, cb ) ->
            execute OPC_START_FLASH, new Buffer( start_address ), (error, mtu, checksum)->
                console.log "flash started. mtu: #{mtu}; checksum: #{checksum}" if VERBOSE
                cb(error, mtu, checksum)

        send_data: ( data ) ->
            queued_data.push data
            send_function()

        register_progress_callback: ( progress ) ->
            progress_callback = ( data )->
                if data.length != 7
                    raise "invalid progress PDU size #{data.length}"

                progress( data.readUInt32LE(0), data.readUInt16LE(4), data.readUInt8(6) )

        flush: ( callback )->
            execute OPC_FLUSH, callback

        unregister_progress_callback: ->
            progress_callback = default_progress_callback
    }

    new FlashMemory( flash_cb, start_address, data, address_size, page_size, page_buffers, cb )

flash = ( file_name, start_address, peripheral, cb )->
    console.log "inquire bootloader buffer sizes..." if VERBOSE

    execute OPC_GET_SIZES, (error, address_size, page_size, page_buffers)->
        raise "#{device.address} #{version} Error: #{error}" if error

        console.log "address_size: #{address_size}; page_size: #{page_size}, page_buffers: #{page_buffers}" if VERBOSE

        fs.readFile file_name, (error, data)->
            raise "Error reading input file #{file_name}" if error

            upload_range peripheral, start_address, data, address_size, page_size, page_buffers, cb

try
    if options.help
        print_usage()
        process.exit 0

    if options.list
        console.log " mac               | version              | addr. size | page size | buffers "
        console.log "-----------------------------------------------------------------------------"

        print_line = (mac, version, address_size, page_size, page_buffer)->
            console.log " #{left mac, 17} | #{left version, 20} | #{right address_size, 10} | #{right page_size, 9} | #{right page_buffer, 7}"

        scan_devices null, (devices)->
            number_of_devices = Object.keys(devices).length

            if number_of_devices == 0
                console.log "no devices found."
                process.exit 0

            wait_for = number_of_devices
            stop_waiting = ->
                wait_for = wait_for - 1
                process.exit 0 if wait_for == 0

            for id, device of devices

                connect_device device, (peripheral, error)->
                    if error
                        console.log "#{device.address}: Error: #{error}"
                        stop_waiting()
                    else
                        execute OPC_GET_VERSION, (error, version)->
                            if error
                                console.log "#{device.address} Error: #{error}"
                                stop_waiting()
                            else
                                execute OPC_GET_SIZES, (error, address_size, page_size, page_buffer)->
                                    if error
                                        console.log "#{device.address} #{version} Error: #{error}"
                                    else
                                        print_line device.address, version, address_size, page_size, page_buffer
                                        peripheral.disconnect()
                                        stop_waiting()

    else if options.version
        scan_devices device_address(), (device)->
            raise "device not found!" if !device

            connect_device device, (peripheral, error)->
                raise "#{device.address}: Error: #{error}" if error

                execute OPC_GET_VERSION, (error, version)->
                    raise "error requesting version string: #{error}" if error

                    console.log version
                    process.exit 0

    else if options.flash
        console.log "start flashing #{options.flash} at 0x#{start_address().toString(16)} to device #{device_address()}"

        scan_devices device_address(), (device)->
            raise "device not found!" if !device

            console.log "device found..."

            connect_device device, (peripheral, error)->
                raise "connecting #{device.address}: Error: #{error}" if error

                console.log "device connected." if VERBOSE

                flash options.flash, start_address(), device, (error)->
                    raise "flashing device: Error: #{error}" if error

                    console.log "device successfully flashed"
                    process.exit 0


    else
        console.log "Unrecognized command."
        print_usage()
        process.exit 1

catch error
    console.log error
    process.exit 1
