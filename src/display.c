/** file:   display.c
  * modified:   2015-01-09 16:16:54
  * author:     Richard Bryan
  *
  * the display module is responsible for 'drawing' higher level
  * static shapes to the led rings.  It maintains a stack of
  * display components (e.g. points, lines, polygons) of varying
  * brightness level and blink rates.  On each call to tic(), these
  * components are drawn from tail to head (i.e. the head component
  * takes precedence on any leds modified by lower priority components).
  *
  * There is a maximum of 64 display components.  Callers should release
  * their components (via comp_free) when they're not being used (e.g. when
  * a mode change occurs).
  */

//___ I N C L U D E S ________________________________________________________
#include <asf.h>
#include "display.h"
#include "utlist.h"
#include "main.h"


//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define MAX_ALLOCATIONS     64

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________


display_comp_t* comp_alloc ( void );
  /* @brief allocate a new display component
   * @param None
   * @retrn the new component
   */

void comp_free ( display_comp_t* ptr );
  /* @brief free a new display component
   * @param pointer to comp to free
   * @retrn None
   */

void draw_comp( display_comp_t* comp_ptr);
  /* @brief draws the given component to the display
   *    (i.e. sets the led state(s) comprising the
   *    component)
   * @param component to draw
   * @retrn None
   */



//___ V A R I A B L E S ______________________________________________________

/* statically allocate maximum number of display components */
display_comp_t component_allocs[MAX_ALLOCATIONS] = {{0}};


/* pointer to head of active component list */
display_comp_t *head_component_ptr = NULL;


//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

display_comp_t *comp_alloc( void ) {
    display_comp_t* comp_ptr = component_allocs;

    while (comp_ptr->type != dispt_unused) comp_ptr++;

    assert(comp_ptr < component_allocs + MAX_ALLOCATIONS);

    return comp_ptr;
}

void comp_free ( display_comp_t* ptr ) { if (ptr) ptr->type = dispt_unused; }

void draw_comp( display_comp_t* comp) {
    uint8_t pos = comp->pos;

    if (!comp->on) return;

    switch(comp->type) {
      case dispt_point:
        led_on(pos, comp->brightness);
        break;
      case dispt_line:
        while ((comp->pos - pos) % 60  < comp->length) {
          led_on(pos, comp->brightness);
          pos--;
        }
        break;
      case dispt_snake:
        //TODO
        break;
      case dispt_polygon:
        //TODO
        break;
      default:
        main_terminate_in_error( ERROR_DISP_DRAW_BAD_COMP_TYPE );
        break;
    }
}

//___ F U N C T I O N S ______________________________________________________


void display_swirl(int tail_len, int tick_us, int revolutions, int max_intensity) {
    while(revolutions) {
        for (int i=0; i < 60; i++) {
            /* light up current swirl segment */
            for ( int j = 0; j  < tail_len; j++ ) {
                uint8_t intensity = 1 + j*max_intensity/tail_len;
                led_set_intensity((i+j) % 60, intensity);
            }
            delay_us(tick_us);
            led_off(i);
        }

    led_clear_all();
    revolutions--;
    }


}


display_comp_t* display_point ( uint8_t pos,
        uint8_t brightness, uint16_t blink_interval ) {

    display_comp_t *comp_ptr = comp_alloc();

    comp_ptr->type = dispt_point;
    comp_ptr->on = true;
    comp_ptr->brightness = brightness;
    comp_ptr->blink_interval = blink_interval;
    comp_ptr->pos = pos;
    comp_ptr->length = 1;

    comp_ptr->next = comp_ptr->prev = NULL;

    DL_APPEND(head_component_ptr, comp_ptr);

    return comp_ptr;

}

display_comp_t* display_line ( uint8_t pos,
        uint8_t brightness, uint16_t blink_interval,
        uint8_t length) {

    display_comp_t *comp_ptr = comp_alloc();

    comp_ptr->type = dispt_point;
    comp_ptr->on = true;
    comp_ptr->brightness = brightness;
    comp_ptr->blink_interval = blink_interval;
    comp_ptr->pos = pos;
    comp_ptr->length = length;

    comp_ptr->next = comp_ptr->prev = NULL;

    DL_APPEND(head_component_ptr, comp_ptr);

    return comp_ptr;
}


void display_comp_release (display_comp_t *comp_ptr) {
    led_off(comp_ptr->pos);
    comp_ptr->type = dispt_unused;

    DL_DELETE(head_component_ptr, comp_ptr);
}

void display_refresh(void) {

}

void display_tic(void) {
  display_comp_t* comp_ptr;

  DL_FOREACH(head_component_ptr, comp_ptr) {
    draw_comp(comp_ptr);
  }
}

void display_init(void) {

}

// vim: shiftwidth=2
