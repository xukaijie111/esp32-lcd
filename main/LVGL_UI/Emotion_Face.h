#ifndef EMOTION_FACE_H
#define EMOTION_FACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* Face states shown on screen */
typedef enum {
    EMO_IDLE = 0,     /* hide face, show normal UI            */
    EMO_LISTENING,    /* woke up, listening for a command     */
    EMO_HAPPY,        /* command recognized, show smile       */
} emotion_state_t;

/* Create the emotion face overlay.
 * Must be called once from the LVGL context (after the main UI is built). */
void emotion_face_init(void);

/* Request a face state. Thread-safe: can be called from any task
 * (e.g. the audio detect task). The change is applied by an LVGL timer. */
void emotion_set(emotion_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* EMOTION_FACE_H */
