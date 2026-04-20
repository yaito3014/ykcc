set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2
    GIT_TAG v3.13.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(Catch2)

set(CURRENT_DIR ${CMAKE_CURRENT_LIST_DIR})

function(yk_asteroid_add_test test_name)
    set(target yk_asteroid_${test_name}_test)

    add_executable(${target} ${test_name}.cpp)

    target_sources(
        ${target}
        PRIVATE
            FILE_SET ${target}_headers
                TYPE HEADERS
                BASE_DIRS ${CURRENT_DIR}
                FILES ${CURRENT_DIR}/test.hpp
    )

    target_link_libraries(${target} PRIVATE yk::asteroid Catch2::Catch2WithMain)

    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_subdirectory(parser)
add_subdirectory(preprocess)
