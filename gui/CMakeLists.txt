file(GLOB gui_src
        "*.cpp"
        "*.h"
)

add_library(gui
    ${gui_src}
)

target_link_libraries(gui)