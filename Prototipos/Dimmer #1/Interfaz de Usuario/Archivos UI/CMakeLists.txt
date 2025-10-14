SET(SOURCES ui_Screen1.c
    ui_Screen2.c
    ui_Screen3.c
    ui_Screen4.c
    ui_Screen5.c
    ui_Screen6.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    ui_img_images_dimmer_png.c
    ui_img_images_arrow_png.c
    fonts/ui_font_fontRobotoBold14Tildes.c
    fonts/ui_font_fontRobotoBold16Tildes.c
    fonts/ui_font_fontRobotoBold18Tildes.c
    fonts/ui_font_fontRobotoMedium14Tildes.c
    fonts/ui_font_fontRobotoMedium16Tildes.c
    fonts/ui_font_fontRobotoMedium18Tildes.c
    fonts/ui_font_fontRobotoRegular14Tildes.c
    fonts/ui_font_fontRobotoRegular16Tildes.c
    fonts/ui_font_fontRobotoRegular18Tildes.c)

add_library(ui ${SOURCES})
