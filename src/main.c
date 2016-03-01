/**
 * \file main.c
 *
 * \brief main application entry point for armio
 *
 */

#include "asf/asf.h"
#include "display.h"
#include "anim.h"
#include "main.h"
#include "leds.h"
#include "accel.h"
#include "aclock.h"
#include "control.h"
#include "utils.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define VBATT_ADC_PIN               ADC_POSITIVE_INPUT_SCALEDIOVCC
#define LIGHT_ADC_PIN               ADC_POSITIVE_INPUT_PIN1

#define DIM_BRIGHT_VAL      (MIN_BRIGHT_VAL + 1)
#define DIM_LIGHT_THRESHOLD  17 //light level on 0-59 scale to match util display

#define MAIN_TIMER  TC5

/* deep sleep (i.e. shipping mode) wakeup parameters */
#define DEEP_SLEEP_EV_DELTA_S   3 /* max time between double clicks */
#define DEEP_SLEEP_SEQ_UP_COUNT    3 /* # of double clicks facing up to wakeup */
#define DEEP_SLEEP_SEQ_DOWN_COUNT    2 /* # of double clicks facing down to wakeup */

#define NVM_LOG_ADDR_START  (NVM_ADDR_START + NVM_DATA_STORE_SIZE)
#define NVM_LOG_ADDR_MAX    NVM_MAX_ADDR

#define IS_ACTIVITY_EVENT(ev_flags) \
      (ev_flags != EV_FLAG_NONE && \
        ev_flags != EV_FLAG_ACCEL_DOWN)

#define IS_LOW_BATT(vbatt_adc_val) \
  ((vbatt_adc_val >> 4) < 2600) //~2.5v

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________
typedef enum main_state_t {
  STARTUP = 0,
  RUNNING,
  ENTERING_SLEEP,
  WAKEUP,
} main_state_t;

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

#ifdef CLOCK_OUTPUT
static void setup_clock_pin_outputs( void );
  /* @brief multiplex clocks onto output pins
   * @param None
   * @retrn None
   */
#endif

static void watchdog_early_warning_callback(void);
  /* @brief gives us a chance to write time to flash before wdt timeout
   * @param None
   * @retrn None
   */

static void configure_wdt( void );
  /* @brief configure watchdog timer
   * @param None
   * @retrn None
   */

static void prepare_sleep( void );
  /* @brief enter into standby sleep
   * @param None
   * @retrn None
   */

static void wakeup( void );
  /* @brief wakeup from sleep (e.g. enable sleeping modules)
   * @param None
   * @retrn None
   */

static void main_tic ( void );
  /* @brief main control loop update function
   * @param None
   * @retrn None
   */

static void main_init( void );
  /* @brief initialize main control module
   * @param None
   * @retrn None
   */

//___ V A R I A B L E S ______________________________________________________
static struct tc_module main_tc;

static struct {
  /* Ticks since last wake */
  uint32_t waketicks;

  /* Inactivity counter for sleeping.  Resets on any
   * user activity (e.g. click)
   */
  uint32_t inactivity_ticks;

  /* Count of multiple quick presses in a row */
  uint8_t tap_count;

  /* sensors */
  sensor_type_t current_sensor;
  uint16_t light_sensor_adc_val;
  uint16_t vbatt_sensor_adc_val;

  uint8_t light_sensor_scaled_at_wakeup;
  main_state_t state;

  uint8_t brightness;

  /* wakestamp (i.e seconds since noon/midnight) of last wake event */
  int32_t last_wakestamp;

  /* deep sleep (aka 'shipping mode') */
  bool deep_sleep_mode;
  /* count for deep sleep (i.e shipping mode) wakeup recognition */
  uint8_t deep_sleep_down_ctr;
  uint8_t deep_sleep_up_ctr;

} main_gs;

static animation_t *sleep_wake_anim = NULL;

static struct adc_module light_vbatt_sens_adc;

static uint32_t nvm_row_addr = NVM_LOG_ADDR_START;
static uint8_t nvm_row_buffer[NVMCTRL_ROW_SIZE];
static uint16_t nvm_row_ind;
static struct wdt_conf config_wdt;

nvm_data_t main_nvm_data;
user_data_t main_user_data;

#ifdef LOG_FIFO
bool _accel_confirmed = false; 
#endif

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

static void watchdog_early_warning_callback(void) {
      main_nvm_data.year   = aclock_state.year;   
      main_nvm_data.month  = aclock_state.month;  
      main_nvm_data.day    = aclock_state.day;    
                                               
      main_nvm_data.hour   = aclock_state.hour;   
      main_nvm_data.minute = aclock_state.minute;
      main_nvm_data.second = aclock_state.second;
      main_nvm_data.pm     = aclock_state.pm;     
      
      nvm_update_buffer(NVM_DATA_ADDR, (uint8_t *) &main_nvm_data, 0,
             sizeof(nvm_data_t));
}

static void configure_wdt( void ) {
	/* Create a new configuration structure for the Watchdog settings and fill
	 * with the default module settings. */
	wdt_get_config_defaults(&config_wdt);

	/* Set the Watchdog configuration settings */
	config_wdt.always_on            = false;
	config_wdt.clock_source         = GCLK_GENERATOR_4;
	config_wdt.early_warning_period = WDT_PERIOD_2048CLK; //~2s
	config_wdt.timeout_period = WDT_PERIOD_4096CLK; //~4s

	/* Initialize and enable the Watchdog with the user settings */
	wdt_set_config(&config_wdt);
	
        wdt_register_callback(watchdog_early_warning_callback,
		WDT_CALLBACK_EARLY_WARNING);
}

static void wdt_enable( void ) {
      
        config_wdt.enable = true;
	wdt_set_config(&config_wdt);
	wdt_enable_callback(WDT_CALLBACK_EARLY_WARNING);
}

static void wdt_disable( void ) {
      
        config_wdt.enable = false;
	wdt_set_config(&config_wdt);
	wdt_disable_callback(WDT_CALLBACK_EARLY_WARNING);
}



static void configure_input(void) {
    /* Configure our button as an input */
  struct port_config pin_conf;
  port_get_config_defaults(&pin_conf);
  pin_conf.direction = PORT_PIN_DIR_INPUT;
#ifdef ENABLE_BUTTON
  port_pin_set_config(BUTTON_PIN, &pin_conf);

  /* Enable interrupts for the button */
  configure_extint();
#endif

  port_get_config_defaults(&pin_conf);
  pin_conf.direction = PORT_PIN_DIR_OUTPUT;
  port_pin_set_config(LIGHT_SENSE_ENABLE_PIN, &pin_conf);
  port_pin_set_output_level(LIGHT_SENSE_ENABLE_PIN, false);
}

static void config_main_tc( void ) {
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

#if LOG_USAGE
static void log_usage ( void ) {
  /* Log current vbatt with timestamp */
  int32_t reltime = aclock_get_timestamp_relative();
  /* We'll assume the relative timestamp is less than 2^24 seconds
   * (about 194 days) to be able to pack data into 4 bytes */
  
  uint32_t data = (reltime << 8) & 0xffffff00; 
  
  /* log vbatt.  vbatt is a 16-bit averaged value
   * of 12-bit reads.  First decimate downt to a 12-bit val */
  uint16_t vbatt_val = main_gs.vbatt_sensor_adc_val >> 4;
  
  /* Now, record as an 8-bit value representing the offset 
   * from 2048 (~2v) */
  if (vbatt_val <= 2048) {
    vbatt_val = 0;
  } else {
    vbatt_val-=2048;
   
    /* Now decimate down to 8-bit value */
    if (vbatt_val >= 1024) {
      vbatt_val = 255;
    } else {
      vbatt_val>>=2;
    }
  }

  data+=(vbatt_val & 0x000000ff);
  main_log_data((uint8_t *)&data, sizeof(data), true);
}
#endif


#ifdef CLOCK_OUTPUT
static void setup_clock_pin_outputs( void ) {
  /* For debugging purposes, multiplex our
   * clocks onto output pins.. GCLK gens 4 and 7
   * should be enabled for this to function
   * properly
   * */

  struct system_pinmux_config pin_mux;
  system_pinmux_get_config_defaults(&pin_mux);

  /* MUX out the system clock to a I/O pin of the device */
  pin_mux.mux_position = MUX_PA10H_GCLK_IO4;
  system_pinmux_pin_set_config(PIN_PA10H_GCLK_IO4, &pin_mux);

  pin_mux.mux_position = MUX_PA23H_GCLK_IO7;
  system_pinmux_pin_set_config(PIN_PA23H_GCLK_IO7, &pin_mux);
}
#endif

static void prepare_sleep( void ) {
  wdt_disable();

  if (light_vbatt_sens_adc.hw) {
    adc_disable(&light_vbatt_sens_adc);
  }

  port_pin_set_output_level(LIGHT_SENSE_ENABLE_PIN, false);

  led_controller_disable();
  aclock_disable();
  tc_disable(&main_tc);

  /* The vbatt adc may have enabled the voltage reference, so disable
   * it in standby to save power */
  system_voltage_reference_disable(SYSTEM_VOLTAGE_REFERENCE_BANDGAP);
  
}

static void wakeup (void) {
  
  
  wdt_enable();
  
  led_controller_enable();
  aclock_enable();
  accel_enable();

  /* Errata 12227: perform a software reset of tc after waking up */
  tc_reset(&main_tc);
  config_main_tc();

  tc_enable(&main_tc);
  system_interrupt_enable_global();

  if (light_vbatt_sens_adc.hw) {
    adc_enable(&light_vbatt_sens_adc);
  }

  /* Update vbatt estimate on wakeup only */
#if ENABLE_VBATT
  main_set_current_sensor(sensor_vbatt);
  main_start_sensor_read();
  main_read_current_sensor(true);
#endif

#if ENABLE_LIGHT_SENSE
  port_pin_set_output_level(LIGHT_SENSE_ENABLE_PIN, true);
  main_set_current_sensor(sensor_light);
  main_start_sensor_read();

  main_gs.light_sensor_scaled_at_wakeup = adc_light_value_scale(
      main_read_current_sensor(true) );

  if (main_gs.light_sensor_scaled_at_wakeup < DIM_LIGHT_THRESHOLD ) {
    main_gs.brightness = DIM_BRIGHT_VAL;
  } else {
    main_gs.brightness = MAX_BRIGHT_VAL;
  }

  led_set_max_brightness( main_gs.brightness );
  port_pin_set_output_level(LIGHT_SENSE_ENABLE_PIN, false);
#endif

#if LOG_USAGE
#ifndef USAGE_LOG_PERIOD 
#define USAGE_LOG_PERIOD 20
#endif

  if (main_nvm_data.lifetime_wakes % USAGE_LOG_PERIOD == 0) {
    log_usage();
  }
#endif

}

/* 'wakestamps' are used to keep track of wakeup check times.
 * They are used to determine if a double-click sequence has
 * occurred that should wake us up from deep-sleep (i.e. shipping mode)
 */
static int32_t get_wakestamp( void ) {
  /* a 'wakestamp' is simply the # of seconds
   * elapsed since midnight/noon */
    uint8_t hour, min, sec;

    aclock_get_time(&hour, &min, &sec);

    return 3600*(int32_t)hour + 60*(int32_t)min + sec;
}

static int32_t wakestamp_elapsed( int32_t stamp_recent, int32_t stamp_earlier ) {
  /* Returns the # of seconds elapsed betwen stamp_recent
   * and an earlier wakestamp.  Accounts for rollover of
   * wakestamps at midnight/noon boundaries */

  if (stamp_recent < stamp_earlier) {
    stamp_recent += 3600*12; /* assumes max of 1 rollover */
  }

  return stamp_recent - stamp_earlier;
}

static bool wakeup_check( void ) {
  /* If we've been awaken by an interrupt, determine
   * if we should fully wakeup
   */
  int32_t curr_wakestamp;
  bool wake = false;

#ifdef USE_WAKEUP_ALARM
    accel_wakeup_check();
    return true;
#endif

  if (!main_gs.deep_sleep_mode) {
    if (accel_wakeup_check()) {
      return true;
    } else {
      return false;
    }
  }

  
  /* We are in deep sleep/shipping mode, update dclick counter */
  aclock_enable();
  curr_wakestamp = get_wakestamp();
  if (wakestamp_elapsed(curr_wakestamp, main_gs.last_wakestamp) < DEEP_SLEEP_EV_DELTA_S) {
    int16_t x,y,z;
    accel_enable();
    accel_data_read(&x, &y, &z);

    if (z < -5) {
      main_gs.deep_sleep_down_ctr++;
#ifdef DEEP_SLEEP_DEBUG
      _led_on_full( 30 + main_gs.deep_sleep_down_ctr ); 
      delay_ms(100);
      _led_off_full( 30 + main_gs.deep_sleep_down_ctr ); 
      delay_ms(50);
#endif
    } else if (z > 5 && main_gs.deep_sleep_down_ctr >= DEEP_SLEEP_SEQ_DOWN_COUNT) {
      main_gs.deep_sleep_up_ctr++;
#ifdef DEEP_SLEEP_DEBUG
      _led_on_full( main_gs.deep_sleep_up_ctr ); 
      delay_ms(100); 
      _led_off_full( main_gs.deep_sleep_up_ctr ); 
      delay_ms(50); 
#endif
    }


    if (main_gs.deep_sleep_down_ctr >= DEEP_SLEEP_SEQ_DOWN_COUNT &&
        main_gs.deep_sleep_up_ctr >= DEEP_SLEEP_SEQ_UP_COUNT) {
      accel_wakeup_gesture_enabled = true;
      wake = true;
    } else {
      accel_wakeup_gesture_enabled = false;
      wake = false;
      accel_sleep();
    }

  } else {
    /* we will assume the first dclick is face down so we
     * don't bother wasting power reading accelerometer */
    main_gs.deep_sleep_down_ctr = 1; 
    main_gs.deep_sleep_up_ctr = 0;
    accel_wakeup_gesture_enabled = false;
    wake = false;
    
#ifdef DEEP_SLEEP_DEBUG
    _led_on_full( 30 );
    delay_ms(100); 
    _led_off_full( 30 );
    delay_ms(50); 
#endif
  }

  main_gs.last_wakestamp = curr_wakestamp;
  
  aclock_disable();

  return wake;
}

//___ F U N C T I O N S ______________________________________________________
static void main_tic( void ) {
  event_flags_t event_flags = EV_FLAG_NONE;

  main_gs.inactivity_ticks++;
  main_gs.waketicks++;

  event_flags |= accel_event_flags();

  /* Reset inactivity if any button/click event occurs */
  if (IS_ACTIVITY_EVENT(event_flags)) {
     main_gs.inactivity_ticks = 0;
    /* Ensure the display is not dim */
    main_gs.brightness = MAX_BRIGHT_VAL;
    led_set_max_brightness( main_gs.brightness );
  }
  
  /* ### DEBUG led controller */
  if (MNCLICK(event_flags, 12, 20)) {
      _led_on_full( 15 ); 
      delay_ms(100);
      _led_off_full( 15 );
  }

//  if (MNCLICK(event_flags, 20, 30)) {
//    while(1);
//  }

  switch (main_gs.state) {
    case STARTUP:
      /* Stay in startup until animation is finished */
      if (anim_is_finished(sleep_wake_anim)) {
        anim_release(sleep_wake_anim);
        sleep_wake_anim = NULL;
        main_gs.state = RUNNING;
      }
      return;
    case ENTERING_SLEEP:
      /* Wait until animation is finished to sleep */
      if (anim_is_finished(sleep_wake_anim)) {
        anim_release(sleep_wake_anim);
        sleep_wake_anim = NULL;
        
        /* Update and store lifetime usage data */
#ifdef LOG_LIFETIME_USAGE
#ifndef LIFETIME_USAGE_PERIOD
#define LIFETIME_USAGE_PERIOD 30
#endif
        main_nvm_data.lifetime_wakes++;
        main_nvm_data.lifetime_ticks+=main_gs.waketicks;
        if (main_nvm_data.lifetime_wakes % LIFETIME_USAGE_PERIOD == 1) {
          /* Only update buffer once every 100 wakes to extend
           * lifetime of the NVM -- may fail after 100k writes */
          nvm_update_buffer(NVM_DATA_ADDR, (uint8_t *) &main_nvm_data, 0,
              sizeof(nvm_data_t));
        }
#endif

#ifdef LOG_FIFO
        if (ax_fifo_depth == 32  
            && (_accel_confirmed || main_nvm_data.lifetime_wakes < 200)
#ifdef LOG_CONFIRMED_ONLY 
            && _accel_confirmed
#endif
           ) {
          uint32_t code = 0xAAAAAAAA; /* FIFO data begin code */
          main_log_data((uint8_t *) &code, sizeof(uint32_t), false);
          main_log_data((uint8_t *)ax_fifo, 6*ax_fifo_depth, true);
          code = _accel_confirmed ? 0xCCCCCCCC : 0xEEEEEEEE; /* FIFO data end code */
          main_log_data((uint8_t *) &code, sizeof(uint32_t), true);
          uint16_t log_code = 0xEEDD; 
          main_log_data ((uint8_t *)&log_code, sizeof(uint16_t), true);
          main_log_data ((uint8_t *)&main_gs.waketicks, sizeof(uint32_t), true);
        }
#endif
  
        prepare_sleep();
        accel_sleep();

        /* we will stay in standby mode now until an interrupt wakes us
         * from sleep (and we continue from this point) */
        do {
          system_sleep();
        } while(!wakeup_check());
    
        wakeup();
        
        if (main_gs.deep_sleep_mode) {
          /* We have now woken up from deep sleep mode so
           * display a sparkle animation
           */
          main_gs.deep_sleep_mode = false;
          sleep_wake_anim = anim_random(display_point(0, BRIGHT_DEFAULT), 
              MS_IN_TICKS(15), MS_IN_TICKS(4000), true);
        }

        led_clear_all();

//        if (IS_LOW_BATT(main_gs.vbatt_sensor_adc_val)) {
//          /* If low battery, display warning animation for a short period */
//          sleep_wake_anim = anim_blink(display_polygon(30, BRIGHT_MED_LOW, 3),
//               MS_IN_TICKS(300), MS_IN_TICKS(2100), true);
//        }

        main_gs.state = WAKEUP;
      }
      return;
    case WAKEUP:
      if (anim_is_finished(sleep_wake_anim)) {
        /* animation is autorelease */
        sleep_wake_anim = NULL;
        main_gs.state = RUNNING;

        main_gs.waketicks = 0;
        main_gs.inactivity_ticks = 0;
      }
      return;
    case RUNNING:
      /* Check for inactivity timeout */
      if (
#ifdef ALWAYS_ACTIVE
          false
#else
          main_gs.inactivity_ticks > \
          ctrl_mode_active->sleep_timeout_ticks ||
          (IS_CONTROL_MODE_SHOW_TIME() && (
           (event_flags & EV_FLAG_ACCEL_DOWN) || TCLICK(event_flags)))

#endif
          ) {

          /* A sleep event has occurred */
          if (IS_CONTROL_MODE_SHOW_TIME() && TCLICK(event_flags)) {
#ifdef LOG_FIFO
              _accel_confirmed = true;
#else
              accel_wakeup_gesture_enabled = false;
#endif

          } else {
#ifdef LOG_FIFO
              _accel_confirmed = false;
#else
              accel_wakeup_gesture_enabled = main_user_data.wake_gestures;
#endif
          }

          /* Notify control mode of sleep event and reset control mode */
          ctrl_mode_active->tic_cb(EV_FLAG_SLEEP);

          main_gs.state = ENTERING_SLEEP;

#ifdef NO_TIME_ANIMATION
          sleep_wake_anim = NULL;
#else
          if (event_flags & EV_FLAG_ACCEL_DOWN) {
            sleep_wake_anim = NULL;
          } else {
            sleep_wake_anim = anim_swirl(0, 5, MS_IN_TICKS(5), 57, true);
          }
#endif
          return;
      }

      /* Call mode's main tic loop/event handler */
      control_tic(event_flags); 
      
      return; /* END OF RUNNING STATE SWITCH CASE */
  }
}

static void main_init( void ) {
  /* Initalize main state */
  main_gs.waketicks = 0;
  main_gs.tap_count = 0;
  main_gs.inactivity_ticks = 0;
  main_gs.light_sensor_scaled_at_wakeup = 0;
  main_gs.brightness = MAX_BRIGHT_VAL;
  main_gs.state = STARTUP;
  main_gs.deep_sleep_mode = false;
  main_user_data.wake_gestures = true;

#ifdef SHOW_SEC_ALWAYS
  main_user_data.seconds_always_on = true;
#else
  main_user_data.seconds_always_on = false;
#endif

  /* Configure main timer counter */
  config_main_tc();

  /* Initialize NVM controller for data storage */
  struct nvm_config config_nvm;
  nvm_get_config_defaults(&config_nvm);
  nvm_set_config(&config_nvm);

  uint32_t data = 0xffffffff;

#ifdef LOG_USAGE
  /* Set row address to first open space by
   * iterating through log data until an
   * empty (4 bytes of 1s) row is found */
  while (nvm_row_addr < NVM_LOG_ADDR_MAX) {

    nvm_read_buffer(nvm_row_addr, (uint8_t *) &data, sizeof(data));
    if (data == 0xffffffff)
      break;

    nvm_row_addr+=NVMCTRL_ROW_SIZE;
  }
#endif

  /* Read configuration data stored in nvm */
  nvm_read_buffer(NVM_DATA_ADDR, (uint8_t *) &main_nvm_data,
      sizeof(nvm_data_t));

  if (main_nvm_data.lifetime_wakes == 0xffffffff) {
      main_nvm_data.lifetime_wakes = 0; 
  }
  
  if (main_nvm_data.filtered_gestures == 0xffffffff) {
      main_nvm_data.filtered_gestures = 0; 
  }
 
  if (main_nvm_data.filtered_dclicks == 0xffffffff) {
      main_nvm_data.filtered_dclicks = 0; 
  }
  
  if (main_nvm_data.lifetime_ticks == 0xffffffff) {
      main_nvm_data.lifetime_ticks = 0; 
  }

  
  if (main_nvm_data.lifetime_resets == 0xffffffff) {
      main_nvm_data.lifetime_resets = 0; 
  } else {
      main_nvm_data.lifetime_resets++;
  }
  
  if (main_nvm_data.wdt_resets == 0xffff) {
      main_nvm_data.wdt_resets = 0;
  }
  
  enum system_reset_cause reset_cause = system_get_reset_cause();
  if (reset_cause == SYSTEM_RESET_CAUSE_WDT) {
      main_nvm_data.wdt_resets++;   
  }

  nvm_update_buffer(NVM_DATA_ADDR, (uint8_t *) &main_nvm_data, 0,
          sizeof(nvm_data_t));
}

uint32_t main_get_waketime_ms( void ) {
  return TICKS_IN_MS(main_gs.waketicks);
}

void main_inactivity_timeout_reset( void ) {
  main_gs.inactivity_ticks = 0;
}

void main_deep_sleep_enable( void ) {
  main_gs.deep_sleep_mode = true;
  main_gs.deep_sleep_down_ctr = 0;
  main_gs.deep_sleep_up_ctr = 0;
  accel_wakeup_gesture_enabled = false;
  main_gs.state = ENTERING_SLEEP;
}

void main_terminate_in_error ( error_group_code_t error_group, uint32_t subcode ) {
  /* Display error code leds */

  led_clear_all();
  uint8_t i;

  /* display the subcode (lowest 3 bytes aligned with 6, 8, 10 hr marks) */
  for( i = 0; i < 8; i++ ) {
    if( subcode & (((uint32_t) 1)<<i) ) {
      led_on( i + 30, BRIGHT_DEFAULT );
    }
  }
  for( i = 8; i < 16; i++ ) {
    if( subcode & (((uint32_t) 1)<<i) ) {
      led_on( i + 32, BRIGHT_DEFAULT );
    }
  }
  for( i = 16; i < 24; i++ ) {
    if( subcode & (((uint32_t) 1)<<i) ) {
      led_on( i + 34, BRIGHT_DEFAULT );
    }
  }
  for( i = 24; i < 32; i++ ) {
    if( subcode & (((uint32_t) 1)<<i) ) {
      led_on( (i + 36) % 60, BRIGHT_DEFAULT );
    }
  }

  /* blink error code indefinitely */
  while( 1 ) {
    //_led_on_full( ((uint8_t) error_group));
    led_on( (uint8_t) error_group, BRIGHT_DEFAULT);
    delay_ms(100);
    led_off( (uint8_t) error_group);
    delay_ms(100);
  }
}

void main_log_data( uint8_t *data, uint16_t length, bool flush) {
  /* Store data in NVM flash */
  enum status_code error_code;
  uint16_t  i, p;

  if (nvm_row_addr >= NVM_LOG_ADDR_MAX)  return; //out of buffer space 

  for (i = 0; i < length; i++) {
    nvm_row_buffer[nvm_row_ind] = data[i];
    nvm_row_ind++;

    if (nvm_row_ind == NVMCTRL_ROW_SIZE || flush) {
      /* Write the full row buffer to flash */
      /* First, erase the row */
      do {
        error_code = nvm_erase_row(nvm_row_addr);
      } while (error_code == STATUS_BUSY);

      /* Write each page of the row buffer */
      for (p = 0; p < NVMCTRL_ROW_PAGES; p++) {
        do {
          error_code = nvm_write_buffer(nvm_row_addr + p*NVMCTRL_PAGE_SIZE,
              nvm_row_buffer + p*NVMCTRL_PAGE_SIZE,
              NVMCTRL_PAGE_SIZE);

        } while (error_code == STATUS_BUSY);
      }
    }

    if (nvm_row_ind == NVMCTRL_ROW_SIZE) {
      nvm_row_ind = 0;
      nvm_row_addr+=NVMCTRL_ROW_SIZE;

      if (nvm_row_addr >= NVM_LOG_ADDR_MAX)  {
        return; //out of buffer space
        //nvm_row_addr = NVM_LOG_ADDR_START; //rollover
      }
    }

  }
}


void main_start_sensor_read ( void ) {
  if (!(adc_get_status(&light_vbatt_sens_adc) & ADC_STATUS_RESULT_READY))
    adc_start_conversion(&light_vbatt_sens_adc);
}

sensor_type_t main_get_current_sensor ( void ) {
  return main_gs.current_sensor;
}

void main_set_current_sensor ( sensor_type_t sensor ) {
  struct adc_config config_adc;

  main_gs.current_sensor = sensor;

  if (light_vbatt_sens_adc.hw)
    adc_reset(&light_vbatt_sens_adc);

  adc_get_config_defaults(&config_adc);

  /* Average samples to produce each value */
  config_adc.resolution         = ADC_RESOLUTION_CUSTOM;
  config_adc.accumulate_samples = ADC_ACCUMULATE_SAMPLES_1024;
  config_adc.divide_result      = ADC_DIVIDE_RESULT_16;
  config_adc.run_in_standby     = false;
  config_adc.resolution         = ADC_RESOLUTION_16BIT;

  switch(sensor) {
    case sensor_vbatt:
      config_adc.reference = ADC_REFERENCE_INT1V;
      config_adc.positive_input = VBATT_ADC_PIN;
      break;
    case sensor_light:
      config_adc.reference = ADC_REFERENCE_INTVCC0;
      config_adc.positive_input = LIGHT_ADC_PIN;
      break;
  }

  adc_init(&light_vbatt_sens_adc, ADC, &config_adc);
  adc_enable(&light_vbatt_sens_adc);
}


uint16_t main_read_current_sensor( bool blocking ) {
  uint16_t result;
  uint16_t *curr_sens_adc_val_ptr;
  uint8_t status;

  /* Read the current sensor adc (vbatt or light) */
  /* NOTE: if reading light sensor, LIGHT_ENABLE_PIN
   * should have already been set high for a valid read.
   * That pin should not be left high since it draws a
   * lot of current */

  if (main_gs.current_sensor == sensor_vbatt)
    curr_sens_adc_val_ptr = &main_gs.vbatt_sensor_adc_val;
  else
    curr_sens_adc_val_ptr = &main_gs.light_sensor_adc_val;

  do {
    status = adc_read(&light_vbatt_sens_adc, &result);

    if (status == STATUS_OK) {
      *curr_sens_adc_val_ptr = result;
      adc_clear_status(&light_vbatt_sens_adc, ADC_STATUS_RESULT_READY);
    }
  } while (blocking && status != STATUS_OK);

  return *curr_sens_adc_val_ptr;
}

uint16_t main_get_light_sensor_value ( void ) {
  return main_gs.light_sensor_adc_val;
}

uint16_t main_get_vbatt_value ( void ) {
  return main_gs.vbatt_sensor_adc_val;
}

bool main_is_low_vbatt ( void ) {
  return IS_LOW_BATT(main_gs.vbatt_sensor_adc_val);
}

uint8_t main_get_multipress_count( void ) {
  return main_gs.tap_count;
}

#if !(RTC_CALIBRATE)
int main (void) {

  system_init();
  system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);
  
  delay_init();
  main_init();
  led_controller_init();
  led_controller_enable();
  aclock_init();
  control_init();
  display_init();
  anim_init();
  accel_init();

  /* Errata 39.3.2 -- device may not wake up from
   * standby if nvm goes to sleep. Not needed
   * for revision D or later */
#ifdef __SAMD21E16A__
  NVMCTRL->CTRLB.bit.SLEEPPRM = NVMCTRL_CTRLB_SLEEPPRM_DISABLED_Val;
#endif

  /* Read vbatt sensor on startup */
#if ENABLE_VBATT
  main_set_current_sensor(sensor_vbatt);
  main_start_sensor_read();
  main_read_current_sensor(true);
#endif

  configure_wdt();

  /* Show a startup LED swirl */
#ifdef SPARKLE_FOREVER_MODE
  sleep_wake_anim = anim_random(display_point(0, BRIGHT_DEFAULT), MS_IN_TICKS(15), ANIMATION_DURATION_INF, false);
#endif

  if (!sleep_wake_anim) {
    if (main_nvm_data.lifetime_resets == 0) {
      /* This watch has just been born.  Display a slow swirl so we
       * can ensure all the LEDs are working */
        //sleep_wake_anim = anim_random(display_point(0, BRIGHT_DEFAULT), 
        //    MS_IN_TICKS(15), MS_IN_TICKS(2000), true);
        sleep_wake_anim = anim_swirl(0, 5, MS_IN_TICKS(50), 120, true);
#ifdef NOT_NOW //DEEP_SLEEP_AT_BIRTH
        main_deep_sleep_enable();
#endif
    } else {
      /* A reset has occurred later in life */
        sleep_wake_anim = anim_swirl(0, 5, MS_IN_TICKS(10), 30, false);
    }
  }

  configure_input();
  system_interrupt_enable_global();
  
  wdt_enable();

  while (1) {
    if (tc_get_status(&main_tc) & TC_STATUS_COUNT_OVERFLOW) {
      tc_clear_status(&main_tc, TC_STATUS_COUNT_OVERFLOW);
      main_tic();
      anim_tic();
      display_tic();
     
      if (main_gs.waketicks % 500 == 0) {
        wdt_reset_count();
      }

    }
  }

}

#else /* RTC_CALIBRATE enabled */
int main (void) {
  system_init();
  system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);
  
  delay_init();
  main_init();
  led_controller_conf_output();
  led_clear_all();
  
  _led_on_full(0);
  delay_ms(1000);
  _led_off_full(0);
  
  rtc_cal_run();
}
#endif /* RTC_CALIBRATE */
// vim:shiftwidth=2
