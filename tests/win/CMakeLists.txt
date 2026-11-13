find_package(Catch2 3 CONFIG REQUIRED)

add_executable(CoreLibTests
        test_core_file.cpp
)

target_link_libraries(CoreLibTests PRIVATE
        CoreLib
        Catch2::Catch2WithMain
)

target_include_directories(CoreLibTests PRIVATE
        ${PROJECT_SOURCE_DIR}/core
)

include(Catch)

catch_discover_tests(CoreLibTests
        TEST_PREFIX "Windows: "
        PROPERTIES TIMEOUT 30
)
