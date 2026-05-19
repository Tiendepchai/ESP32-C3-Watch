#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
    int width;
    int height;
} text_renderer_box_t;

typedef enum {
    NAV_ICON_NONE = 0,
    NAV_ICON_LEFT,
    NAV_ICON_RIGHT,
    NAV_ICON_STRAIGHT,
    NAV_ICON_UTURN,
} nav_icon_t;

void text_renderer_clear(uint8_t *framebuffer, size_t framebuffer_len, bool on);
bool text_renderer_can_render_codepoint(uint32_t codepoint);
void text_renderer_draw_string(uint8_t *framebuffer, int fb_width, int fb_height, int x, int y, const char *text);
void text_renderer_draw_wrapped_text(uint8_t *framebuffer, int fb_width, int fb_height,
                                     const text_renderer_box_t *box, const char *text, bool add_ellipsis);
void text_renderer_draw_nav_icon(uint8_t *framebuffer, int fb_width, int fb_height,
                                 int x, int y, nav_icon_t icon);

#ifdef __cplusplus
}
#endif
