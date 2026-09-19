if(NOT DEFINED PITIME_EXECUTABLE OR NOT EXISTS "${PITIME_EXECUTABLE}")
    message(FATAL_ERROR "PITIME_EXECUTABLE must name the built executable")
endif()
if(NOT DEFINED TEST_DIRECTORY)
    message(FATAL_ERROR "TEST_DIRECTORY must name a build-directory test folder")
endif()
file(MAKE_DIRECTORY "${TEST_DIRECTORY}")

function(run_success expected_output)
    execute_process(
        COMMAND "${PITIME_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 30
    )
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Command failed (${ARGN}): ${result}: ${error}")
    endif()
    string(REPLACE "\r\n" "\n" output "${output}")
    if(NOT "${output}" STREQUAL "${expected_output}")
        message(FATAL_ERROR "Unexpected stdout (${ARGN}): [${output}]")
    endif()
    if("${error}" STREQUAL "")
        message(FATAL_ERROR "Calculation must report timing on stderr (${ARGN})")
    endif()
endfunction()

function(run_failure)
    execute_process(
        COMMAND "${PITIME_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 30
    )
    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "Invalid command unexpectedly succeeded (${ARGN})")
    endif()
    if(NOT "${output}" STREQUAL "")
        message(FATAL_ERROR "Invalid command must not emit digits (${ARGN})")
    endif()
    if("${error}" STREQUAL "")
        message(FATAL_ERROR "Invalid command must explain the error (${ARGN})")
    endif()
endfunction()

run_success("3.\n" --digits 0 --threads 1)
run_success("3.1\n" --digits 1 --threads 2)
run_success("3.1415926535\n" --digits 10 --threads 0)
run_success("" --digits 10000 --quiet)

set(digits_file "${TEST_DIRECTORY}/pi digits.txt")
run_success("" --digits 20 --quiet --output "${digits_file}")
file(READ "${digits_file}" digits)
string(REPLACE "\r\n" "\n" digits "${digits}")
if(NOT "${digits}" STREQUAL "3.14159265358979323846\n")
    message(FATAL_ERROR "Quiet calculation wrote incorrect file contents: [${digits}]")
endif()
run_success("" --digits 10 --output "${digits_file}")
file(READ "${digits_file}" digits)
string(REPLACE "\r\n" "\n" digits "${digits}")
if(NOT "${digits}" STREQUAL "3.1415926535\n")
    message(FATAL_ERROR "Output file must be replaced with the requested digit count")
endif()

foreach(flag IN ITEMS --help --version)
    execute_process(
        COMMAND "${PITIME_EXECUTABLE}" "${flag}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 30
    )
    if(
        NOT "${result}" STREQUAL "0" OR "${output}" STREQUAL "" OR
        NOT "${error}" STREQUAL ""
    )
        message(FATAL_ERROR "${flag} must print information on stdout and exit successfully")
    endif()
endforeach()

execute_process(
    COMMAND "${PITIME_EXECUTABLE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 30
)
string(REPLACE "\r\n" "\n" output "${output}")
string(LENGTH "${output}" output_length)
if(
    NOT "${result}" STREQUAL "0" OR NOT output_length EQUAL 10003 OR
    NOT output MATCHES "^3\\.141592653589793238462643383279" OR
    "${error}" STREQUAL ""
)
    message(FATAL_ERROR "Default invocation must print 10000 digits and report timing")
endif()

foreach(value IN ITEMS -1 +1 1.5 10x abc 100000001 18446744073709551616)
    run_failure(--digits "${value}")
endforeach()
foreach(value IN ITEMS -1 +1 1.5 10x abc 257 4294967296)
    run_failure(--threads "${value}")
endforeach()
run_failure(--digits)
run_failure(--threads)
run_failure(--output)
run_failure(--unknown)
run_failure(10)
run_failure(--digits 10 --output "${TEST_DIRECTORY}")

message(STATUS "CLI formatting, default behavior, file output, and invalid arguments passed")
