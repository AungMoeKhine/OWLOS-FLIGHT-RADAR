# portable core as an IDF component. tools/preview builds the same files on PC with MSVC
idf_component_register(SRCS "sky.c" "demo.c" "gfx.c" "font.c" "ui.c" "ui_scope.c" "ui_pages.c" "ui_wifi.c" "paper.c"
                            "ui_about.c" "qr_site.c" "map.c" "png_decode.c" "watch.c" "light.c"
                            "font_small.c" "font_medium.c" "font_large.c" "font_huge.c"
                       INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -ffast-math)
