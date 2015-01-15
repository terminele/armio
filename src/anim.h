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

//___ T Y P E D E F S ________________________________________________________

typedef enum {
    anim_unused = 0,
    anim_rotate_cw,
    anim_rotate_ccw,
    anim_rand,
    anim_fade_in,
    anim_fade_out
} animation_type_t;

typedef struct animation_t {
    animation_type_t type;
    display_comp_t *disp_comp;
    bool enabled;
    bool autorelease_disp_comp; //for display components we allocate
    uint16_t interval_counter;
    uint16_t tick_interval; //update interval in ticks
    int32_t tick_duration; //duration in ticks
    uint8_t step; //only applicable for rotations
    struct animation_t *next, *prev;

} animation_t;

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________

animation_t* anim_rotate( display_comp_t *disp_comp,
        bool clockwise, uint16_t tick_interval, int32_t duration);
  /* @brief animate the given display component rotating
   * @param clockwise - true for clockwise rotation, false for opposite (ccw)
   * @param disp_comp - display component to animate
   * @param tick_interval - rotation rate in ticks (i.e. ms)
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
   * @param tick_interval - rotation rate in ticks (i.e. ms)
   * @param distance - # of step before finishing (i.e 60 is a full circle)
   * @param clockwise - true for clockwise rotation, false for opposite (ccw)
   * @retrn handle to swirl animation object
   */

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
