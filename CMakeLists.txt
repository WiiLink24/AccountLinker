cmake_minimum_required(VERSION 3.23)
project(DominosAccountLinker CXX)

include_directories(${LIBOGCDIR}/include)
include_directories(${DEVKITPRO}/portlibs/wii/include)
include_directories(${DEVKITPRO}/portlibs/ppc/include)
include_directories(${DEVKITPRO}/portlibs/ppc/include/freetype2)
include_directories(gui)
include_directories(data)
link_directories(${DEVKITPRO}/portlibs/wii/lib)
link_directories(${DEVKITPRO}/portlibs/ppc/lib)
link_directories(${DEVKITPRO}/libogc/lib/wii)

# Binary linker stuff
set(BIN2S ${DEVKITPRO}/tools/bin/bin2s)
set(BINARY_AS ${DEVKITPPC}/bin/powerpc-eabi-as)

set(MACHDEP "-DGEKKO -mrvl -mcpu=750 -meabi -mhard-float")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ASM_FLAGS "-x assembler-with-cpp")
set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-function -O2 ${CMAKE_CXX_FLAGS} ${MACHDEP}")
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-function -O2 ${CMAKE_CXX_FLAGS} ${MACHDEP} -fdiagnostics-color")
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

add_subdirectory(data)
add_subdirectory(gui)
add_executable(dominos-account-linker
        main.cpp
        menu.cpp
        menu.h
        config.cpp
        config.h
        utils.cpp
        utils.h
        qr.h
)

target_link_libraries(dominos-account-linker PRIVATE png gui `freetype-config --libs` wiiuse bte asnd ogc qrencode mbedcrypto z fat
        # It absolutely hates glob here so we have to manually set it ourselves
        ${CMAKE_CURRENT_SOURCE_DIR}/data/channel_gradient_bottom.png.o
        ${CMAKE_CURRENT_SOURCE_DIR}/data/channel_gradient_top.png.o
        ${CMAKE_CURRENT_SOURCE_DIR}/data/bg_stripes.png.o
)