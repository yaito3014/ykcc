include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2
    GIT_TAG v3.14.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(Catch2)

add_executable(test_dummy dummy.cpp)
target_link_libraries(test_dummy PRIVATE Catch2::Catch2WithMain yk::asteroid)
