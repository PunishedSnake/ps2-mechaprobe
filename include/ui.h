#ifndef PS2_MECHAPROBE_UI_H
#define PS2_MECHAPROBE_UI_H

typedef struct {
    const char *label;
    const char *hint;
    int enabled;
} ui_menu_item_t;

void ui_init(void);
void ui_message(const char *title, const char *status,
                const char *body, const char *footer, int tone);
int ui_menu_select(const char *title, const char *status,
                   const ui_menu_item_t *items, unsigned int item_count,
                   unsigned int *selection);
void ui_wait_cross(void);

#define UI_TONE_INFO 0
#define UI_TONE_SUCCESS 1
#define UI_TONE_WARNING 2
#define UI_TONE_DANGER 3

#endif
