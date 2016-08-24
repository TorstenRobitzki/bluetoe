exports.buf = (()->
    crcTable = (()->
        crcTable = []

        for n in [ 0...256 ]
            c = n

            for k in [ 0...8 ]
                c = if ( c & 1 ) != 0 then (0xEDB88320 ^ (c >>> 1)) else (c >>> 1)

            crcTable[n] = c

        crcTable
    )()

    ( data, crc = 0 )->
        crc = crc ^ (-1)

        for i in [ 0...data.length ]
            crc = (crc >>> 8) ^ crcTable[ ( crc ^ data[ i ] ) & 0xFF]

        result = (crc ^ (-1))

        if result < 0 then 0x100000000 + result else result
)()