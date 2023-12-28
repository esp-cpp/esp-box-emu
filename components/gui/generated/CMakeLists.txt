SET(SOURCES screens/ui_romscreen.c
    screens/ui_settingsscreen.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c)

add_library(ui ${SOURCES})
