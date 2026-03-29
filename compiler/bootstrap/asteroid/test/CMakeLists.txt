include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2
    GIT_TAG v3.13.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(Catch2)

function(yk_asteroid_add_test test_name)
    set(target yk_asteroid_${test_name}_test)

    add_executable(${target} ${test_name}.cpp)

    target_link_libraries(${target} PRIVATE yk::asteroid Catch2::Catch2WithMain)

    add_test(NAME ${target} COMMAND ${target})
endfunction()

yk_asteroid_add_test(preprocess_lexer)
yk_asteroid_add_test(sequence)
