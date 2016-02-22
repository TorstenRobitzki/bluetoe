noble   = require 'noble'
util    = require('util');
options = require('minimist')(
    process.argv.slice(2), {
        boolean: ['help', 'version', 'list'],
        string: ['address', 'device'],
        alias: {
            'help': ['h'],
            'address': ['a'],
            'device' : ['d'],
            'version': ['v'],
            'list'   : ['l']
        }
        unknown: ( field )-> throw "Unrecognizes argument \"#{field}\""
    })

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
    throw "Unexpected control point notification"

progress_callback      = (data, is_notification)->
    throw "Unexpected progress notification"

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

                    progress_char.on 'data,', (data, is_notification)->
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
            throw "while parsing response (code=#{response_code}), the response expected to be #{size} in size, but was #{data.length}"

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
            expected_size 9
            cb(null, data.readUInt8(0), data.readUInt32LE(1), data.readUInt32LE(5))
        when OPC_STOP_FLASH, OPC_FLUSH
            expected_size 0
            cb(null)
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

execute = ( opcode, cb )->
    control_point_callback = (data)->
        response_code = data.readUInt8()
        data = data.slice 1

        if response_code != opcode
            cb("invalid response code #{response_code}; expected #{opcode}")
        else
            parse_control_point_response response_code, data, cb

    buffer = new Buffer( [ opcode ] )
    control_point_char.write buffer, false, (error)->
        if error
            cb( error )

device_address = ->
    if !options['device']
        console.log "device address (--device) reqired!"
        process.exit 1

    options['device']

print_usage = ->
    console.log "usage: ble_flash [options] <input-file>"
    console.log "options:"
    console.log "  --help, -h                   this help"
    console.log "  --address <addr>, -a <addr>  device start address if <input-file> is a binary file"
    console.log "  --device <mac>, -d <mac>     48 bit MAC address of the device to be flashed"
    console.log "  --version, -v                request version string from device"
    console.log "  --list, -l                   scan for a list of bootloaders"

if options.help
    print_usage()
    process.exit 0

if options.list
    console.log " mac              | version              | addr. size | page size | buffers "
    console.log "----------------------------------------------------------------------------"

    print_line = (mac, version, address_size, page_size, page_buffer)->
        console.log " #{left mac, 16} | #{left version, 20} | #{right address_size, 10} | #{right page_size, 9} | #{right page_buffer, 7}"

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
        if device
            connect_device device, (peripheral, error)->
                if error
                    console.log "#{device.address}: Error: #{error}"
                    process.exit 1
                else
                    execute OPC_GET_VERSION, (error, version)->
                        if error
                            console.log "error requesting version string: #{error}"
                            process.exit 1
                        else
                            console.log version
                            process.exit 0
        else
            console.log "device not found!"
            process.exit 1

else
    console.log "Unrecognized command."
    print_usage()
    process.exit 1

# if options.version
#     connect ->

