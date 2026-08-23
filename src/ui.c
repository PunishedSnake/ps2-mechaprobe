#include <tamtypes.h>
#include <debug.h>
#include <libpad.h>

#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "ui.h"

#define UI_BG          0x00110a07u
#define UI_HEADER      0x001e140eu
#define UI_PANEL       0x00291c13u
#define UI_PANEL_ALT   0x0033291bu
#define UI_TEXT        0x00f5eee9u
#define UI_MUTED       0x00bba491u
#define UI_ACCENT      0x00ebcd31u
#define UI_SUCCESS     0x0089c44cu
#define UI_WARNING     0x0048b0e6u
#define UI_DANGER      0x005c5ce6u
#define UI_DISABLED    0x005a5149u

static u32 tone_color(int tone)
{
    switch (tone) {
        case UI_TONE_SUCCESS: return UI_SUCCESS;
        case UI_TONE_WARNING: return UI_WARNING;
        case UI_TONE_DANGER: return UI_DANGER;
        default: return UI_ACCENT;
    }
}

static void clear_line(unsigned int y, u32 background)
{
    scr_setbgcolor(background);
    scr_clearline((int)y);
}

static void print_line(unsigned int x, unsigned int y, u32 background,
                       u32 foreground, const char *text)
{
    clear_line(y, background);
    scr_setfontcolor(foreground);
    scr_setXY((int)x, (int)y);
    scr_printf("%s", text != NULL ? text : "");
}

static void shell(const char *section, const char *title, const char *status,
                  int tone)
{
    unsigned int y;
    u32 accent = tone_color(tone);
    char version_line[80];

    scr_setCursor(0);
    scr_setbgcolor(UI_BG);
    scr_clear();

    clear_line(0, accent);
    print_line(2, 1, UI_HEADER, UI_TEXT, "PS2 Mecha Probe");
    snprintf(version_line, sizeof(version_line), "v0.1.0-dev  |  %s",
             section != NULL ? section : "STATUS");
    print_line(2, 2, UI_HEADER, accent, version_line);
    clear_line(3, UI_HEADER);

    print_line(2, 5, UI_BG, UI_TEXT, title != NULL ? title : "Mecha Probe");
    if (status != NULL && status[0] != '\0')
        print_line(2, 6, UI_PANEL, UI_MUTED, status);
    else
        clear_line(6, UI_BG);

    for (y = 7; y < 38; y++)
        clear_line(y, UI_BG);
}

void ui_init(void)
{
    init_scr();
    scr_setCursor(0);
    scr_setbgcolor(UI_BG);
    scr_setfontcolor(UI_TEXT);
    scr_clear();
    ui_message("Starting", "Graphics frontend ready",
               "Preparing an isolated PS2 runtime for MechaCon probing.",
               NULL, UI_TONE_INFO);
}

void ui_message(const char *title, const char *status,
                const char *body, const char *footer, int tone)
{
    char buffer[1024];
    char *line;
    char *saveptr = NULL;
    unsigned int y = 9;
    u32 accent = tone_color(tone);

    shell("STATUS", title, status, tone);
    for (y = 8; y <= 34; y++)
        clear_line(y, UI_PANEL);

    if (body != NULL) {
        snprintf(buffer, sizeof(buffer), "%s", body);
        line = strtok_r(buffer, "\n", &saveptr);
        y = 9;
        while (line != NULL && y <= 33) {
            print_line(4, y, UI_PANEL, y == 9 ? accent : UI_TEXT, line);
            line = strtok_r(NULL, "\n", &saveptr);
            y++;
        }
    }

    clear_line(36, UI_PANEL_ALT);
    clear_line(37, UI_PANEL_ALT);
    print_line(2, 38, UI_PANEL_ALT, UI_MUTED,
               footer != NULL ? footer : "X Continue");
    clear_line(39, UI_PANEL_ALT);
}

static void render_menu(const char *title, const char *status,
                        const ui_menu_item_t *items, unsigned int item_count,
                        unsigned int selected)
{
    unsigned int i;
    unsigned int y = 9;

    shell("PROBE", title, status, UI_TONE_INFO);
    for (i = 0; i < item_count && y + 1 < 36; i++, y += 4) {
        int is_selected = i == selected;
        u32 background = is_selected ? UI_PANEL_ALT : UI_PANEL;
        u32 label_color = items[i].enabled ? UI_TEXT : UI_DISABLED;
        char label[76];

        snprintf(label, sizeof(label), "%s%s",
                 is_selected ? "> " : "  ", items[i].label);
        print_line(3, y, background,
                   is_selected && items[i].enabled ? UI_ACCENT : label_color,
                   label);
        print_line(5, y + 1, background,
                   items[i].enabled ? UI_MUTED : UI_DISABLED,
                   items[i].hint != NULL ? items[i].hint : "");
        clear_line(y + 2, UI_BG);
    }

    clear_line(36, UI_PANEL_ALT);
    clear_line(37, UI_PANEL_ALT);
    print_line(2, 38, UI_PANEL_ALT, UI_MUTED,
               "UP/DOWN Select     X Open     TRIANGLE Back");
    clear_line(39, UI_PANEL_ALT);
}

int ui_menu_select(const char *title, const char *status,
                   const ui_menu_item_t *items, unsigned int item_count,
                   unsigned int *selection)
{
    unsigned int current;

    if (items == NULL || item_count == 0 || selection == NULL)
        return -1;
    current = *selection < item_count ? *selection : 0;

    for (;;) {
        u32 pressed;

        render_menu(title, status, items, item_count, current);
        pressed = platform_wait_press();
        if (pressed & PAD_UP)
            current = (current + item_count - 1u) % item_count;
        if (pressed & PAD_DOWN)
            current = (current + 1u) % item_count;
        if ((pressed & PAD_CROSS) && items[current].enabled) {
            *selection = current;
            return (int)current;
        }
        if (pressed & PAD_TRIANGLE) {
            *selection = current;
            return -1;
        }
    }
}

void ui_wait_cross(void)
{
    while (!(platform_wait_press() & PAD_CROSS)) {}
}
