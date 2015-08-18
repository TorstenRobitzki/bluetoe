noble = require('noble')

noble.on 'discover', (peripheral)->
    console.log "peripheral discovered: #{peripheral}"

    if peripheral.id == 'c57821f91dd84fee81bc3f839557fecf'

        peripheral.on 'connect', ->
            console.log 'peripheral connected'

        console.log 'connecting....'
        peripheral.connect (error)->
            console.log "error connecting: #{error}"

noble.on 'stateChange', (state)->
    if state == 'poweredOn'
        console.log "Scanning"
        noble.startScanning()
    else
        console.log "Stop Scanning"
        noble.stopScanning()



