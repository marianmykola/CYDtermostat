#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

// Minimal memory settings
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (16U * 1024U)

// Display settings
#define LV_DISP_REFR_PERIOD 50
#define LV_DISP_DEF_REFR_PERIOD 50

// Input device settings
#define LV_INDEV_DEF_READ_PERIOD 50
#define LV_INDEV_DEF_DRAG_LIMIT 10

// Disable unused features
#define LV_USE_ARABIC_PERSIAN_CHARS 0
#define LV_USE_BIDI 0
#define LV_USE_FONT_SUBPX 0
#define LV_USE_ARC 0
#define LV_USE_BAR 0
#define LV_USE_SLIDER 0
#define LV_USE_SWITCH 0
#define LV_USE_TEXTAREA 0
#define LV_USE_IMG_CACHE 0
#define LV_USE_DOWNSCALE 0
#define LV_USE_OUTLINE 0

// Minimal logging
#define LV_USE_LOG 0

// Disable asserts for production
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ_INHERIT 0

// Minimal drawing
#define LV_USE_ANTIALIAS 0

// Disable file system
#define LV_USE_FS 0
#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_FATFS 0
#define LV_USE_FS_LITTLEFS 0

// Minimal fonts - only what we need
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_DEFAULT &lv_font_montserrat_24

// Disable themes
#define LV_USE_THEME_DEFAULT 0
#define LV_THEME_DEFAULT_DARK 0

// Disable layouts
#define LV_USE_FLEX 0
#define LV_USE_GRID 0

// Minimal widgets - only what we need
#define LV_USE_ARC 0
#define LV_USE_BAR 0
#define LV_USE_BTN 1
#define LV_USE_LABEL 1
#define LV_USE_SLIDER 0
#define LV_USE_SWITCH 0
#define LV_USE_TEXTAREA 0

// Disable animations
#define LV_USE_ANIMATION 0

// Disable extra widgets
#define LV_USE_EXTRA 0

// Disable monitors
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#endif /* LV_CONF_H */
