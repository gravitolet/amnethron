# Release
if (NOT DEFINED NKR_VERSION OR NKR_VERSION STREQUAL "")
    set(NKR_VERSION "$ENV{INPUT_VERSION}")
endif ()
if (NKR_VERSION STREQUAL "")
    set(NKR_VERSION "0.1.0")
endif ()
add_compile_definitions(NKR_VERSION=\"${NKR_VERSION}\")

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DNKR_CPP_DEBUG")

# Func
function(nkr_add_compile_definitions arg)
    message("[add_compile_definitions] ${ARGV}")
    add_compile_definitions(${ARGV})
endfunction()
