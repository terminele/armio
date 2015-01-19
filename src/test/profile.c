/** file:       profile.c
  * creator:   2015-01-19 09:48:43
  * author:     Richard Bryan
  *
  *
  * Used to profile execution time of
  * various sections of code
  */

//___ I N C L U D E S ________________________________________________________
#include "leds.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define MAIN_TIMER  TC5

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________
void timer_init( void );

//___ V A R I A B L E S ______________________________________________________
static struct tc_module main_tc;

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________


void timer_init( void ) {

    struct tc_config config_tc;

    /* Configure main timer counter */
    tc_get_config_defaults( &config_tc );
    config_tc.clock_source = GCLK_GENERATOR_0;
    config_tc.counter_size = TC_COUNTER_SIZE_16BIT;
    //config_tc.clock_prescaler = TC_CLOCK_PRESCALER_DIV8;
    config_tc.wave_generation = TC_WAVE_GENERATION_NORMAL_FREQ;
    config_tc.counter_16_bit.compare_capture_channel[0] = 2048;
    config_tc.counter_16_bit.value = 0;

    tc_init(&main_tc, MAIN_TIMER, &config_tc);

}

void tc_pwm_isr ( struct tc_module *const tc_inst);
//___ F U N C T I O N S ______________________________________________________
int main (void)
{

    system_init();
    system_apb_clock_clear_mask (SYSTEM_CLOCK_APB_APBB, PM_APBAMASK_WDT);
    delay_init();

    led_controller_init();
    led_controller_enable();
    system_interrupt_enable_global();
    timer_init();

    led_on(31, BRIGHT_DEFAULT);

    delay_s(1);
    led_off(31);

    tc_enable(&main_tc);
    tc_pwm_isr(NULL);
    uint32_t val = tc_get_count_value(&main_tc);

    while (1) {
        if (val >= 300) {
            led_on(0, BRIGHT_DEFAULT);
            led_on(30, BRIGHT_DEFAULT);
        }
        else if (val >= 240) {
            led_on(0, BRIGHT_DEFAULT);
            led_on(24, BRIGHT_DEFAULT);
            led_on(val - 240, BRIGHT_DEFAULT);
        }
        else if (val >= 180) {
            led_on(0, BRIGHT_DEFAULT);
            led_on(18, BRIGHT_DEFAULT);
            led_on(val - 180, BRIGHT_DEFAULT);
        } else if (val >= 120){
            led_on(0, BRIGHT_DEFAULT);
            led_on(12, BRIGHT_DEFAULT);
            led_on(val - 120, BRIGHT_DEFAULT);
        } else if (val >= 60) {
            led_on(0, BRIGHT_DEFAULT);
            led_on(6, BRIGHT_DEFAULT);
            led_on(7, BRIGHT_DEFAULT);
            led_on(val - 60, BRIGHT_DEFAULT);
        } else {
            led_on(0, BRIGHT_DEFAULT);
            led_on(1, BRIGHT_DEFAULT);
            led_on(val, BRIGHT_DEFAULT);
        }

    }
}
