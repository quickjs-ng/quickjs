project(tests_cpp
    VERSION 0.1.0
    LANGUAGES CXX)

add_executable(test_eval
    test_eval.cpp
)

target_link_libraries(test_eval
    PRIVATE
        qjs
)

add_custom_command(
    TARGET test_eval
    POST_BUILD
    COMMAND test_eval
)
