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

yk_asteroid_add_test(sequence)
yk_asteroid_add_test(separated_by)
yk_asteroid_add_test(identifier)
yk_asteroid_add_test(pp_number)
yk_asteroid_add_test(alternative)
yk_asteroid_add_test(op_or_punc)
yk_asteroid_add_test(header_name)
yk_asteroid_add_test(char_literal)
yk_asteroid_add_test(string_literal)
yk_asteroid_add_test(line_splice)
yk_asteroid_add_test(lexer)
yk_asteroid_add_test(preprocessor)
