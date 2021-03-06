project(libbintail LANGUAGES CXX VERSION 0.1)

set(SOURCES
    include/bintail/bintail.hpp
    bintail.cc
    mvscn.cc
    elf.h
    elf.cc
    mvelem.h
    mvelem.cc)

add_library(libbintail ${SOURCES})

add_executable(tests ${SOURCES}
    main_test.cc
    elf_test.cc
    bintail_test.cc)

target_include_directories(libbintail PUBLIC
    include
    PRIVATE
    ${ELF_INCLUDE_DIRS} ${MULTIVERSE_INCLUDE_DIRS})

set_target_properties(libbintail PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)

target_link_libraries(libbintail ${ELF_LIBRARIES})

set_target_properties(tests PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)

target_link_libraries(tests
    Catch2::Catch2
    libbintail
)

install(TARGETS libbintail DESTINATION lib)

add_executable(bintail-cli
    main.cc
)

set_target_properties(bintail-cli PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
)

target_link_libraries(bintail-cli
    libbintail)

install(TARGETS bintail-cli DESTINATION bin)

#include(CTest)
#include(Catch)
#catch_discover_tests(tests)

