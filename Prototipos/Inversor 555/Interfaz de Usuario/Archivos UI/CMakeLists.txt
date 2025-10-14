SET(SOURCES ui_Screen1.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_images_inversor_png.c
    fonts/ui_font_fontMontserratM14Tildes.c
    fonts/ui_font_fontMontserratM18Tildes.c
    fonts/ui_font_fuenteMontserratM16Tildes.c)

add_library(ui ${SOURCES})
