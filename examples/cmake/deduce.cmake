set(board_controller_mapping
    PCA10056 NRF52840
    PCA10040 NRF52832)

# Map given in the form "key1 value1 key2 value2..."
function(map_lookup result_var map key)
    list(FIND ${map} ${key} index)
    math(EXPR next_index "${index} | 1")

    if ( index EQUAL -1 OR next_index EQUAL index )
        set(${result_var}-NOTFOUND NOTFOUND PARENT_SCOPE)
    else()
        list(GET ${map} ${next_index} output)
        set(${result_var} ${output} PARENT_SCOPE)
    endif()
endfunction()

function(deduce_binding result_var board)
    map_lookup(result board_controller_mapping ${board})

    if (DEFINED result-NOTFOUND)
        message(FATAL_ERROR "Unknow board: ${board}")
    endif()
    set(${result_var} ${result} PARENT_SCOPE)
endfunction()

function(deduce_bluetoe_binding result_var controller)
    set(${result_var} nrf52 PARENT_SCOPE)
endfunction()

function(set_preprocessore_macros)
endfunction()