# Helper: runs "npm run build" in the frontend directory.
# Invoked by CMake's add_custom_command.  Using execute_process here
# guarantees WORKING_DIRECTORY works regardless of the CMake generator.
#
# Expects -DFRONTEND_DIR=<path> -DNPM_EXE=<path>

if(NOT FRONTEND_DIR)
    message(FATAL_ERROR "FRONTEND_DIR not set")
endif()
if(NOT NPM_EXE)
    message(FATAL_ERROR "NPM_EXE not set")
endif()

message(STATUS "npm run build in ${FRONTEND_DIR} …")

execute_process(
    COMMAND "${NPM_EXE}" run build
    WORKING_DIRECTORY "${FRONTEND_DIR}"
    RESULT_VARIABLE npm_result
    OUTPUT_VARIABLE npm_out
    ERROR_VARIABLE  npm_err
)

if(NOT npm_result EQUAL 0)
    if(npm_out)
        message("${npm_out}")
    endif()
    if(npm_err)
        message("${npm_err}")
    endif()
    message(FATAL_ERROR "npm run build failed (exit code ${npm_result})")
endif()

# Touch stamp so CMake knows the frontend is fresh
file(TOUCH "${FRONTEND_DIR}/dist/.stamp")
