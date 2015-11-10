/** file:       leds.c
  * modified:   2014-10-03 17:11:10
  * author:     Richard Bryan
  */


//___ I N C L U D E S ________________________________________________________
#include "leds.h"
#include <string.h>

#define SEGMENTS_H
//#define BANKS_H

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define SEGMENT_COUNT           12
#define BANK_COUNT              5

/* values for configuring the fastest clock interval */
#define VISION_PERSIST_MS   15      /* interval before noticeable blink */
#define TC_FREQ_MHz         8       /* clock for the TC module */


#define BANK_SELECT_TIMER   TC4
   /* counts from 0 to 4, does not trigger an interrupt */

//#define PWM_SELECT_TIMER    AVR32_TC.bank[1]
  /* counts from 0 to MAX_BRIGHT_VAL, triggers the tc_bank_isr interrupt */

#define PWM_BASE_TIMER      TC3
  /* configured to count out VISION_PERSIST_MS / ( BANK_COUNT * 2**BRIGHT_LEVELS ),
   * triggers the tc_pwm_isr
   */


//FIXME
#define CONF_EVENT_BANK_INC_GEN_ID       EVSYS_ID_GEN_TC3_OVF
#define CONF_EVENT_BANK_INC_USER_ID            EVSYS_ID_USER_TC4_EVU

#define CONF_EVENT_BRIGHT_INC_GEN_ID       EVSYS_ID_GEN_TC4_MCX_4
#define CONF_EVENT_BRIGHT_INC_USER_ID            EVSYS_ID_USER_TC5_EVU

/* return the bank/segment ID for the given led index */
#define LED_SEGMENT(led_index)      ( led_index / BANK_COUNT )

/* LED Bank numbering is 0,1,2,3,4,0,4,3,2,1,0,....
 * therefore calculating the bank # for a given LED # is
 * tricky.  If the led's segment is odd, then the bank
 * is simply the led # modulo 5.  Otherwise, it is
 * either 0 (if its one of the 'hour' tick leds) or 5 minus
 * the led # modulo 5
 */
#define LED_BANK(i)                 \
  ( LED_SEGMENT(i) % 2 ? (i % 5 ? (5-(i % 5)) : 0 ) : i % 5 )


#define BANK_GPIO( bank_number ) LED_BANK_GPIO_PINS[bank_number]
#define SEGMENT_GPIO( seg_number ) LED_SEGMENT_GPIO_PINS[seg_number]

#ifdef BANKS_H
/* Banks are active high */
#define BANK_ENABLE( bank_number )          \
  PORTA.OUTSET.reg = 1UL << BANK_GPIO( bank_number )

#define BANK_DISABLE( bank_number )         \
  PORTA.OUTCLR.reg = 1UL << BANK_GPIO( bank_number )

#define BANKS_CLEAR() \
    port_group_set_output_level(&PORTA, BANK_PIN_PORT_MASK, 0 )


#else

/* Banks are active low */
#define BANK_ENABLE( bank_number )          \
  PORTA.OUTCLR.reg = 1UL << BANK_GPIO( bank_number )

#define BANK_DISABLE( bank_number )         \
  PORTA.OUTSET.reg = 1UL << BANK_GPIO( bank_number )

#define BANKS_CLEAR() \
    port_group_set_output_level(&PORTA, BANK_PIN_PORT_MASK, 0xffffffff )

#endif

#ifdef SEGMENTS_H

/* Segments are active high */
#define SEGMENTS_CLEAR() \
    port_group_set_output_level(&PORTA, SEGMENT_PIN_PORT_MASK, 0 )

#define SEGMENT_ENABLE( segment_number )    \
    port_pin_set_output_level( SEGMENT_GPIO( segment_number ), true )

#define SEGMENT_DISABLE( segment_number )   \
    port_pin_set_output_level( SEGMENT_GPIO ( segment_number ), false )

#else
/* Segments are active low */
#define SEGMENT_ENABLE( segment_number )    \
    port_pin_set_output_level( SEGMENT_GPIO( segment_number ), false )

#define SEGMENT_DISABLE( segment_number )   \
    port_pin_set_output_level( SEGMENT_GPIO ( segment_number ), true )

#define SEGMENTS_CLEAR() \
    port_group_set_output_level(&PORTA, SEGMENT_PIN_PORT_MASK, 0xffffffff )
#endif

#define BANKS_SEGMENTS_CLEAR() \
  BANKS_CLEAR();SEGMENTS_CLEAR()

#define SEGMENT_PIN_PORT_MASK ( \
      1 << PIN_PA16	|	\
      1 << PIN_PA15	|	\
      1 << PIN_PA14	|	\
      1 << PIN_PA11	|	\
      1 << PIN_PA07	|	\
      1 << PIN_PA06	|	\
      1 << PIN_PA05	|	\
      1 << PIN_PA04	|	\
      1 << PIN_PA28	|	\
      1 << PIN_PA27	|	\
      1 << PIN_PA22	|	\
      1 << PIN_PA19 )

#define BANK_PIN_PORT_MASK (    \
      1 << PIN_PA17     |       \
      1 << PIN_PA18     |       \
      1 << PIN_PA25     |       \
      1 << PIN_PA24     |       \
      1 << PIN_PA23 )





#define CURRENT_BANK() \
        ((Tc *) BANK_SELECT_TIMER)->COUNT8.COUNT.reg;

#define BANK_IS_SYNCING() \
        (((Tc *) BANK_SELECT_TIMER)->COUNT8.STATUS.reg & TC_STATUS_SYNCBUSY)


//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________
static void tc_pwm_isr ( struct tc_module *const tc_instance);
  /* @brief initialize led module
   * @param None
   * @retrn None
   */

static void configure_tc ( void );
  /* @brief configure the timer / counter
   * @param None
   * @retrn None
   */


//___ V A R I A B L E S ______________________________________________________
const uint8_t LED_BANK_GPIO_PINS[BANK_COUNT] = {
  PIN_PA17,
  PIN_PA18,
  PIN_PA25,
  PIN_PA24,
  PIN_PA23
};

const uint8_t LED_SEGMENT_GPIO_PINS[SEGMENT_COUNT] = {
  PIN_PA16,
  PIN_PA15,
  PIN_PA14,
  PIN_PA11,
  PIN_PA07,
  PIN_PA06,
  PIN_PA05,
  PIN_PA04,
  PIN_PA28,
  PIN_PA27,
  PIN_PA22,
  PIN_PA19
};

static struct tc_module pwm_tc_instance;
static struct tc_module bank_tc_instance;

static struct events_resource bank_inc_event;

#define BRIGHT_INDEX_MAX    BRIGHT_LEVELS - 1
static uint8_t bright_index;      // current brightness level
static uint16_t brightness_ctr = 1;      // counter for incrementing bright index
static uint8_t bank_ctr = BANK_COUNT;
static uint8_t max_brightness = MAX_BRIGHT_VAL;
static uint8_t led_intensities[ BANK_COUNT ][ SEGMENT_COUNT ];
static uint32_t led_segment_masks[ BANK_COUNT ][ BRIGHT_LEVELS ];

//___ I N T E R R U P T S  ___________________________________________________
static void tc_pwm_isr ( struct tc_module *const tc_inst) {

  /* Getting the current bank index from the timer counter
   * actually takes longer than maintaining our own global
   * variable.  This is probably related to synchronization
   * issues that should be investigated */
  //bank = tc_get_count_value(&bank_tc_instance);
  bank_ctr--;

  /* switch to next bank */
  BANKS_SEGMENTS_CLEAR();

  /* Enable (toggle low) the specific led segments applying mask to "clear" register */
#ifdef SEGMENTS_H
  PORTA.OUTSET.reg  = led_segment_masks[bank_ctr][bright_index];
#else
  PORTA.OUTCLR.reg  = led_segment_masks[bank_ctr][bright_index];
#endif

  BANK_ENABLE( bank_ctr );


  /* Switch to next brightness level after each full bank cycle */
  if (bank_ctr == 0) {
    brightness_ctr--;
    bank_ctr = BANK_COUNT;

    if (!brightness_ctr) {
        /* a full cycle of all leds at this brightness level has occured */
        bright_index++;
        brightness_ctr = 1 << bright_index;

        /* Check if all brightness levels have been cycled over and
         * reset if necessary */
        if (bright_index > BRIGHT_INDEX_MAX) {
            bright_index = 0;
            brightness_ctr = 1;
        }
    }
  }

}

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________
static void configure_tc ( void ) {
  /** determine the switching frequency for led state:
    *
    * count_period_ns = (VISION_PERSIST_MS * 1e6) / (bank_count * (1<<BRIGHT_LEVELS));
    * TC_period_ns = ( 1 / ( TC_FREQ_MHz * 1e6 ) ) * 1e9;
    * count_top = count_period_ns / TC_period_ns;
    *
    * count_top = ( VISION_PERSIST_MS * ( TC_FREQ_MHz * 1e3 / bank_count ) )
    *                >> BRIGHT_LEVELS
    *
    * Assuming TC_GLOBAL_FREQ_MHz = 8, BANK_COUNT = 5
    *
    * count_top = ( VISION_PERSIST_MS * 1600 ) >> BRIGHT_LEVELS
    *
    * Assuming BRIGHT_LEVELS = 4, VISION_PERSIST_MS = 10
    *
    * count_top = 10 * 1600 >> 4 = 10 * 100 = 1000
    */
  const uint32_t count_period_ns = 1e6 * VISION_PERSIST_MS / \
                                   ( BANK_COUNT * (1<<BRIGHT_LEVELS) );
  const uint32_t TC_period_ns = 1e3 / TC_FREQ_MHz;
  const uint16_t pwm_base_count_top = (uint16_t) (count_period_ns / TC_period_ns);

  struct tc_config config_tc;
  struct tc_events events_tc;
  struct events_config config_ev;

  /* Configure highest frequency counter for led PWM */
  tc_get_config_defaults( &config_tc );
  config_tc.counter_size    = TC_COUNTER_SIZE_16BIT;
  config_tc.wave_generation = TC_WAVE_GENERATION_MATCH_FREQ;
  config_tc.counter_16_bit.compare_capture_channel[0] = pwm_base_count_top;
  config_tc.run_in_standby = false;
  config_tc.clock_source = GCLK_GENERATOR_0;

  tc_init(&pwm_tc_instance, PWM_BASE_TIMER, &config_tc);

  tc_register_callback( &pwm_tc_instance,
      tc_pwm_isr, TC_CALLBACK_CC_CHANNEL0);

  /* Configure bank counter incremented by base pwm counter events */
  tc_get_config_defaults( &config_tc );
  config_tc.counter_size = TC_COUNTER_SIZE_8BIT;
  config_tc.counter_8_bit.period = BANK_COUNT;
  config_tc.counter_8_bit.value = 0;

  tc_init(&bank_tc_instance, BANK_SELECT_TIMER, &config_tc);

  /* Configure base PWM timer to generate bank timer increment events */
  //events_tc.generate_event_on_compare_channel[0] = true;
  events_tc.generate_event_on_overflow = true;
  tc_enable_events(&pwm_tc_instance, &events_tc);

  /* Configure bank timer to increment on incoming events */
  events_tc.generate_event_on_compare_channel[0] = true;
  events_tc.generate_event_on_compare_channel[0] = false;
  events_tc.event_action = TC_EVENT_ACTION_INCREMENT_COUNTER;
  events_tc.on_event_perform_action = true;

  tc_enable_events(&bank_tc_instance, &events_tc);

  /* Allocate and attach pwm->bank timer inc event */
  events_get_config_defaults(&config_ev);

  config_ev.path           = EVENTS_PATH_SYNCHRONOUS;
  config_ev.generator      = CONF_EVENT_BANK_INC_GEN_ID;

  events_allocate(&bank_inc_event, &config_ev);
  events_attach_user(&bank_inc_event, CONF_EVENT_BANK_INC_USER_ID);
}


//___ F U N C T I O N S ______________________________________________________

void led_controller_init ( void ) {

  configure_tc();
}

void led_controller_enable ( void ) {
  struct port_config pin_conf;

  /* Configure bank and segment pin groups as outputs */
  port_get_config_defaults(&pin_conf);
  pin_conf.direction = PORT_PIN_DIR_OUTPUT;

  port_group_set_config(&PORTA, SEGMENT_PIN_PORT_MASK, &pin_conf );
  port_group_set_config(&PORTA, BANK_PIN_PORT_MASK, &pin_conf );

  led_clear_all();

  /* Errata 12227: perform a software reset of tc after waking up */
  //tc_reset(&pwm_tc_instance);
  //tc_reset(&bank_tc_instance);
  
  //configure_tc();
    
  tc_enable(&pwm_tc_instance);

  tc_enable(&bank_tc_instance);
  
  tc_enable_callback(&pwm_tc_instance, TC_CALLBACK_CC_CHANNEL0);
  /* The bank tc shouldnt run -- it should only increment on pwm events
   * therefore, stop and reset its value immediately
   */
  tc_stop_counter(&bank_tc_instance);
  tc_set_count_value(&bank_tc_instance, 0);

}

void led_controller_disable ( void ) {
  struct port_config pin_conf;

  tc_disable_callback(&pwm_tc_instance, TC_CALLBACK_CC_CHANNEL0);
  tc_disable(&pwm_tc_instance);
  tc_disable(&bank_tc_instance);

  SEGMENTS_CLEAR();
  BANKS_CLEAR();

  /* Configure led pins with powersave */
  port_get_config_defaults(&pin_conf);
  pin_conf.powersave = true;
  port_group_set_config(&PORTA, SEGMENT_PIN_PORT_MASK, &pin_conf );
  port_group_set_config(&PORTA, BANK_PIN_PORT_MASK, &pin_conf );

}


void led_set_intensity ( uint8_t led, uint8_t intensity ) {
  uint8_t bank = LED_BANK( led );
  uint8_t segment = LED_SEGMENT( led );
  uint8_t segment_gpio = SEGMENT_GPIO(segment);
  uint8_t i;



  led_intensities[bank][segment] = intensity;

  for (i = 0; i < BRIGHT_LEVELS; i++) {
      if (intensity > i && i < max_brightness)
        led_segment_masks[ bank ][ i ] |= 1UL << segment_gpio;
      else
        led_segment_masks[ bank ][ i ] &= ~(1UL << segment_gpio);

  }

}


void led_clear_all( void ) {
  /* clear (disable) all active leds */
  BANKS_SEGMENTS_CLEAR();
  memset(led_segment_masks, 0, BANK_COUNT*BRIGHT_LEVELS*sizeof(uint32_t));
}

void led_set_max_brightness( uint8_t brightness ) {
    max_brightness = brightness;
}


void _led_on_full( uint8_t led ) {
  SEGMENT_ENABLE(LED_SEGMENT(led));
  BANK_ENABLE(LED_BANK(led));
}

void _led_off_full( uint8_t led ) {
  SEGMENT_DISABLE(LED_SEGMENT(led));
  BANK_DISABLE(LED_BANK(led));
}

// vim:shiftwidth=2
