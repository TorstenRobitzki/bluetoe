options = require('minimist')(
    process.argv.slice(2), {
        boolean: ['help'],
        string: ['address', 'device'],
        alias: {
            'help': ['h'],
            'address': ['a'],
            'device' : ['d']
        }
        unknown: ( field )-> throw "Unrecognizes argument \"#{field}\""
    })

if options['help']
    console.log "usage: ble_flash [options] <input-file>"
    console.log "options:"
    console.log "  --help, -h                   this help"
    console.log "  --address, -a                device start address if <input-file> is a binary file"
    console.log "  --device, -d                 48 bit MAC address of the device to be flashed"
    process.exit 1

console.dir options

