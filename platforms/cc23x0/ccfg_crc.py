#!/usr/bin/env python3
"""
The checksums of the CC23xx's CCFG block, patched into a linked ELF file.

The linker script places symbols <prefix>_<region>_begin and <prefix>_<region>_end around
every region of the block the boot ROM checks, and the four bytes after a region's end hold
its CRC-32 (the zlib one, little endian). The part locks its debug port on a block that fails
the check, so every image is patched before it is turned into a hex file. This is what the
SDK's crc_tool does, without its dependencies: a plain ELF32 parser and zlib.

    ccfg_crc.py <elf> <symbol prefix>
"""
import struct
import sys
import zlib


def sections_and_symbols( image ):
    # the ELF32 header: e_shoff at 32, e_shentsize, e_shnum and e_shstrndx from 46
    ( e_shoff, ) = struct.unpack_from( '<I', image, 32 )
    ( e_shentsize, e_shnum, e_shstrndx ) = struct.unpack_from( '<HHH', image, 46 )

    sections = []
    for i in range( e_shnum ):
        ( sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize ) = \
            struct.unpack_from( '<IIIIIIIIII', image, e_shoff + i * e_shentsize )
        sections.append( dict( type = sh_type, addr = sh_addr, offset = sh_offset, size = sh_size, link = sh_link, entsize = sh_entsize ) )

    symbols = {}
    for section in sections:
        if section[ 'type' ] != 2:   # SHT_SYMTAB
            continue

        strings = sections[ section[ 'link' ] ]
        for i in range( section[ 'size' ] // 16 ):
            ( st_name, st_value ) = struct.unpack_from( '<II', image, section[ 'offset' ] + i * 16 )
            end  = image.index( b'\0', strings[ 'offset' ] + st_name )
            name = image[ strings[ 'offset' ] + st_name : end ].decode()
            symbols[ name ] = st_value

    return sections, symbols


def file_offset( sections, address, size ):
    for section in sections:
        if section[ 'type' ] == 1 and section[ 'addr' ] <= address and address + size <= section[ 'addr' ] + section[ 'size' ]:
            return section[ 'offset' ] + address - section[ 'addr' ]

    raise SystemExit( f'no loaded section holds 0x{address:08x}..0x{address + size:08x}' )


def main( path, prefix ):
    image = bytearray( open( path, 'rb' ).read() )
    sections, symbols = sections_and_symbols( image )

    regions = sorted( name[ : -len( '_begin' ) ] for name in symbols if name.startswith( prefix ) and name.endswith( '_begin' ) )

    for region in regions:
        begin = symbols[ region + '_begin' ]
        end   = symbols[ region + '_end' ]
        crc_at = end + 1

        start = file_offset( sections, begin, crc_at + 4 - begin )
        crc   = zlib.crc32( bytes( image[ start : start + crc_at - begin ] ) )
        image[ start + crc_at - begin : start + crc_at - begin + 4 ] = struct.pack( '<I', crc )

        print( f'{region}: 0x{begin:08x}..0x{end:08x} crc32 0x{crc:08x} at 0x{crc_at:08x}' )

    open( path, 'wb' ).write( image )


if __name__ == '__main__':
    if len( sys.argv ) != 3:
        raise SystemExit( __doc__ )

    main( sys.argv[ 1 ], sys.argv[ 2 ] )
