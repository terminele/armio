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
#define VISION_PERSIST_MS   20      /* interval before noticeable blink */
#define TC_FREQ_MHz         8       /* clock for the TC module */


//#define BANK_SELECT_TIMER   AVR32_TC.bank[0]
   /* counts from 0 to 4, does not trigger an interrupt */

//#define PWM_SELECT_TIMER    AVR32_TC.bank[1]
  /* counts from 0 to MAX_BRIGHT_VAL, triggers the tc_bank_isr interrupt */

//#define PWM_BASE_TIMER      AVR32_TC.bank[2]
  /* configured to count out VISION_PERSIST_MS / ( BANK_COUNT * 2**BRIGHT_RES ),
   * triggers the tc_pwm_isr
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


#define BANK_GPIO( bank_number ) LED_BANK_GPIO_PINS[bank_number]
#define SEGMENT_GPIO( seg_number ) LED_SEGMENT_GPIO_PINS[seg_number]

#define BANK_ENABLE( bank_number )          \
  PORTA.OUTCLR.reg = 1UL << BANK_GPIO( bank_number )

#define BANK_DISABLE( bank_number )         \
  PORTA.OUTSET.reg = 1UL << BANK_GPIO( bank_number )

#define SEGMENT_ENABLE( segment_number )    \
    port_pin_set_output_level( SEGMENT_GPIO( segment_number ), false )

#define SEGMENT_DISABLE( segment_number )   \
    port_pin_set_output_level( SEGMENT_GPIO ( segment_number ), true )




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

#define SEGMENTS_CLEAR() \
    port_group_set_output_level(&PORTA, SEGMENT_PIN_PORT_MASK, 0xffffffff )

#define BANKS_CLEAR() \
    port_group_set_output_level(&PORTA, BANK_PIN_PORT_MASK, 0xffffffff )


//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________
typedef union {
  struct {
    /* led blink interval in units of 4*VISION_PERSIST_MS */
    uint8_t   blink     : BLINK_RES;

    /* led intensity level -- duty cycle between 1/15 to 1      */
    /* max intensity is 20% duty cycle (due to 5 bank cycling)  */
    /* intensity level divides this down by up to a 16th        */
    /* 0 --> led is disabled                                    */
    uint8_t   bright    : BRIGHT_RES;
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
static uint8_t bank_ctr;           // current bank value
static uint8_t brightness_ctr;      // current brightness level
static uint16_t blink_ctr;      // current blink value

  /* manage led intensity number and bank number */
  // TODO : TC should be used to increment these values but need to use the
  // event managment system to configure 2 additional counters, once TC are
  // used then brightness_ctr and bank_ctr can be set by reading the
  // current count value

static led_status_t led_status[ BANK_COUNT ][ SEGMENT_COUNT ];// = { {0} };


//___ I N T E R R U P T S  ___________________________________________________
static void tc_bright_isr ( struct tc_module *const tc_inst) {
  uint8_t segment;
  uint8_t prev_bank;
  uint32_t segment_enable_mask = 0;

  prev_bank = bank_ctr;


  /* switch to next bank */
  BANK_DISABLE( bank_ctr );

  /* disable all segments because we will now switch banks */
  SEGMENTS_CLEAR();

  bank_ctr++;
  bank_ctr = bank_ctr % BANK_COUNT;


  BANK_ENABLE( bank_ctr );

  /* turn on the relevant segments */
  for( segment = 0; segment < SEGMENT_COUNT; segment++ ) {
    led_status_t status = led_status[ bank_ctr ][ segment ];
    if (status.bright && status.bright >= brightness_ctr) {
        if (!status.blink || blink_ctr % (BLINK_FACTOR*status.blink) < BLINK_FACTOR*status.blink/2) {
          segment_enable_mask |= 1UL << SEGMENT_GPIO(segment);
        }
      }
  }


  /* Enable (toggle low) the specific led segments applying mask to "clear" register */
  PORTA.OUTCLR.reg  = segment_enable_mask;

  /* Switch to next brightness level after each full bank cycle */
  if (bank_ctr == 0) {
    brightness_ctr++;

    if (brightness_ctr == MAX_BRIGHT_VAL ) {
      /* a full cycle of all leds at all brightness leds has    */
      /* completed (this should have taken VISION_PERSIST_MS)   */
      /* reset brightness counter and increment blink counter   */
        brightness_ctr = 0;
        blink_ctr++;
        blink_ctr = blink_ctr % MAX_BLINK_VAL;
    }
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
  config_tc.run_in_standby = false;
  config_tc.clock_source = GCLK_GENERATOR_0;

  tc_init(&tc_instance, TC3, &config_tc);

  tc_register_callback( &tc_instance,
      tc_bright_isr, TC_CALLBACK_CC_CHANNEL0);



}


//___ F U N C T I O N S ______________________________________________________

void led_controller_init ( void ) {

  brightness_ctr = 0;
  blink_ctr = 0;
  bank_ctr = 0;

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

  system_apb_clock_set_mask(SYSTEM_CLOCK_APB_APBC, PM_APBCMASK_TC3);
  tc_enable_callback(&tc_instance, TC_CALLBACK_CC_CHANNEL0);
  tc_enable(&tc_instance);

}

void led_controller_disable ( void ) {
  struct port_config pin_conf;

  tc_disable_callback(&tc_instance, TC_CALLBACK_CC_CHANNEL0);
  tc_disable(&tc_instance);

  SEGMENTS_CLEAR();
  BANKS_CLEAR();

  /* Configure led pins with powersave */
  port_get_config_defaults(&pin_conf);
  pin_conf.powersave = true;
  port_group_set_config(&PORTA, SEGMENT_PIN_PORT_MASK, &pin_conf );
  port_group_set_config(&PORTA, BANK_PIN_PORT_MASK, &pin_conf );

  system_apb_clock_clear_mask(SYSTEM_CLOCK_APB_APBC, PM_APBCMASK_TC3);

}


void led_set_state ( uint8_t led_index, uint8_t blink, uint8_t bright ) {
  uint8_t bank_number = LED_BANK( led_index );
  uint8_t segment_number = LED_SEGMENT( led_index );

  led_status[ bank_number ][ segment_number ].blink = blink;
  led_status[ bank_number ][ segment_number ].bright = bright;
}

void led_on ( uint8_t led ) {

  led_set_state( led, BLINK_DEFAULT, BRIGHT_DEFAULT );
}

void led_off ( uint8_t led ) {
  led_set_state( led, BLINK_DEFAULT, 0 );
}

void led_set_intensity ( uint8_t led, uint8_t intensity ) {
  uint8_t bank_number = LED_BANK( led );
  uint8_t segment_number = LED_SEGMENT( led );

  led_status[ bank_number ][ segment_number ].bright = intensity;

}

void led_set_blink ( uint8_t led, uint8_t blink ) {
  uint8_t bank_number = LED_BANK( led );
  uint8_t segment_number = LED_SEGMENT( led );

  led_status[ bank_number ][ segment_number ].blink = blink;

}


void led_clear_all( void ) {
  uint8_t bank, segment;
  /* clear (disable) all active leds */

  SEGMENTS_CLEAR();
  BANKS_CLEAR();

  for( bank = 0; bank < BANK_COUNT; bank++ ) {
    for( segment = 0; segment < SEGMENT_COUNT; segment++ ) {
        led_status[ bank ][ segment ].led_state = 0;
    }
  }
}

// vim: shiftwidth=2
