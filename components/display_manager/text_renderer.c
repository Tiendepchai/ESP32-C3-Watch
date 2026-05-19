#include "text_renderer.h"

#include <string.h>

static const uint8_t s_font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x10,0x08,0x08,0x10,0x08}
};

#define FONT_WIDTH             5
#define FONT_HEIGHT            7
#define FONT_ADVANCE           6
#define LINE_HEIGHT            8
#define LINE_BUFFER_BYTES      128

typedef enum {
    TEXT_RENDERER_BASE_MARK_NONE = 0,
    TEXT_RENDERER_BASE_MARK_BREVE,
    TEXT_RENDERER_BASE_MARK_CIRCUMFLEX,
    TEXT_RENDERER_BASE_MARK_HORN,
    TEXT_RENDERER_BASE_MARK_STROKE,
} text_renderer_base_mark_t;

typedef enum {
    TEXT_RENDERER_TONE_NONE = 0,
    TEXT_RENDERER_TONE_GRAVE,
    TEXT_RENDERER_TONE_ACUTE,
    TEXT_RENDERER_TONE_HOOK,
    TEXT_RENDERER_TONE_TILDE,
    TEXT_RENDERER_TONE_DOT,
} text_renderer_tone_t;

typedef struct {
    char base_char;
    text_renderer_base_mark_t base_mark;
    text_renderer_tone_t tone;
} text_renderer_vietnamese_glyph_t;

typedef struct {
    char base_char;
    text_renderer_base_mark_t base_mark;
    uint32_t forms[6];
} text_renderer_vietnamese_group_t;

static const text_renderer_vietnamese_group_t s_vietnamese_groups[] = {
    { 'A', TEXT_RENDERER_BASE_MARK_NONE,       {0x0041, 0x00C0, 0x00C1, 0x1EA2, 0x00C3, 0x1EA0} },
    { 'a', TEXT_RENDERER_BASE_MARK_NONE,       {0x0061, 0x00E0, 0x00E1, 0x1EA3, 0x00E3, 0x1EA1} },
    { 'A', TEXT_RENDERER_BASE_MARK_BREVE,      {0x0102, 0x1EB0, 0x1EAE, 0x1EB2, 0x1EB4, 0x1EB6} },
    { 'a', TEXT_RENDERER_BASE_MARK_BREVE,      {0x0103, 0x1EB1, 0x1EAF, 0x1EB3, 0x1EB5, 0x1EB7} },
    { 'A', TEXT_RENDERER_BASE_MARK_CIRCUMFLEX, {0x00C2, 0x1EA6, 0x1EA4, 0x1EA8, 0x1EAA, 0x1EAC} },
    { 'a', TEXT_RENDERER_BASE_MARK_CIRCUMFLEX, {0x00E2, 0x1EA7, 0x1EA5, 0x1EA9, 0x1EAB, 0x1EAD} },
    { 'E', TEXT_RENDERER_BASE_MARK_NONE,       {0x0045, 0x00C8, 0x00C9, 0x1EBA, 0x1EBC, 0x1EB8} },
    { 'e', TEXT_RENDERER_BASE_MARK_NONE,       {0x0065, 0x00E8, 0x00E9, 0x1EBB, 0x1EBD, 0x1EB9} },
    { 'E', TEXT_RENDERER_BASE_MARK_CIRCUMFLEX, {0x00CA, 0x1EC0, 0x1EBE, 0x1EC2, 0x1EC4, 0x1EC6} },
    { 'e', TEXT_RENDERER_BASE_MARK_CIRCUMFLEX, {0x00EA, 0x1EC1, 0x1EBF, 0x1EC3, 0x1EC5, 0x1EC7} },
    { 'I', TEXT_RENDERER_BASE_MARK_NONE,       {0x0049, 0x00CC, 0x00CD, 0x1EC8, 0x0128, 0x1ECA} },
    { 'i', TEXT_RENDERER_BASE_MARK_NONE,       {0x0069, 0x00EC, 0x00ED, 0x1EC9, 0x0129, 0x1ECB} },
    { 'O', TEXT_RENDERER_BASE_MARK_NONE,       {0x004F, 0x00D2, 0x00D3, 0x1ECE, 0x00D5, 0x1ECC} },
    { 'o', TEXT_RENDERER_BASE_MARK_NONE,       {0x006F, 0x00F2, 0x00F3, 0x1ECF, 0x00F5, 0x1ECD} },
    { 'O', TEXT_RENDERER_BASE_MARK_CIRCUMFLEX, {0x00D4, 0x1ED2, 0x1ED0, 0x1ED4, 0x1ED6, 0x1ED8} },
    { 'o', TEXT_RENDERER_BASE_MARK_CIRCUMFLEX, {0x00F4, 0x1ED3, 0x1ED1, 0x1ED5, 0x1ED7, 0x1ED9} },
    { 'O', TEXT_RENDERER_BASE_MARK_HORN,       {0x01A0, 0x1EDC, 0x1EDA, 0x1EDE, 0x1EE0, 0x1EE2} },
    { 'o', TEXT_RENDERER_BASE_MARK_HORN,       {0x01A1, 0x1EDD, 0x1EDB, 0x1EDF, 0x1EE1, 0x1EE3} },
    { 'U', TEXT_RENDERER_BASE_MARK_NONE,       {0x0055, 0x00D9, 0x00DA, 0x1EE6, 0x0168, 0x1EE4} },
    { 'u', TEXT_RENDERER_BASE_MARK_NONE,       {0x0075, 0x00F9, 0x00FA, 0x1EE7, 0x0169, 0x1EE5} },
    { 'U', TEXT_RENDERER_BASE_MARK_HORN,       {0x01AF, 0x1EEA, 0x1EE8, 0x1EEC, 0x1EEE, 0x1EF0} },
    { 'u', TEXT_RENDERER_BASE_MARK_HORN,       {0x01B0, 0x1EEB, 0x1EE9, 0x1EED, 0x1EEF, 0x1EF1} },
    { 'Y', TEXT_RENDERER_BASE_MARK_NONE,       {0x0059, 0x1EF2, 0x00DD, 0x1EF6, 0x1EF8, 0x1EF4} },
    { 'y', TEXT_RENDERER_BASE_MARK_NONE,       {0x0079, 0x1EF3, 0x00FD, 0x1EF7, 0x1EF9, 0x1EF5} },
};

static size_t text_renderer_utf8_sequence_len(uint8_t lead)
{
    if ((lead & 0x80U) == 0U) {
        return 1;
    }
    if ((lead & 0xE0U) == 0xC0U) {
        return 2;
    }
    if ((lead & 0xF0U) == 0xE0U) {
        return 3;
    }
    if ((lead & 0xF8U) == 0xF0U) {
        return 4;
    }
    return 1;
}

static bool text_renderer_decode_utf8_char(const uint8_t *in, size_t *consumed, uint32_t *codepoint)
{
    size_t len;

    if (in == NULL || consumed == NULL || codepoint == NULL || in[0] == 0U) {
        return false;
    }

    len = text_renderer_utf8_sequence_len(in[0]);
    *consumed = len;

    switch (len) {
    case 1:
        *codepoint = in[0];
        return true;
    case 2:
        if ((in[1] & 0xC0U) != 0x80U) {
            *codepoint = in[0];
            *consumed = 1;
            return false;
        }
        *codepoint = ((uint32_t)(in[0] & 0x1FU) << 6) |
                     ((uint32_t)(in[1] & 0x3FU));
        return true;
    case 3:
        if ((in[1] & 0xC0U) != 0x80U || (in[2] & 0xC0U) != 0x80U) {
            *codepoint = in[0];
            *consumed = 1;
            return false;
        }
        *codepoint = ((uint32_t)(in[0] & 0x0FU) << 12) |
                     ((uint32_t)(in[1] & 0x3FU) << 6) |
                     ((uint32_t)(in[2] & 0x3FU));
        return true;
    case 4:
        if ((in[1] & 0xC0U) != 0x80U || (in[2] & 0xC0U) != 0x80U || (in[3] & 0xC0U) != 0x80U) {
            *codepoint = in[0];
            *consumed = 1;
            return false;
        }
        *codepoint = ((uint32_t)(in[0] & 0x07U) << 18) |
                     ((uint32_t)(in[1] & 0x3FU) << 12) |
                     ((uint32_t)(in[2] & 0x3FU) << 6) |
                     ((uint32_t)(in[3] & 0x3FU));
        return true;
    default:
        *codepoint = in[0];
        *consumed = 1;
        return false;
    }
}

static bool text_renderer_is_combining_mark(uint32_t codepoint)
{
    return codepoint >= 0x0300U && codepoint <= 0x036FU;
}

static bool text_renderer_decompose_vietnamese(uint32_t codepoint, text_renderer_vietnamese_glyph_t *out_glyph)
{
    if (out_glyph == NULL) {
        return false;
    }

    if (codepoint == 0x0110U) {
        out_glyph->base_char = 'D';
        out_glyph->base_mark = TEXT_RENDERER_BASE_MARK_STROKE;
        out_glyph->tone = TEXT_RENDERER_TONE_NONE;
        return true;
    }
    if (codepoint == 0x0111U) {
        out_glyph->base_char = 'd';
        out_glyph->base_mark = TEXT_RENDERER_BASE_MARK_STROKE;
        out_glyph->tone = TEXT_RENDERER_TONE_NONE;
        return true;
    }

    for (size_t i = 0; i < sizeof(s_vietnamese_groups) / sizeof(s_vietnamese_groups[0]); ++i) {
        for (size_t tone_index = 0; tone_index < 6U; ++tone_index) {
            if (s_vietnamese_groups[i].forms[tone_index] != codepoint) {
                continue;
            }

            out_glyph->base_char = s_vietnamese_groups[i].base_char;
            out_glyph->base_mark = s_vietnamese_groups[i].base_mark;
            out_glyph->tone = (text_renderer_tone_t)tone_index;
            return true;
        }
    }

    return false;
}

static void text_renderer_draw_pixel(uint8_t *framebuffer, int fb_width, int fb_height, int x, int y, bool on)
{
    size_t index;
    uint8_t bit;

    if (framebuffer == NULL || x < 0 || y < 0 || x >= fb_width || y >= fb_height) {
        return;
    }

    index = (size_t)x + ((size_t)y / 8U) * (size_t)fb_width;
    bit = (uint8_t)(1U << (y & 0x7));

    if (on) {
        framebuffer[index] |= bit;
    } else {
        framebuffer[index] &= (uint8_t)~bit;
    }
}

static void text_renderer_draw_hline(uint8_t *framebuffer, int fb_width, int fb_height,
                                     int x0, int x1, int y)
{
    if (x1 < x0) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;
    }

    for (int x = x0; x <= x1; ++x) {
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x, y, true);
    }
}

static void text_renderer_draw_ascii_char(uint8_t *framebuffer, int fb_width, int fb_height, int x, int y, char c)
{
    const uint8_t *glyph;

    if ((unsigned char)c < 32U || (unsigned char)c > 126U) {
        c = '?';
    }

    glyph = s_font5x7[(unsigned char)c - 32U];
    for (int col = 0; col < FONT_WIDTH; ++col) {
        for (int row = 0; row < FONT_HEIGHT; ++row) {
            bool on = ((glyph[col] >> row) & 0x01U) != 0U;
            if (on) {
                text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + col, y + row, true);
            }
        }
    }
}

static void text_renderer_draw_base_mark(uint8_t *framebuffer, int fb_width, int fb_height,
                                         int x, int y, text_renderer_base_mark_t base_mark)
{
    switch (base_mark) {
    case TEXT_RENDERER_BASE_MARK_BREVE:
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 1, y + 0, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 2, y + 1, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 3, y + 0, true);
        break;
    case TEXT_RENDERER_BASE_MARK_CIRCUMFLEX:
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 1, y + 1, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 2, y + 0, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 3, y + 1, true);
        break;
    case TEXT_RENDERER_BASE_MARK_HORN:
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 4, y + 0, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 5, y + 1, true);
        break;
    case TEXT_RENDERER_BASE_MARK_STROKE:
        text_renderer_draw_hline(framebuffer, fb_width, fb_height, x + 1, x + 4, y + 3);
        break;
    case TEXT_RENDERER_BASE_MARK_NONE:
    default:
        break;
    }
}

static void text_renderer_draw_tone_mark(uint8_t *framebuffer, int fb_width, int fb_height,
                                         int x, int y, text_renderer_tone_t tone)
{
    switch (tone) {
    case TEXT_RENDERER_TONE_GRAVE:
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 1, y + 0, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 2, y + 1, true);
        break;
    case TEXT_RENDERER_TONE_ACUTE:
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 4, y + 0, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 3, y + 1, true);
        break;
    case TEXT_RENDERER_TONE_HOOK:
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 3, y + 0, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 4, y + 1, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 3, y + 2, true);
        break;
    case TEXT_RENDERER_TONE_TILDE:
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 0, y + 1, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 1, y + 0, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 2, y + 1, true);
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 3, y + 0, true);
        break;
    case TEXT_RENDERER_TONE_DOT:
        text_renderer_draw_pixel(framebuffer, fb_width, fb_height, x + 2, y + 7, true);
        break;
    case TEXT_RENDERER_TONE_NONE:
    default:
        break;
    }
}

static void text_renderer_draw_codepoint(uint8_t *framebuffer, int fb_width, int fb_height,
                                         int x, int y, uint32_t codepoint)
{
    text_renderer_vietnamese_glyph_t glyph = {0};

    if (codepoint == ' ') {
        return;
    }

    if (codepoint >= 32U && codepoint <= 126U) {
        text_renderer_draw_ascii_char(framebuffer, fb_width, fb_height, x, y, (char)codepoint);
        return;
    }

    if (!text_renderer_decompose_vietnamese(codepoint, &glyph)) {
        text_renderer_draw_ascii_char(framebuffer, fb_width, fb_height, x, y, '?');
        return;
    }

    text_renderer_draw_ascii_char(framebuffer, fb_width, fb_height, x, y, glyph.base_char);
    text_renderer_draw_base_mark(framebuffer, fb_width, fb_height, x, y, glyph.base_mark);
    text_renderer_draw_tone_mark(framebuffer, fb_width, fb_height, x, y, glyph.tone);
}

static void text_renderer_trim_to_glyphs(char *text, int max_glyphs)
{
    int count = 0;
    char *cursor = text;

    if (text == NULL || max_glyphs < 0) {
        return;
    }

    while (*cursor != '\0') {
        size_t consumed = 1;
        uint32_t codepoint = 0;

        text_renderer_decode_utf8_char((const uint8_t *)cursor, &consumed, &codepoint);
        if (!text_renderer_is_combining_mark(codepoint)) {
            if (count >= max_glyphs) {
                *cursor = '\0';
                return;
            }
            count++;
        }

        cursor += consumed;
    }
}

static size_t text_renderer_build_line(const char **cursor, char *line, size_t line_size, int max_chars)
{
    const char *p = *cursor;
    const char *last_space_cursor = NULL;
    size_t len = 0;
    size_t len_at_last_space = 0;
    int glyph_count = 0;

    if (line == NULL || line_size == 0 || max_chars <= 0) {
        return 0;
    }

    while (*p == ' ') {
        p++;
    }

    while (*p != '\0') {
        size_t consumed = 1;
        uint32_t codepoint = 0;

        text_renderer_decode_utf8_char((const uint8_t *)p, &consumed, &codepoint);
        if (codepoint == '\n') {
            p += consumed;
            break;
        }
        if (text_renderer_is_combining_mark(codepoint)) {
            p += consumed;
            continue;
        }

        if (glyph_count >= max_chars) {
            if (last_space_cursor != NULL) {
                p = last_space_cursor;
                len = len_at_last_space;
            }
            break;
        }

        if (codepoint == ' ') {
            if (len + 1U >= line_size) {
                break;
            }
            line[len++] = ' ';
            last_space_cursor = p + consumed;
            len_at_last_space = (len > 0U) ? (len - 1U) : 0U;
        } else {
            if (len + consumed >= line_size) {
                break;
            }
            memcpy(&line[len], p, consumed);
            len += consumed;
        }

        glyph_count++;
        p += consumed;
    }

    while (len > 0U && line[len - 1U] == ' ') {
        len--;
    }
    line[len] = '\0';

    *cursor = p;
    return len;
}

void text_renderer_clear(uint8_t *framebuffer, size_t framebuffer_len, bool on)
{
    if (framebuffer == NULL) {
        return;
    }
    memset(framebuffer, on ? 0xFF : 0x00, framebuffer_len);
}

bool text_renderer_can_render_codepoint(uint32_t codepoint)
{
    text_renderer_vietnamese_glyph_t glyph = {0};

    if (codepoint >= 32U && codepoint <= 126U) {
        return true;
    }

    return text_renderer_decompose_vietnamese(codepoint, &glyph);
}

void text_renderer_draw_string(uint8_t *framebuffer, int fb_width, int fb_height, int x, int y, const char *text)
{
    const char *cursor = text;

    if (framebuffer == NULL || text == NULL) {
        return;
    }

    while (*cursor != '\0') {
        size_t consumed = 1;
        uint32_t codepoint = 0;

        text_renderer_decode_utf8_char((const uint8_t *)cursor, &consumed, &codepoint);
        cursor += consumed;

        if (text_renderer_is_combining_mark(codepoint)) {
            continue;
        }

        text_renderer_draw_codepoint(framebuffer, fb_width, fb_height, x, y, codepoint);
        x += FONT_ADVANCE;
    }
}

void text_renderer_draw_wrapped_text(uint8_t *framebuffer, int fb_width, int fb_height,
                                     const text_renderer_box_t *box, const char *text, bool add_ellipsis)
{
    const char *cursor = text;
    int max_chars;
    int max_lines;
    char line[LINE_BUFFER_BYTES];
    bool truncated = false;

    if (framebuffer == NULL || box == NULL || text == NULL || box->width <= 0 || box->height <= 0) {
        return;
    }

    max_chars = box->width / FONT_ADVANCE;
    max_lines = box->height / LINE_HEIGHT;
    if (max_chars <= 0 || max_lines <= 0) {
        return;
    }

    for (int line_index = 0; line_index < max_lines; ++line_index) {
        size_t len;

        while (*cursor == ' ') {
            cursor++;
        }

        if (*cursor == '\0') {
            break;
        }

        len = text_renderer_build_line(&cursor, line, sizeof(line), max_chars);
        if (len == 0U) {
            break;
        }

        if (line_index == max_lines - 1 && *cursor != '\0') {
            truncated = true;
            if (add_ellipsis && max_chars >= 3) {
                text_renderer_trim_to_glyphs(line, max_chars - 3);
                memcpy(&line[strlen(line)], "...", 4);
            }
        }

        text_renderer_draw_string(framebuffer, fb_width, fb_height,
                                  box->x, box->y + (line_index * LINE_HEIGHT), line);

        if (truncated) {
            break;
        }
    }
}

/* 16x16 turn-direction icons. Row-major, MSB = leftmost pixel.
 * Hand-crafted to look like classic turn arrows on a 128x64 OLED. */
static const uint8_t s_icon_left[32] = {
    0x00, 0x00,   /* row  0 */
    0x00, 0x00,   /* row  1 */
    0x00, 0x00,   /* row  2 */
    0x04, 0x00,   /* row  3 ..X........... */
    0x0C, 0x00,   /* row  4 .XX........... */
    0x1C, 0x00,   /* row  5 XXX........... */
    0x3F, 0xF0,   /* row  6 XXXXXXXXXXXX.. */
    0x7F, 0xF8,   /* row  7 XXXXXXXXXXXXX. */
    0x7F, 0xF8,   /* row  8 XXXXXXXXXXXXX. */
    0x3F, 0xF0,   /* row  9 XXXXXXXXXXXX.. */
    0x1C, 0x00,   /* row 10 XXX........... */
    0x0C, 0x00,   /* row 11 .XX........... */
    0x04, 0x00,   /* row 12 ..X........... */
    0x00, 0x00,   /* row 13 */
    0x00, 0x00,   /* row 14 */
    0x00, 0x00,   /* row 15 */
};

static const uint8_t s_icon_right[32] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x20,
    0x00, 0x30,
    0x00, 0x38,
    0x0F, 0xFC,
    0x1F, 0xFE,
    0x1F, 0xFE,
    0x0F, 0xFC,
    0x00, 0x38,
    0x00, 0x30,
    0x00, 0x20,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

static const uint8_t s_icon_straight[32] = {
    0x00, 0x00,
    0x01, 0x80,   /* row  1 .......XX....... */
    0x03, 0xC0,   /* row  2 ......XXXX...... */
    0x07, 0xE0,   /* row  3 .....XXXXXX..... */
    0x0F, 0xF0,   /* row  4 ....XXXXXXXX.... */
    0x1F, 0xF8,   /* row  5 ...XXXXXXXXXX... */
    0x01, 0x80,   /* row  6 shaft */
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x01, 0x80,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

static const uint8_t s_icon_uturn[32] = {
    0x00, 0x00,
    0x0F, 0xC0,   /* row  1 ....XXXXXX...... */
    0x1F, 0xE0,   /* row  2 ...XXXXXXXX..... */
    0x38, 0x70,   /* row  3 ..XXX......XXX.. */
    0x30, 0x30,   /* row  4 ..XX........XX.. */
    0x00, 0x30,   /* row  5 ............XX.. */
    0x00, 0x30,
    0x00, 0x30,
    0x07, 0xF0,   /* row  8 .....XXXXXXX.... */
    0x03, 0xE0,   /* row  9 ......XXXXX..... */
    0x01, 0xC0,   /* row 10 .......XXX...... */
    0x00, 0x80,   /* row 11 ........X....... */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

void text_renderer_draw_nav_icon(uint8_t *framebuffer, int fb_width, int fb_height,
                                 int x, int y, nav_icon_t icon)
{
    const uint8_t *data;

    switch (icon) {
    case NAV_ICON_LEFT:     data = s_icon_left; break;
    case NAV_ICON_RIGHT:    data = s_icon_right; break;
    case NAV_ICON_STRAIGHT: data = s_icon_straight; break;
    case NAV_ICON_UTURN:    data = s_icon_uturn; break;
    default:                return;
    }

    for (int row = 0; row < 16; ++row) {
        uint16_t bits = ((uint16_t)data[row * 2] << 8) | data[row * 2 + 1];
        for (int col = 0; col < 16; ++col) {
            if (bits & (uint16_t)(1U << (15 - col))) {
                text_renderer_draw_pixel(framebuffer, fb_width, fb_height,
                                         x + col, y + row, true);
            }
        }
    }
}
