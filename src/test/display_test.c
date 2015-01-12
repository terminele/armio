/**
 * \file main.c
 *
 * \brief main application entry point for armio
 *
 */

#include <asf.h>
#include "leds.h"
#include "display.h"
#include "main.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________

#define MAIN_TIMER  TC5

#define MAIN_TIMER_TICK_US      1000

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________


//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

void main_tic ( void );
  /* @brief main control loop update function
   * @param None
   * @retrn None
   */

void main_init( void );
  /* @brief initialize main control module
   * @param None
   * @retrn None
   */

//___ V A R I A B L E S ______________________________________________________
static struct tc_module main_tc;

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

void main_tic( void ) {
    static uint32_t tick_cnt = 0;
    static display_comp_t* display_comp = NULL;
    static int8_t polycount = 3;

    tick_cnt++;

    if (!display_comp)
        display_comp = display_line(0, BRIGHT_DEFAULT, BLINK_NONE, 5);

    if (tick_cnt % 20 == 0)
        display_comp_update_pos(display_comp, (display_comp->pos + 1) % 60);

    if (tick_cnt % 1200 == 1199) {
        display_comp_release(display_comp);
        //display_comp = display_point(polycount, BRIGHT_DEFAULT, BLINK_NONE);
        display_comp = display_polygon(0, BRIGHT_DEFAULT, BLINK_NONE, polycount);
        polycount++;
    }


}

//___ F U N C T I O N S ______________________________________________________


void main_terminate_in_error ( uint8_t error_code ) {

    /* Display error code led */
    led_clear_all();

    /* loop indefinitely */
    while(1) {
        led_on(error_code, BRIGHT_DEFAULT);
        delay_ms(100);
        led_off(error_code);
        delay_ms(100);
    }
}

void main_init( void ) {

    struct tc_config config_tc;

    /* Configure main timer counter */
    tc_get_config_defaults( &config_tc );
    config_tc.clock_source = GCLK_GENERATOR_0;
    config_tc.counter_size = TC_COUNTER_SIZE_16BIT;
    config_tc.clock_prescaler = TC_CLOCK_PRESCALER_DIV8; //give 1us count for 8MHz clock
    config_tc.wave_generation = TC_WAVE_GENERATION_MATCH_FREQ;
    config_tc.counter_16_bit.compare_capture_channel[0] = MAIN_TIMER_TICK_US;
    config_tc.counter_16_bit.value = 0;

    tc_init(&main_tc, MAIN_TIMER, &config_tc);
    tc_enable(&main_tc);

}

int main (void)
{

    system_init();
    system_apb_clock_clear_mask (SYSTEM_CLOCK_APB_APBB, PM_APBAMASK_WDT);
    system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);

    /* Errata 39.3.2 -- device may not wake up from
     * standby if nvm goes to sleep.  May not be necessary
     * for samd21e15/16 if revision E
     */
    //NVMCTRL->CTRLB.bit.SLEEPPRM = NVMCTRL_CTRLB_SLEEPPRM_DISABLED_Val;

    delay_init();
    main_init();
    led_controller_init();
    led_controller_enable();
    display_init();


    /* Show a startup LED swirl */
    //display_swirl(10, 200, 2, 64);
    led_on(0, BRIGHT_DEFAULT);
    delay_ms(500);
    led_off(0);

    system_interrupt_enable_global();

    while (1) {
        if (tc_get_status(&main_tc) & TC_STATUS_COUNT_OVERFLOW) {
            tc_clear_status(&main_tc, TC_STATUS_COUNT_OVERFLOW);
            main_tic();
            display_tic();
        }
    }

}
