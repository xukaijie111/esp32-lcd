#include "Emotion_Face.h"
#include <stdint.h>

/* ---------- config ---------- */
#define FACE_SIZE          220
#define EYE_SIZE           42
#define EYE_OFFSET_X       48
#define EYE_OFFSET_Y       60
#define HAPPY_HOLD_TICKS   20                        /* 20 * 100 ms = 2 s */

#define COLOR_FACE_LISTEN  lv_color_hex(0x2563EB)    /* blue   */
#define COLOR_FACE_HAPPY   lv_color_hex(0xF59E0B)    /* amber  */
#define COLOR_FEATURE      lv_color_white()

/* ---------- state (written from other tasks, read in LVGL timer) ---------- */
static volatile emotion_state_t g_state_req   = EMO_IDLE;   /* requested state        */
static emotion_state_t          g_state_cur   = EMO_IDLE;   /* currently applied      */
static volatile int32_t         g_happy_count = 0;          /* 100 ms ticks remaining */

/* ---------- widgets ---------- */
static lv_obj_t *overlay    = NULL;
static lv_obj_t *face       = NULL;
static lv_obj_t *eye_l      = NULL;
static lv_obj_t *eye_r      = NULL;
static lv_obj_t *mouth_oval = NULL;    /* open mouth (listening) */
static lv_obj_t *mouth_arc  = NULL;    /* smile arc  (happy)     */
static lv_timer_t *emo_timer = NULL;

static void apply_state(emotion_state_t st)
{
    if (st == EMO_IDLE) {
        lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(face, (st == EMO_HAPPY) ? COLOR_FACE_HAPPY : COLOR_FACE_LISTEN, 0);

    if (st == EMO_LISTENING) {
        /* round wide-open eyes + open mouth */
        lv_obj_set_size(eye_l, EYE_SIZE, EYE_SIZE);
        lv_obj_set_size(eye_r, EYE_SIZE, EYE_SIZE);
        lv_obj_clear_flag(mouth_oval, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_arc, LV_OBJ_FLAG_HIDDEN);
    } else { /* EMO_HAPPY */
        /* squinted eyes + big smile arc */
        lv_obj_set_size(eye_l, EYE_SIZE, EYE_SIZE / 2);
        lv_obj_set_size(eye_r, EYE_SIZE, EYE_SIZE / 2);
        lv_obj_add_flag(mouth_oval, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mouth_arc, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_move_foreground(overlay);
}

static void emo_timer_cb(lv_timer_t *t)
{
    (void)t;

    /* auto-hide the happy face after the hold time */
    if (g_state_cur == EMO_HAPPY && g_happy_count > 0 && --g_happy_count == 0) {
        g_state_req = EMO_IDLE;
    }

    if (g_state_req != g_state_cur) {
        g_state_cur = g_state_req;
        apply_state(g_state_cur);
    }
}

static lv_obj_t *create_eye(lv_obj_t *parent, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs)
{
    lv_obj_t *eye = lv_obj_create(parent);
    lv_obj_set_size(eye, EYE_SIZE, EYE_SIZE);
    lv_obj_align(eye, align, x_ofs, y_ofs);
    lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye, COLOR_FEATURE, 0);
    lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye, 0, 0);
    lv_obj_clear_flag(eye, LV_OBJ_FLAG_SCROLLABLE);
    return eye;
}

void emotion_face_init(void)
{
    if (overlay != NULL) {
        return;    /* already created */
    }

    /* full-screen dim overlay on top of everything */
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    /* round face */
    face = lv_obj_create(overlay);
    lv_obj_set_size(face, FACE_SIZE, FACE_SIZE);
    lv_obj_align(face, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(face, COLOR_FACE_LISTEN, 0);
    lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(face, 0, 0);
    lv_obj_clear_flag(face, LV_OBJ_FLAG_SCROLLABLE);

    /* eyes */
    eye_l = create_eye(face, LV_ALIGN_TOP_LEFT,  EYE_OFFSET_X,  EYE_OFFSET_Y);
    eye_r = create_eye(face, LV_ALIGN_TOP_RIGHT, -EYE_OFFSET_X, EYE_OFFSET_Y);

    /* open mouth (listening) */
    mouth_oval = lv_obj_create(face);
    lv_obj_set_size(mouth_oval, 64, 40);
    lv_obj_align(mouth_oval, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_radius(mouth_oval, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(mouth_oval, COLOR_FEATURE, 0);
    lv_obj_set_style_bg_opa(mouth_oval, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mouth_oval, 0, 0);
    lv_obj_clear_flag(mouth_oval, LV_OBJ_FLAG_SCROLLABLE);

    /* smile arc (happy): 135deg -> 225deg draws the bottom arc of a circle */
    mouth_arc = lv_arc_create(face);
    lv_obj_set_size(mouth_arc, 110, 110);
    lv_obj_align(mouth_arc, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_arc_set_rotation(mouth_arc, 0);
    lv_arc_set_bg_angles(mouth_arc, 0, 0);
    lv_arc_set_angles(mouth_arc, 135, 225);
    lv_obj_remove_style(mouth_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(mouth_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(mouth_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(mouth_arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(mouth_arc, COLOR_FEATURE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(mouth_arc, true, LV_PART_INDICATOR);

    emo_timer = lv_timer_create(emo_timer_cb, 100, NULL);
}

void emotion_set(emotion_state_t state)
{
    if (overlay == NULL) {
        return;    /* not initialized yet */
    }

    if (state == EMO_HAPPY) {
        g_happy_count = HAPPY_HOLD_TICKS;    /* restart auto-back-to-idle countdown */
    }
    g_state_req = state;
}
