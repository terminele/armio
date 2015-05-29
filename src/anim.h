/** file:       anim.h
  * created:   2015-01-12 13:29:48
  * author:     Richard Bryan
  */

#ifndef __ANIM_H__
#define __ANIM_H__

//___ I N C L U D E S ________________________________________________________
#include "display.h"
#include "utlist.h"

//___ M A C R O S ____________________________________________________________
#define ANIMATION_DURATION_INF -1

#define BLINK_INT_DEFAULT   MS_IN_TICKS(100)
#define BLINK_INT_FAST      MS_IN_TICKS(50)
#define BLINK_INT_MED       MS_IN_TICKS(200)
#define BLINK_INT_SLOW      MS_IN_TICKS(500)

//___ T Y P E D E F S ________________________________________________________

typedef enum {
    animt_unused = 0,
    animt_rotate_cw,
    animt_rotate_ccw,
    animt_rand,
    animt_fade_inout,
    animt_blink,
    animt_cut,
    animt_yoyo,
    animt_flicker,
} animation_type_t;

typedef struct animation_t {
    display_comp_t *disp_comp;
    uint16_t interval_counter;
    uint16_t tick_interval; //update interval in ticks
    int32_t tick_duration; //duration in ticks

    union {
      int8_t step; //only applicable for rotations and yoyos
      int8_t index; //used for flicker transitioning
    };

    uint8_t len; //only applicable for yoyos
    uint8_t bright_start; //only applicable for fades
    uint8_t bright_end; //only applicable for fades

    struct animation_t *next, *prev;

    animation_type_t type;
    bool enabled;
    bool autorelease_disp_comp; //for display components we allocate
    bool autorelease_anim; //for animations we should release at their ending

} animation_t;

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________

animation_t* anim_rotate( display_comp_t *disp_comp,
        bool clockwise, uint16_t tick_interval, int32_t duration);
  /* @brief animate the given display component rotating
   * @param clockwise - true for clockwise rotation, false for opposite (ccw)
   * @param disp_comp - display component to animate
   * @param tick_interval - rotation rate in ticks
   * @param duration - duration of animation in ticks or
   *    ANIMATION_DURATION_INF for indefinite animation
   * @retrn animation reference handle
   */
animation_t* anim_random( display_comp_t *disp_comp,
        uint16_t tick_interval, int32_t duration );
  /* @brief animate display comp with random positions
   * @param disp_comp - display component to animate
   * @param tick_interval - how often to update position
   * @param duration - duration of animation in ticks or
   *    ANIMATION_DURATION_INF for indefinite animation
   * @retrn animation reference handle
   */

animation_t* anim_swirl(uint8_t start, uint8_t len, uint16_t tick_interval,
        uint32_t distance, bool clockwise);
  /* @brief convenience function for creating and
   *  animating a finite swirling snake
   * @param start - starting pos (0-60)
   * @param len - snake length
   * @param tick_interval - rotation rate in ticks
   * @param distance - # of steps before finishing (i.e. 60 is a full circle)
   * @param clockwise - true for clockwise rotation, false for opposite (ccw)
   * @retrn handle to swirl animation object
   */

animation_t* anim_fade(display_comp_t *disp_comp,
        uint8_t bright_start, uint8_t bright_end, uint16_t tick_interval,
        int16_t cycles, bool autorelease);
  /* @brief fade the given disp comp
   * @param disp_comp - display component to animate
   * @param bright_start - starting brightness level
   * @param bright_end - ending brightness level
   * @param tick_interval - interval between brightness steps
   * @param cycles - # of (half) cycles to repeat. If this is 1, then
   *    a single fade in or out occurs.  Else a series of fades in and
   *    out occur (e.g. 6 cycles would be 3 sequences of fade in and fade
   *    outs).  Can be ANIMATION_DURATION_INF
   * @param autorelease - if animation and display comp should be freed at completion
   * @retrn handle to fade animation object
   */

animation_t* anim_cutout(display_comp_t *disp_comp,
        uint16_t tick_duration, bool autorelease);
  /* @brief shows the given component for the given duration, then hides it
   * @param disp_comp - display component to animate
   * @param tick_duration - duration that component should be shown
   * @param autorelease - if animation and display comp should be freed at completion
   * @retrn None
   */

animation_t* anim_blink(display_comp_t *disp_comp, uint16_t tick_interval,
        uint16_t tick_duration, bool autorelease);
  /* @brief blinks the component for the given duration, then hides it
   * @param disp_comp - display component to animate
   * @param tick_interval - blink interval in ticks
   * @param tick_duration - duration that component should be shown (or
   * ANIM_DURATION_INF for indefinite)
   * @param autorelease - if animation and display comp should be freed at completion
   * @retrn None
   */

animation_t* anim_flicker(display_comp_t *disp_comp,
    uint16_t tick_duration, bool autorelease);
  /* @brief flickers the component on/off based on flicker pattern
   * @param disp_comp - display component to animate
   * @param tick_duration - duration that component should be shown (or
   * ANIM_DURATION_INF for indefinite)
   * @param autorelease - if animation and display comp should be freed at completion
   * @retrn None
   */

animation_t* anim_yoyo(display_comp_t *disp_comp, uint8_t len,
        uint16_t tick_interval, int16_t yos, bool autorelease);
  /* @brief displays a line yoyo'ing between length 1 and max length
   * @param disp_comp - display component to animate
   * @param len - max line length
   * @param tick_interval - interval between grow/contraction steps
   * @param yos - number of grow/contractions to animate.  1 'yo' would be
   *    a single animation of a point to a full line in one direction.  2 yos
   *    would be a point animating to a full line then contracting back to a
   *    point.  And so on.  May be ANIM_DURATION_INF
   * ANIM_DURATION_INF for indefinite)
   * @param autorelease - if animation and display comp should be freed at completion
   * @retrn None
   */

static inline animation_t* anim_snake_grow( uint8_t pos,
        uint8_t len, uint16_t tick_interval, bool autorelease) {
    display_comp_t *comp_ptr = display_snake(pos, MAX_BRIGHT_VAL,
                        1, true);
    animation_t *anim_ptr = anim_yoyo(comp_ptr, len, tick_interval, 1, autorelease);
    anim_ptr->autorelease_disp_comp = true;
    return anim_ptr;
}

  /* @brief animates a snake growing from a single point to the given length
   * @param pos - starting position
   * @param len - max snake length
   * @param tick_interval - interval between grow/contraction steps
   * @param autorelease - if animation should be freed at completion
   * @retrn None
   */

static inline animation_t* anim_fade_in(display_comp_t *disp_comp,
        uint8_t brightness, uint16_t tick_interval, bool autorelease) {
    return anim_fade(disp_comp, 0, brightness, tick_interval, 1, autorelease);
}
  /* @brief fade in the given display component
   * @param disp_comp - display component to animate
   * @param brightness - ending brightness level
   * @param tick_interval - interval between brightness steps
   * @param autorelease - if animation and display comp should be freed at completion
   * @retrn handle to fade animation object
   */

static inline animation_t* anim_fade_out(display_comp_t *disp_comp,
        uint16_t tick_interval, bool autorelease) {
    return anim_fade(disp_comp, disp_comp->brightness, 0, tick_interval, 1,
            autorelease);
}
  /* @brief fade out the given display component
   * @param disp_comp - display component to animate
   * @param tick_interval - interval between brightness steps
   * @param autorelease - if animation and display comp should be freed at completion
   * @retrn handle to fade animation object
   */

static inline void anim_update_length( animation_t *anim, uint8_t length ) {
    anim->len = length;
}

static inline bool anim_is_finished ( animation_t *anim ) {
    return !anim->enabled;
}
  /* @brief check if the given animation has finished
   * @param animation reference handle to check
   * @retrn true if finished, else false
   */

void anim_stop( animation_t *anim );
  /* @brief stop the given animation
   * @param animation reference handle to stop
   * @retrn None
   */
void anim_release( animation_t *anim );
  /* @brief release/free the given animation
   * @param animation obj to free
   * @retrn None
   */

void anim_tic( void );
  /* @brief animation update function
   * @param None
   * @retrn None
   */

void anim_init( void );
  /* @brief initialize animation module
   * @param None
   * @retrn None
   */


#endif /* end of include guard: __ANIM_H__ */

// vim:shiftwidth=2
