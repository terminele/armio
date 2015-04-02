/** file:   display.c
  * modified:   2015-01-09 16:16:54
  * author:     Richard Bryan
  *
  * the display module is responsible for 'drawing' higher level
  * static shapes to the led rings.  It maintains a stack of
  * display components (e.g. points, lines, polygons) of varying
  * brightness levels .  On each call to tic(), these
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
#define MAX_ALLOCATIONS     10
#define MOD(a,b) (((a) % (b)) < 0 ? (((a) % (b)) + (b)) : ((a) % (b)))
//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________


display_comp_t* comp_alloc ( void );
  /* @brief allocate a new display component
   * @param None
   * @retrn the new component
   */

void comp_free ( display_comp_t* ptr );
  /* @brief free a display component
   * @param pointer to comp to free
   * @retrn None
   */

void comp_leds_clear( display_comp_t *comp );
  /* @brief turn off leds corresponding to given component
   * @param component to clear
   * @retrn None
   */

void comp_draw( display_comp_t* comp_ptr);
  /* @brief draws the given component to the display
   *    (i.e. sets the led state(s) comprising the
   *    component)
   * @param component to draw
   * @retrn None
   */



//___ V A R I A B L E S ______________________________________________________

/* statically allocate maximum number of display components */
static display_comp_t component_allocs[MAX_ALLOCATIONS];


/* pointer to head of active component list */
static display_comp_t *head_component_ptr = NULL;


//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

display_comp_t *comp_alloc( void ) {
    uint8_t i;

    for (i=0; i < MAX_ALLOCATIONS; i++) {
        if (component_allocs[i].type == dispt_unused)
            return component_allocs + i;
    }

    main_terminate_in_error(ERROR_DISP_ALLOC_FAIL);
    return NULL;
}

void comp_free ( display_comp_t* ptr ) {
    ptr->type = dispt_unused;
}


void comp_draw( display_comp_t* comp) {
    int32_t tmp, pos, pos_end;
    uint8_t bright = comp->brightness;
    if (!comp->on) {
      comp_leds_clear(comp);
      return;
    }

    switch(comp->type) {
      case dispt_point:
        led_on(comp->pos, comp->brightness);
        break;
      case dispt_snake:
      case dispt_line:
        pos = comp->cw ? MOD(comp->pos + comp->length - 1, 60) : \
              MOD(comp->pos - comp->length + 1, 60);
        do {
          led_on(pos, bright);
          if (pos == comp->pos) return;

          pos = comp->cw ? MOD(pos - 1, 60) : MOD(pos + 1, 60);
          if (comp->type == dispt_snake &&
                  bright > MIN_BRIGHT_VAL)
              bright--;

        } while (true);

        break;
      case dispt_polygon:
        for (tmp = 0; tmp < comp->length; tmp++) {
            pos = (comp->pos + ((tmp*60)/comp->length)) % 60;
            led_on(pos, comp->brightness);
        }
        break;
      default:
        main_terminate_in_error( ERROR_DISP_DRAW_BAD_COMP_TYPE );
        break;
    }
}


void comp_leds_clear(  display_comp_t *comp ) {
    int32_t tmp, pos, pos_end;
    switch(comp->type) {
      case dispt_point:
        led_off(comp->pos);
        break;
      case dispt_snake:
      case dispt_line:

        pos = MOD(comp->pos, 60);
        pos_end = comp->cw ? MOD(comp->pos + comp->length, 60) : \
              MOD(comp->pos - comp->length, 60);
        do {

            led_off(pos);
            pos = comp->cw ? MOD(pos + 1, 60) : MOD(pos - 1, 60);

        } while (pos != pos_end);

        break;
      case dispt_polygon:
        for (tmp = 0; tmp < comp->length; tmp++) {
            pos = comp->pos + ((tmp*60)/comp->length);
            led_off(pos % 60);
        }
        break;
      default:
        main_terminate_in_error( ERROR_DISP_CLEAR_BAD_COMP_TYPE );
        break;
    }

}

//___ F U N C T I O N S ______________________________________________________


display_comp_t* display_point ( int8_t pos,
        uint8_t brightness ) {

    display_comp_t *comp_ptr = comp_alloc();

    comp_ptr->type = dispt_point;
    comp_ptr->on = true;
    comp_ptr->brightness = brightness;
    comp_ptr->pos = pos;
    comp_ptr->length = 1;

    comp_ptr->next = comp_ptr->prev = NULL;

    DL_APPEND(head_component_ptr, comp_ptr);

    return comp_ptr;

}

display_comp_t* display_line ( int8_t pos,
        uint8_t brightness, int8_t length) {

    display_comp_t *comp_ptr = comp_alloc();

    comp_ptr->type = dispt_line;
    comp_ptr->on = true;
    comp_ptr->brightness = brightness;
    comp_ptr->pos = pos;
    comp_ptr->length = length;
    comp_ptr->cw = true;

    comp_ptr->next = comp_ptr->prev = NULL;

    DL_APPEND(head_component_ptr, comp_ptr);

    return comp_ptr;
}

display_comp_t* display_snake ( int8_t pos,
        uint8_t brightness, int8_t length, bool clockwise) {

    display_comp_t *comp_ptr = comp_alloc();

    comp_ptr->type = dispt_snake;
    comp_ptr->on = true;
    comp_ptr->brightness = brightness;
    comp_ptr->pos = pos;
    comp_ptr->length = length;
    comp_ptr->cw = clockwise;

    comp_ptr->next = comp_ptr->prev = NULL;

    DL_APPEND(head_component_ptr, comp_ptr);

    return comp_ptr;
}



display_comp_t* display_polygon ( int8_t pos,
        uint8_t brightness, int8_t num_sides) {

    display_comp_t *comp_ptr = comp_alloc();

    comp_ptr->type = dispt_polygon;
    comp_ptr->on = true;
    comp_ptr->brightness = brightness;
    comp_ptr->pos = pos;
    comp_ptr->length = num_sides;

    comp_ptr->next = comp_ptr->prev = NULL;

    DL_APPEND(head_component_ptr, comp_ptr);

    return comp_ptr;
}

void display_comp_hide (display_comp_t *comp) {
    comp->on = false;
    comp_leds_clear(comp);
}

void display_comp_hide_all ( void ) {
  display_comp_t* comp_ptr;

  DL_FOREACH(head_component_ptr, comp_ptr) {
    comp_ptr->on = false;
  }

  led_clear_all();

}


void display_comp_show_all ( void ) {
  display_comp_t* comp_ptr;

  DL_FOREACH(head_component_ptr, comp_ptr) {
    comp_ptr->on = true;
  }

}

void display_comp_update_pos ( display_comp_t *comp, int8_t pos ) {
    if (pos == comp->pos)
        return;

    comp_leds_clear(comp);
    comp->pos = pos;

}

void display_comp_update_length ( display_comp_t *comp, int8_t length ) {

    if (comp->type != dispt_line && comp->type != dispt_snake)
        return;

    if (comp->length == length)
        return;

    comp_leds_clear(comp);
    comp->length = length;

}

void display_relative( display_comp_t* line_ptr, uint8_t origin,
        int8_t value) {

    comp_leds_clear(line_ptr);
    if (value >= 0) {
        line_ptr->length = value + 1;
        line_ptr->pos = (origin + value) % 60;
    } else {
        value *= -1;
        line_ptr->length = value + 1;
        line_ptr->pos = origin;

    }
}

void display_comp_release (display_comp_t *comp_ptr) {
    if (!comp_ptr) return;
    comp_leds_clear(comp_ptr);
    DL_DELETE(head_component_ptr, comp_ptr);
    comp_free(comp_ptr);
}

void display_refresh(void) {

}

void display_tic(void) {
  display_comp_t* comp_ptr;

  DL_FOREACH(head_component_ptr, comp_ptr) {
    comp_draw(comp_ptr);
  }
}

void display_init(void) {
    int i;
    for (i=0; i < MAX_ALLOCATIONS; i++) {
        component_allocs[i].type = dispt_unused;
    }
}
