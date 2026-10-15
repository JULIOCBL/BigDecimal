cmake_minimum_required(VERSION 3.16)
project(BigDecimal VERSION 1.0.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(BIGDECIMAL_BUILD_TESTS "Build native contract tests" OFF)
option(BIGDECIMAL_BUILD_EXAMPLE "Build native example" OFF)
option(BIGDECIMAL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(bigdecimal
    src/BigDecimal.cpp src/BigDecimalMath.cpp src/BigDecimalConversions.cpp
    src/BigInteger.cpp src/BigDecimalSerialization.cpp)
add_library(BigDecimal::BigDecimal ALIAS bigdecimal)
target_compile_features(bigdecimal PUBLIC cxx_std_17)
target_include_directories(bigdecimal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/bigdecimal>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/bigdecimal>)
set_target_properties(bigdecimal PROPERTIES CXX_EXTENSIONS OFF EXPORT_NAME BigDecimal
    VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(bigdecimal PRIVATE -Wall -Wextra -Wpedantic)
    if(BIGDECIMAL_WARNINGS_AS_ERRORS)
        target_compile_options(bigdecimal PRIVATE -Werror)
    endif()
endif()
if(BIGDECIMAL_BUILD_TESTS)
    enable_testing()
endif()
if(BIGDECIMAL_BUILD_TESTS)
    add_executable(bigdecimal_api_tests tests/api.cpp)
    target_link_libraries(bigdecimal_api_tests PRIVATE BigDecimal::BigDecimal)
    add_test(NAME bigdecimal_api COMMAND bigdecimal_api_tests)
    add_executable(bigdecimal_serialization_tests tests/serialization_api.cpp)
    target_link_libraries(bigdecimal_serialization_tests PRIVATE BigDecimal::BigDecimal)
    add_test(NAME bigdecimal_serialization COMMAND bigdecimal_serialization_tests)
    if(MSVC)
        target_compile_options(bigdecimal_api_tests PRIVATE /UNDEBUG)
        target_compile_options(bigdecimal_serialization_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(bigdecimal_api_tests PRIVATE -UNDEBUG)
        target_compile_options(bigdecimal_serialization_tests PRIVATE -UNDEBUG)
    endif()
endif()
if(BIGDECIMAL_BUILD_EXAMPLE)
    add_executable(bigdecimal_example examples/basic.cpp)
    target_link_libraries(bigdecimal_example PRIVATE BigDecimal::BigDecimal)
endif()
install(TARGETS bigdecimal EXPORT BigDecimalTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/bigdecimal DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT BigDecimalTargets NAMESPACE BigDecimal:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BigDecimal)
configure_package_config_file(cmake/BigDecimalConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/BigDecimalConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BigDecimal)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/BigDecimalConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BigDecimalConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/BigDecimalConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BigDecimal)
install(FILES LICENSE NOTICE README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
