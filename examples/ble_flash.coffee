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
            devices[ peripheral.id ] = peripheral

    noble.on 'stateChange', ( state )->

        if state == 'poweredOn'
            timer = setTimeout (()->
                    noble.stopScanning()
                    cb(devices)
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

execute = ( opcode, cb )->
    control_point_callback = (data)->
        cb(data, null)

    buffer = new Buffer( [ opcode ] )
    control_point_char.write buffer, false, (error)->
        if error
            cb( null, error )

if options.help
    console.log "usage: ble_flash [options] <input-file>"
    console.log "options:"
    console.log "  --help, -h                   this help"
    console.log "  --address <addr>, -a <addr>  device start address if <input-file> is a binary file"
    console.log "  --device <mac>, -d <mac>     48 bit MAC address of the device to be flashed"
    console.log "  --version, -v                request version string from device"
    console.log "  --list, -l                   scan for a list of bootloaders"
    process.exit 0

if options.list
    scan_devices [], (devices)->
        if Object.keys(devices).length == 0
            console.log "no devices found."
            process.exit 0

        wait_for = 0
        stop_waiting = ->
            wait_for = wait_for - 1
            process.exit 0 if wait_for == 0

        for id, device of devices
            wait_for = wait_for + 1

            connect_device device, (peripheral, error)->
                if error
                    console.log "#{device.address}: Error: #{error}"
                    stop_waiting()
                else
                    execute OPC_GET_VERSION, (version, error)->
                        if error
                            console.log "#{device.address} Error: #{error}"
                            stop_waiting()
                        else
                            console.log "#{device.address} #{version}"
                            peripheral.disconnect()
                            stop_waiting()

# if !options['address']
#     console.log "device address reqired!"
#     process.exit 1

# if options.version
#     connect ->

