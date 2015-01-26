/** file:       anim.c
  * created:   2015-01-12 16:58:12
  * author:     Richard Bryan
  */

//___ I N C L U D E S ________________________________________________________
#include "anim.h"
#include "main.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define MAX_ANIMATION_ALLOCS 8
//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________


animation_t* anim_alloc ( void );
  /* @brief allocate a new animation object
   * @param None
   * @retrn the new animation
   */

void anim_free ( animation_t* ptr );
  /* @brief free an animation object
   * @param pointer to anim to free
   * @retrn None
   */

void anim_update( animation_t *anim);
  /* @brief update the given animation
   * @param animation obj
   * @retrn None
   */

//___ V A R I A B L E S ______________________________________________________
animation_t animation_allocs[MAX_ANIMATION_ALLOCS] = {{0}};
animation_t *head_anim_ptr = NULL;
//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

animation_t* anim_alloc ( void ) {

    uint8_t i;

    for (i=0; i < MAX_ANIMATION_ALLOCS; i++) {
        if (animation_allocs[i].type == anim_unused)
            return &(animation_allocs[i]);
    }

    assert(false);
    return NULL;
}

void anim_free ( animation_t* ptr ) {
    ptr->type = anim_unused;
}


void anim_update( animation_t *anim ) {
    uint8_t tmp;
    switch(anim->type) {
        case anim_rotate_cw:
            display_comp_update_pos(anim->disp_comp,
                (anim->disp_comp->pos + anim->step) % 60);
            break;
        case anim_rotate_ccw:
            display_comp_update_pos(anim->disp_comp,
                anim->disp_comp->pos < anim->step ?
                    60 + anim->disp_comp->pos - anim->step :
                    anim->disp_comp->pos - anim->step);
            break;
        case anim_rand:
            display_comp_update_pos(anim->disp_comp,
                    rand() % 60);
            break;
        case anim_fade_inout:
            if (anim->disp_comp->brightness == anim->bright_end) {
                tmp = anim->bright_end;
                anim->bright_end = anim->bright_start;
                anim->bright_start = tmp;
            }

            display_comp_update_brightness(anim->disp_comp,
                    anim->disp_comp->brightness < anim->bright_end ?
                    anim->disp_comp->brightness + 1 :
                    anim->disp_comp->brightness - 1);

            break;
        default:
            main_terminate_in_error( ERROR_ANIM_BAD_TYPE );
            break;

    }
}

//___ F U N C T I O N S ______________________________________________________


animation_t* anim_rotate(display_comp_t *disp_comp,
        bool clockwise, uint16_t tick_interval, int32_t duration) {
    animation_t *anim = anim_alloc();

    anim->type = clockwise ? anim_rotate_cw : anim_rotate_ccw;
    anim->enabled = true;
    anim->disp_comp = disp_comp;
    anim->autorelease_disp_comp = false;
    anim->tick_interval = tick_interval;
    anim->interval_counter = 0;
    anim->tick_duration = duration;
    anim->prev = anim->next = NULL;
    anim->step = 1;

    DL_APPEND(head_anim_ptr, anim);

    return anim;
}

animation_t* anim_random( display_comp_t *disp_comp,
        uint16_t tick_interval, int32_t duration) {

    animation_t *anim = anim_alloc();

    anim->type = anim_rand;
    anim->enabled = true;
    anim->autorelease_disp_comp = false;
    anim->tick_interval = tick_interval;
    anim->disp_comp = disp_comp;
    anim->interval_counter = 0;
    anim->tick_duration = duration;
    anim->prev = anim->next = NULL;

    DL_APPEND(head_anim_ptr, anim);

    return anim;
}

animation_t* anim_swirl(uint8_t start, uint8_t len, uint16_t tick_interval,
        uint32_t distance, bool clockwise) {
    display_comp_t *disp_comp = display_snake(start, BRIGHT_DEFAULT, len);
    animation_t *anim = anim_rotate(disp_comp, clockwise, tick_interval,
            distance * tick_interval);

    anim->autorelease_disp_comp = true;

    return anim;

}

animation_t* anim_fade(display_comp_t *disp_comp,
        uint8_t bright_start, uint8_t bright_end, uint16_t tick_interval,
        int16_t cycles) {

    animation_t *anim = anim_alloc();

    anim->type = anim_fade_inout;
    anim->enabled = true;
    anim->autorelease_disp_comp = false;
    anim->disp_comp = disp_comp;
    anim->tick_interval = tick_interval;
    anim->interval_counter = 0;
    anim->bright_start = bright_start;
    anim->bright_end = bright_end;

    if (cycles != ANIMATION_DURATION_INF)
        anim->tick_duration = cycles * abs(bright_end - bright_start) * tick_interval;
    else
        anim->tick_duration = ANIMATION_DURATION_INF;

    anim->prev = anim->next = NULL;
    display_comp_update_brightness(disp_comp, bright_start);

    DL_APPEND(head_anim_ptr, anim);

    return anim;

}

void anim_stop( animation_t *anim) {
    anim->enabled = false;
}

void anim_release( animation_t *anim) {
    if (anim->autorelease_disp_comp)
        display_comp_release(anim->disp_comp);

    anim_free(anim);

}

void anim_tic( void ) {
    animation_t *anim = NULL;

    DL_FOREACH(head_anim_ptr, anim) {
        if (anim->tick_duration > 0) {
            anim->tick_duration--;
            if (anim->tick_duration == 0) {
                anim->enabled = false;
            }
        }

        anim->interval_counter++;
        if (anim->enabled && anim->interval_counter == anim->tick_interval) {
            anim->interval_counter = 0;
            anim_update(anim);
        }

    }
}

void anim_init( void ) {
}
