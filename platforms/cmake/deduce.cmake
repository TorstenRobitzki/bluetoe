set(board_controller_mapping
    PCA10056 NRF52840
    PCA10040 NRF52832
    LP_EM_CC2340R5 CC2340R5)

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

# the platform directory and the binding target of a part: nrf52 for the Nordic parts, cc23x0
# for the TI CC2340 family
function(deduce_bluetoe_binding result_var controller)
    if (controller MATCHES "^CC23")
        set(${result_var} cc23x0 PARENT_SCOPE)
    else()
        set(${result_var} nrf52 PARENT_SCOPE)
    endif()
endfunction()

function(set_preprocessore_macros)
endfunction()