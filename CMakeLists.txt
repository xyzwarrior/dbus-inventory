cmake_minimum_required(VERSION 3.16)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

option(BUILD_YOCTO "Use system-provided libraries" OFF)

if (NOT BUILD_YOCTO)
    include(FetchContent)
    include(ExternalProject)
    include("cmake/dependencies.cmake")
    FetchContent_MakeAvailable(sdbus-c++)
else ()
    add_definitions(-DPIM_PERSIST_PATH="\\\"/var/lib/phosphor-inventory-manager\\\"")
endif()

project(dbus-inventory LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Disables GNU extensions
find_package(sdbus-c++ 2.0 REQUIRED)
find_package(cereal REQUIRED)

set(INTERFACE_LIST
    xyz.openbmc_project.Common.UUID
    xyz.openbmc_project.Inventory.Item
    xyz.openbmc_project.Inventory.Item.System
    xyz.openbmc_project.Inventory.Item.Chassis
    xyz.openbmc_project.Inventory.Item.Bmc
    xyz.openbmc_project.Inventory.Item.Board
    xyz.openbmc_project.Inventory.Item.Ethernet
    xyz.openbmc_project.Inventory.Item.NetworkInterface
    xyz.openbmc_project.Inventory.Decorator.Asset
    xyz.openbmc_project.Inventory.Decorator.AssetTag
    xyz.openbmc_project.Inventory.Decorator.Cacheable
    xyz.openbmc_project.Inventory.Decorator.Replaceable
    xyz.openbmc_project.Inventory.Decorator.Revision
)

set(INTERFACES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(INTERFACES_SRC "")
foreach(IFACE IN LISTS INTERFACE_LIST)
    message(STATUS "Interface: ${IFACE}")
    add_custom_command(
        COMMAND ${CMAKE_COMMAND} -E make_directory "${INTERFACES_DIR}"
        OUTPUT "${INTERFACES_DIR}/${IFACE}.hpp"
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/sdbus++
            -r ${CMAKE_CURRENT_SOURCE_DIR}/tools/yaml
            -t ${CMAKE_CURRENT_SOURCE_DIR}/tools/sdbusplus/templates interface server-header
            ${IFACE} > "${INTERFACES_DIR}/${IFACE}.hpp"
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/sdbus++
        COMMENT "Generating interface hpp for ${IFACE}"
        VERBATIM
    )
    add_custom_command(
        COMMAND ${CMAKE_COMMAND} -E make_directory "${INTERFACES_DIR}"
        OUTPUT "${INTERFACES_DIR}/${IFACE}.cpp"
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/sdbus++ 
            -r ${CMAKE_CURRENT_SOURCE_DIR}/tools/yaml
            -t ${CMAKE_CURRENT_SOURCE_DIR}/tools/sdbusplus/templates interface server-cpp
            ${IFACE} > "${INTERFACES_DIR}/${IFACE}.cpp"
        DEPENDS "${INTERFACES_DIR}/${IFACE}.hpp"
        COMMENT "Generating interface cpp for ${IFACE}"
        VERBATIM
    )
    list(APPEND INTERFACES_SRC "${INTERFACES_DIR}/${IFACE}.cpp")
endforeach()
add_custom_command(
    OUTPUT ${INTERFACES_DIR}/maker.cpp
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/maker.py "${INTERFACE_LIST}" > ${INTERFACES_DIR}/maker.cpp
    COMMENT "Generating maker cpp"
    VERBATIM
)
add_executable(dbus-inventory 
    server.cpp
    ${INTERFACES_SRC}
    ${INTERFACES_DIR}/maker.cpp
)
target_include_directories(dbus-inventory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${INTERFACES_DIR})
#target_link_libraries(dbus-inventory PRIVATE SDBusCpp::sdbus-c++)
target_link_libraries(dbus-inventory PRIVATE sdbus-c++)
target_compile_features(dbus-inventory PRIVATE cxx_std_17)
target_compile_options(dbus-inventory PRIVATE -Wall)

install(TARGETS dbus-inventory DESTINATION ${CMAKE_INSTALL_BINDIR})
