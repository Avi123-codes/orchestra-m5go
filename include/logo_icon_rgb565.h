#ifndef LOGO_ICON_RGB565_H
#define LOGO_ICON_RGB565_H

#include <stdint.h>

// Set these to your icon's actual size
#define LOGO_ICON_WIDTH   160
#define LOGO_ICON_HEIGHT  160

// RGB565 pixel data for the icon (row-major).
// Defined in a .c file: see note below.
extern const uint16_t logo_icon_rgb565[LOGO_ICON_WIDTH * LOGO_ICON_HEIGHT];

#endif // LOGO_ICON_RGB565_H
