
class flash
    ###
     @peripheral ble abstraction expected to have following functions
       start_flash( start_address, callback(mtu, receive_capacity, checksum) )
       send_data( data )
       register_progress_callback(checksum, consecutive, mtu, receive_capacity)
     @start_address address to flash the data on the target
     @data Buffer containing the data to be flashed
     @address_size size of an address on the target in bytes
     @page_size size of flash page on the target in bytes
     @page_buffer number of page buffers within the targets bootloader
     @cb callback to be called, when the flash is
    ###
    constructor: (@peripheral, @start_address, @data, @address_size, @page_size, @page_buffer, @cb)->



