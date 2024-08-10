file(GLOB pngs "*.png")

foreach (png ${pngs})
    execute_process(
            COMMAND ${BIN2S} -H ${png}.h ${png}
            OUTPUT_FILE ${png}.s
    )
endforeach ()

# Only one font here
execute_process(
        COMMAND ${BIN2S} -H noto_sans_jp_regular.otf.h noto_sans_jp_regular.otf
        OUTPUT_FILE noto_sans_jp_regular.otf.s
)

file(GLOB binaries "*.s")

foreach (binary ${binaries})
    string(REGEX REPLACE "\\.[^.]*$" "" binary_without_ext ${binary})
    execute_process(
            COMMAND ${BINARY_AS} -o ${binary_without_ext}.o ${binary}
    )
endforeach ()