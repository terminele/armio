/** file:       anim.c
  * created:   2015-01-12 16:58:12
  * author:     Richard Bryan
  */

//___ I N C L U D E S ________________________________________________________
#include "anim.h"
#include "main.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define MAX_ANIMATION_ALLOCS 8

#define ANIM_ERROR_MALLOC_FAIL          ((uint32_t) 1)
#define ANIM_ERROR_BAD_TYPE( type )    (((uint32_t) 2) \
    | (((uint32_t) type)<<8))

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________


animation_t* anim_alloc ( void );
  /* @brief allocate a new animation object
   * @param None
   * @retrn the new animation
   */

void anim_free ( animation_t* ptr );
  /* @brief free an animation object
   * @param pointer to anim to free * @retrn None
   */

void anim_update( animation_t *anim);
  /* @brief update the given animation
   * @param animation obj
   * @retrn None
   */

//___ V A R I A B L E S ______________________________________________________
animation_t animation_allocs[MAX_ANIMATION_ALLOCS] = {{0}};
animation_t *head_anim_ptr = NULL;
const uint8_t flicker_pattern[] =  \
{60,
 15,
 135,
 15,
 75,
 15,
 200,
 5,
 220,
 15,
 240,
 30,
 60,
 15,
 15,
 15,
 30,
 60,
 15,
 15,
 30,
 //165,
 //240,
 75,
 15};

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

animation_t* anim_alloc ( void ) {
  uint8_t i;

  for (i=0; i < MAX_ANIMATION_ALLOCS; i++) {
    if (animation_allocs[i].type == animt_unused) {
      return &(animation_allocs[i]);
    }
  }

  main_terminate_in_error( error_group_animation, ANIM_ERROR_MALLOC_FAIL );
  return NULL;
}

void anim_free ( animation_t* ptr ) {
  ptr->type = animt_unused;
}


void anim_update( animation_t *anim ) {
  uint8_t tmp;
  switch(anim->type) {
    case animt_rotate_cw:
      display_comp_update_pos(anim->disp_comp,
          (anim->disp_comp->pos + anim->step) % 60);
      break;
    case animt_rotate_ccw:
      display_comp_update_pos(anim->disp_comp,
          anim->disp_comp->pos < anim->step ?
          60 + anim->disp_comp->pos - anim->step :
          anim->disp_comp->pos - anim->step);
      break;
    case animt_rand:
      display_comp_update_pos(anim->disp_comp,
          rand() % 60);
      break;
    case animt_fade_inout:
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
    case animt_cut:
      /* main tic will take care of this */
      break;
    case animt_blink:
      if (anim->disp_comp->on) {
        display_comp_hide(anim->disp_comp);
      } else {
        display_comp_show(anim->disp_comp);
      }
      break;
    case animt_yoyo:
      display_comp_update_length( anim->disp_comp,
          anim->disp_comp->length + anim->step);
      if ((anim->step > 0 && anim->disp_comp->length == anim->len) ||
          (anim->step < 0 && anim->disp_comp->length == 1)) {
        anim->step *= -1;
      }
      break;
    case animt_flicker:
      anim->tick_interval = MS_IN_TICKS(flicker_pattern[anim->index]);
      anim->index++;
      if (anim->index == sizeof(flicker_pattern)/sizeof(*flicker_pattern)) {
        anim->index = 0;
      }

      if (anim->disp_comp->on) {
        display_comp_hide(anim->disp_comp);
      } else {
        display_comp_show(anim->disp_comp);
      }

      break;

    default:
      main_terminate_in_error( error_group_animation,
          ANIM_ERROR_BAD_TYPE( anim->type ) );
      break;
  }
}

//___ F U N C T I O N S ______________________________________________________


animation_t* anim_rotate(display_comp_t *disp_comp,
    bool clockwise, uint16_t tick_interval, int32_t duration) {
  animation_t *anim = anim_alloc();

  anim->type = clockwise ? animt_rotate_cw : animt_rotate_ccw;
  anim->enabled = true;
  anim->disp_comp = disp_comp;
  anim->autorelease_disp_comp = false;
  anim->autorelease_anim = false;
  anim->tick_interval = tick_interval;
  anim->interval_counter = 0;
  anim->tick_duration = duration;
  anim->prev = anim->next = NULL;
  anim->step = 1;

  DL_APPEND(head_anim_ptr, anim);

  return anim;
}

animation_t* anim_random( display_comp_t *disp_comp,
    uint16_t tick_interval, int32_t duration, bool autorelease) {

  animation_t *anim = anim_alloc();

  anim->type = animt_rand;
  anim->enabled = true;
  anim->autorelease_disp_comp = autorelease;
  anim->autorelease_anim = autorelease;
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
  display_comp_t *disp_comp = display_snake(start, BRIGHT_DEFAULT, len, clockwise);

  animation_t *anim = anim_rotate(disp_comp, clockwise, tick_interval,
      distance > 0 ? distance * tick_interval : 1);

  anim->autorelease_anim = false;
  anim->autorelease_disp_comp = true;

  return anim;

}

animation_t* anim_fade(display_comp_t *disp_comp,
    uint8_t bright_start, uint8_t bright_end, uint16_t tick_interval,
    int16_t cycles, bool autorelease) {

  animation_t *anim = anim_alloc();

  anim->type = animt_fade_inout;
  anim->enabled = true;
  anim->autorelease_disp_comp = autorelease;
  anim->autorelease_anim = autorelease;
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

animation_t* anim_cutout(display_comp_t *disp_comp,
    uint16_t tick_duration, bool autorelease) {


  animation_t *anim = anim_alloc();

  anim->type = animt_cut;
  anim->enabled = true;
  anim->autorelease_disp_comp = autorelease;
  anim->autorelease_anim = autorelease;
  anim->disp_comp = disp_comp;
  anim->tick_duration = tick_duration;
  anim->interval_counter = 0;

  anim->prev = anim->next = NULL;

  DL_APPEND(head_anim_ptr, anim);

  return anim;
}

animation_t* anim_blink(display_comp_t *disp_comp, uint16_t tick_interval,
    uint16_t tick_duration, bool autorelease) {

  animation_t *anim = anim_alloc();

  anim->type = animt_blink;
  anim->enabled = true;
  anim->autorelease_disp_comp = autorelease;
  anim->autorelease_anim = autorelease;
  anim->disp_comp = disp_comp;
  anim->tick_interval = tick_interval;
  anim->tick_duration = tick_duration;
  anim->interval_counter = 0;

  anim->prev = anim->next = NULL;

  DL_APPEND(head_anim_ptr, anim);

  return anim;
}


animation_t* anim_flicker(display_comp_t *disp_comp,
    uint16_t tick_duration, bool autorelease) {

  animation_t *anim = anim_alloc();

  anim->type = animt_flicker;
  anim->enabled = true;
  anim->autorelease_disp_comp = autorelease;
  anim->autorelease_anim = autorelease;
  anim->disp_comp = disp_comp;
  anim->tick_interval = MS_IN_TICKS(flicker_pattern[0]);
  anim->index = 1;
  anim->tick_duration = tick_duration;
  anim->interval_counter = 0;

  anim->prev = anim->next = NULL;

  DL_APPEND(head_anim_ptr, anim);

  return anim;
}
animation_t* anim_yoyo(display_comp_t *disp_comp, uint8_t len,
    uint16_t tick_interval, int16_t yos, bool autorelease) {

  //start with a line/snake of length 1
  display_comp_update_length(disp_comp, 1);

  animation_t *anim = anim_alloc();
  anim->type = animt_yoyo;
  anim->enabled = true;
  anim->autorelease_disp_comp = autorelease;
  anim->autorelease_anim = autorelease;
  anim->disp_comp = disp_comp;
  anim->tick_interval = tick_interval;

  if (yos == ANIMATION_DURATION_INF) {
    anim->tick_duration = ANIMATION_DURATION_INF;
  } else {
    anim->tick_duration = tick_interval * (1 + yos * (len - 1));
  }

  anim->interval_counter = 0;
  anim->len = len;
  anim->step = 0;

  if (len > 1)
    anim->step = 1;

  anim->prev = anim->next = NULL;

  DL_APPEND(head_anim_ptr, anim);

  return anim;
}

void anim_stop( animation_t *anim) {
  if (anim)
    anim->enabled = false;
}

void anim_release( animation_t *anim) {
  if (!anim) return;

  if (anim->autorelease_disp_comp) {
    display_comp_hide(anim->disp_comp);
    display_comp_release(anim->disp_comp);
  }

  DL_DELETE(head_anim_ptr, anim);
  anim_free(anim);

}

void anim_tic( void ) {
  animation_t *anim = NULL;
  animation_t *tmp = NULL;

  DL_FOREACH_SAFE(head_anim_ptr, anim, tmp) {
    if (anim->tick_duration > 0) {
      anim->tick_duration--;
      if (anim->tick_duration == 0) {
        anim->enabled = false;

        if (anim->autorelease_anim) {
          anim_release(anim);
        }
        continue;
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

// vim:shiftwidth=2
