/** file:       leds.c
  * modified:   2014-10-03 17:11:10
  * author:     Richard Bryan
  */


//___ I N C L U D E S ________________________________________________________
#include "leds.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define SEGMENT_COUNT           12
#define BANK_COUNT              5

/* values for configuring the fastest clock interval */
#define VISION_PERSIST_MS   20      /* time for vision persistance */
#define TC_FREQ_MHz         8       /* clock for the TC module */

//#define BANK_SELECT_TIMER   AVR32_TC.bank[0]
   /* counts from 0 to 4, does not trigger an interrupt */

//#define PWM_SELECT_TIMER    AVR32_TC.bank[1]
  /* counts from 0 to 15, triggers the tc_bank_isr interrupt */

//#define PWM_BASE_TIMER      AVR32_TC.bank[2]
  /* configured to count out 10ms / ( BANK_COUNT * 2**BRIGHT_RES ),
   * triggers the tc_pwm_isr -- this assumes that 10ms is the vision
   * persistence duration
   */

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

#define BANK_ENABLE( bank_number )          \
    port_pin_set_output_level( LED_BANK_GPIO_PINS[bank_number], false )

#define BANK_DISABLE( bank_number )         \
    port_pin_set_output_level( LED_BANK_GPIO_PINS[bank_number], true )

#define SEGMENT_ENABLE( segment_number )    \
    port_pin_set_output_level( LED_SEGMENT_GPIO_PINS[segment_number], false )

#define SEGMENT_DISABLE( segment_number )   \
    port_pin_set_output_level( LED_SEGMENT_GPIO_PINS[segment_number], true )


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

#define SEGMENTS_CLEAR() \
    port_group_set_output_level(PORT->Group, SEGMENT_PIN_PORT_MASK, 0xffffffff )

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________
typedef union {
  struct {
    uint8_t   blink     : BLINK_RES;    /* 0, no blink to max blink */
    uint8_t   bright    : BRIGHT_RES;    /* led intensity */
  };
  uint8_t led_state;
} led_status_t;

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________
static void tc_bright_isr ( struct tc_module *const tc_instance);
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
const uint32_t LED_BANK_GPIO_PINS[BANK_COUNT] = {
  PIN_PA17,
  PIN_PA18,
  PIN_PA25,
  PIN_PA24,
  PIN_PA23
};

const uint32_t LED_SEGMENT_GPIO_PINS[SEGMENT_COUNT] = {
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

static struct tc_module tc_instance;
static volatile uint8_t led_bank;           // current bank value
static volatile uint8_t led_bright_tc_num;      // current brightness
  /* manage led intensity number and bank number */
  // TODO : TC should be used to increment these values but need to use the
  // event managment system to configure 2 additional counters, once TC are
  // used then led_bright_tc_num and led_bank can be set by reading the
  // current count value

static led_status_t led_status[ BANK_COUNT ][ SEGMENT_COUNT ];// = { {0} };


//___ I N T E R R U P T S  ___________________________________________________
static void tc_bright_isr ( struct tc_module *const tc_inst) {
  uint8_t segment;


  /* switch to next bank */
  BANK_DISABLE( led_bank );

  /* disable all segments because we will now switch banks */
  SEGMENTS_CLEAR();

  led_bank++;
  led_bank = led_bank % BANK_COUNT;

  BANK_ENABLE( led_bank );

  /* turn on the relevant segments */
  for( segment = 0; segment < SEGMENT_COUNT; segment++ ) {
    if( led_status[ led_bank ][ segment ].bright > led_bright_tc_num ) {
      SEGMENT_ENABLE( segment );
    }
  }


  /* Switch to next brightness level after each full bank cycle */
  if (led_bank == 0) {
    led_bright_tc_num++;
    led_bright_tc_num = led_bright_tc_num % (1 << BRIGHT_RES);
  }

}

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________
static void configure_tc ( void ) {
  /** determine the switching frequency for led state:
    *
    * count_period_ns = (VISION_PERSIST_MS * 1e6) / (bank_count * (1<<BRIGHT_RES));
    * TC_period_ns = ( 1 / ( TC_FREQ_MHz * 1e6 ) ) * 1e9;
    * count_top = count_period_ns / TC_period_ns;
    *
    * count_top = ( VISION_PERSIST_MS * ( TC_FREQ_MHz * 1e3 / bank_count ) )
    *                >> BRIGHT_RES
    *
    * Assuming TC_GLOBAL_FREQ_MHz = 8, BANK_COUNT = 5
    *
    * count_top = ( VISION_PERSIST_MS * 1600 ) >> BRIGHT_RES
    *
    * Assuming BRIGHT_RES = 4, VISION_PERSIST_MS = 10
    *
    * count_top = 10 * 1600 >> 4 = 10 * 100 = 1000
    */

  const uint32_t count_period_ns = 1e6 * VISION_PERSIST_MS / \
                                   ( BANK_COUNT * (1<<BRIGHT_RES) );
  const uint32_t TC_period_ns = 1e3 / TC_FREQ_MHz;
  const uint16_t count_top = (uint16_t) (count_period_ns / TC_period_ns);

  struct tc_config config_tc;

  tc_get_config_defaults( &config_tc );
  config_tc.counter_size    = TC_COUNTER_SIZE_16BIT;
  config_tc.wave_generation = TC_WAVE_GENERATION_MATCH_FREQ;
  config_tc.counter_16_bit.compare_capture_channel[0] = count_top;
  config_tc.run_in_standby = true;
  config_tc.clock_source = GCLK_GENERATOR_0;

  tc_init(&tc_instance, TC0, &config_tc);



  tc_register_callback( &tc_instance,
      tc_bright_isr, TC_CALLBACK_CC_CHANNEL0);

  tc_enable_callback(&tc_instance, TC_CALLBACK_CC_CHANNEL0);

  tc_enable(&tc_instance);
//  tc_start_counter(&tc_instance);
}


//___ F U N C T I O N S ______________________________________________________
void led_init ( void ) {
  uint8_t bank;
  uint8_t segment;
  struct port_config pin_conf;

  led_bright_tc_num = 0;
  led_bank = 0;

  port_get_config_defaults(&pin_conf);
  pin_conf.direction = PORT_PIN_DIR_OUTPUT;

  for( bank = 0; bank < BANK_COUNT; bank++ ) {
    port_pin_set_config( LED_BANK_GPIO_PINS[bank], &pin_conf );
    BANK_DISABLE( bank );
    for( segment = 0; segment < SEGMENT_COUNT; segment++ ) {
      port_pin_set_config( LED_SEGMENT_GPIO_PINS[segment], &pin_conf );
      SEGMENT_DISABLE( segment );
      led_status[ bank ][ segment ].led_state = 0;
    }
  }

  configure_tc();
}
void led_set_state ( uint8_t led_index, uint8_t blink, uint8_t bright ) {
  uint8_t bank_number = LED_BANK( led_index );
  uint8_t segment_number = LED_SEGMENT( led_index );
  led_status_t status;

  status.blink = blink;
  status.bright = bright;

  led_status[ bank_number ][ segment_number ] = status;
}

void led_enable ( uint8_t led ) {
  led_set_state( led, BLINK_DEFAULT, BRIGHT_DEFAULT );
}

void led_disable ( uint8_t led ) {
  led_set_state( led, BLINK_DEFAULT, 0 );
}

void led_set_intensity ( uint8_t led, uint8_t intensity ) {
  led_set_state( led, BLINK_DEFAULT, intensity );
}

void led_disable_all( void ) {
  uint8_t bank, segment;
  for( bank = 0; bank < BANK_COUNT; bank++ ) {
    for( segment = 0; segment < SEGMENT_COUNT; segment++ ) {
        led_status[ bank ][ segment ].led_state = 0;
    }
  }
}

// vim: shiftwidth=2
