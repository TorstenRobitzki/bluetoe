noble = require('noble')

noble.on 'discover', (peripheral)->
    console.log "peripheral discovered: #{peripheral}"

    if peripheral.id == 'c57821f91dd84fee81bc3f839557fecf'

        console.log 'connecting....'
        peripheral.connect (error)->
            if ( error )
                console.log "error connecting: #{error}"
            else
                console.log "connected.."

                peripheral.on 'disconnect', ->
                    console.log "disconnected!"
                    console.log "Scanning"
                    noble.startScanning()

noble.on 'stateChange', (state)->
    if state == 'poweredOn'
        console.log "Scanning"
        noble.startScanning()



