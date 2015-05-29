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
#define BUTTON_PIN          PIN_PA31
#define BUTTON_PIN_EIC      PIN_PA31A_EIC_EXTINT11
#define BUTTON_PIN_EIC_MUX  MUX_PA31A_EIC_EXTINT11
#define BUTTON_EIC_CHAN     11

#define BUTTON_UP           true
#define BUTTON_DOWN         false

#define VBATT_ADC_PIN               ADC_POSITIVE_INPUT_SCALEDIOVCC
#define LIGHT_ADC_PIN               ADC_POSITIVE_INPUT_PIN1

#define DIM_BRIGHT_VAL      (MIN_BRIGHT_VAL + 1)
#define DIM_LIGHT_THRESHOLD  17 //light level on 0-59 scale to match util display

#define MAIN_TIMER  TC5

#define DEFAULT_SLEEP_TIMEOUT_TICKS     MS_IN_TICKS(7000)

/* tick count before considering a button press "long" */
#define LONG_PRESS_TICKS    MS_IN_TICKS(1500)

/* max tick count between successive quick taps */
#define QUICK_TAP_INTERVAL_TICKS    500

/* covering the watch (when in watch mode) will turn off the display
 * if the scaled reading on the light sensor drops by this much */
#define LIGHT_SENSOR_REDUCTION_SHUTOFF  15

/* Starting flash address at which to store data */
#define NVM_ADDR_START      ((1 << 15) + (1 << 14)) /* assumes program size < 48KB */
#define NVM_CONF_ADDR       NVM_ADDR_START
#define NVM_CONF_STORE_SIZE (64)
#define NVM_LOG_ADDR_START  (NVM_ADDR_START + NVM_CONF_STORE_SIZE)
#define NVM_LOG_ADDR_MAX     NVM_MAX_ADDR

#define IS_ACTIVITY_EVENT(ev_flags) \
      (ev_flags != EV_FLAG_NONE && \
        ev_flags != EV_FLAG_ACCEL_Z_LOW)

#if LOG_USAGE
  #define LOG_WAKEUP() \
    log_vbatt(true)
  #define LOG_SLEEP() \
    log_vbatt(false)
#else
  #define LOG_WAKEUP()
  #define LOG_SLEEP()
#endif

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________
typedef enum main_state_t {
  STARTUP = 0,
  RUNNING,
  ENTERING_SLEEP,
  MODE_TRANSITION
} main_state_t;

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________
static void configure_input( void );
  /* @brief configure input buttons
   * @param None
   * @retrn None
   */

#ifdef CLOCK_OUTPUT
static void setup_clock_pin_outputs( void );
  /* @brief multiplex clocks onto output pins
   * @param None
   * @retrn None
   */
#endif

static void update_vbatt_running_avg( void );
  /* @brief update vbatt running avg based on latest read
   * @param None
   * @retrn None
   */

static void configure_extint(void);
  /* @brief enable external interrupts
   * @param None
   * @retrn None
   */
static event_flags_t button_event_flags( void );
  /* @brief check current button event flags
   * @param None
   * @retrn button event flags
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

  /* Ticks since entering current mode */
  uint32_t modeticks;

  /* Ticks since last wake */
  uint32_t waketicks;

  /* Inactivity counter for sleeping.  Resets on any
   * user activity (e.g. button press)
   */
  uint32_t inactivity_ticks;

  /* Current button state */
  bool button_state;

  /* Counter for button ticks since pushed down */
  uint32_t button_hold_ticks;

  /* Count of multiple quick presses in a row */
  uint8_t tap_count;

  /* sensors */
  sensor_type_t current_sensor;
  uint16_t light_sensor_adc_val;
  uint16_t vbatt_sensor_adc_val;
  uint16_t running_avg_vbatt;

  uint8_t light_sensor_scaled_at_wakeup;
  main_state_t state;

  uint8_t brightness;

} main_gs;

static animation_t *sleep_wake_anim;
static animation_t *mode_trans_swirl;
static animation_t *mode_trans_blink;
static display_comp_t *mode_disp_point;

static struct adc_module light_vbatt_sens_adc;
nvm_conf_t main_nvm_conf_data;

static uint32_t nvm_row_addr = NVM_LOG_ADDR_START;
static uint8_t nvm_row_buffer[NVMCTRL_ROW_SIZE];
static uint16_t nvm_row_ind;


//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

static void update_vbatt_running_avg( void ) {
  /* Use exponential moving average with alpha = 1/128
   * to update vbatt level */
  uint32_t temp;
  temp = main_gs.running_avg_vbatt*127;
  main_gs.running_avg_vbatt = (temp + main_gs.vbatt_sensor_adc_val)/128;
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
static void log_vbatt (bool wakeup) {
  /* Log current vbatt with timestamp */
  int32_t time = aclock_get_timestamp();
  uint32_t data = (wakeup ? 0xBEEF0000 : 0xDEAD0000);
  main_set_current_sensor(sensor_vbatt);
  main_start_sensor_read();
  data+=main_read_current_sensor(true);
  main_log_data((uint8_t *)&time, sizeof(time), true);
  main_log_data((uint8_t *)&data, sizeof(data), true);
}
#endif

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

static void configure_extint(void) {
  struct extint_chan_conf eint_chan_conf;
  extint_chan_get_config_defaults(&eint_chan_conf);

  eint_chan_conf.gpio_pin             = BUTTON_PIN_EIC;
  eint_chan_conf.gpio_pin_mux         = BUTTON_PIN_EIC_MUX;
  eint_chan_conf.gpio_pin_pull        = EXTINT_PULL_NONE;
  /* NOTE: cannot wake from standby with filter or edge detection ... */
  eint_chan_conf.detection_criteria   = EXTINT_DETECT_LOW;
  eint_chan_conf.filter_input_signal  = false;
  eint_chan_conf.wake_if_sleeping     = true;
  extint_chan_set_config(BUTTON_EIC_CHAN, &eint_chan_conf);
}

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
  LOG_SLEEP();

#ifdef ENABLE_BUTTON
  /* Enable button callback to awake us from sleep */
  extint_chan_enable_callback(BUTTON_EIC_CHAN,
      EXTINT_CALLBACK_TYPE_DETECT);
#endif

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
  if (light_vbatt_sens_adc.hw) {
    adc_enable(&light_vbatt_sens_adc);
  }

#ifdef ENABLE_BUTTON
  extint_chan_disable_callback(BUTTON_EIC_CHAN,
      EXTINT_CALLBACK_TYPE_DETECT);
#endif
  led_controller_enable();
  aclock_enable();
  accel_enable();

  /* Errata 12227: perform a software reset of tc after waking up */
  tc_reset(&main_tc);
  config_main_tc();


  tc_enable(&main_tc);
  system_interrupt_enable_global();

  /* Update vbatt estimate on wakeup only */
#if ENABLE_VBATT
  main_set_current_sensor(sensor_vbatt);
  main_start_sensor_read();
  main_read_current_sensor(true);
  update_vbatt_running_avg();
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

  LOG_WAKEUP();
}

static event_flags_t button_event_flags ( void ) {
  event_flags_t event_flags = EV_FLAG_NONE;
#ifdef ENABLE_BUTTON
  bool new_btn_state = port_pin_get_input_level(BUTTON_PIN);

  if (new_btn_state == BUTTON_UP &&
      main_gs.button_state == BUTTON_DOWN) {
    /* button has been released */
    main_gs.button_state = BUTTON_UP;
    if (main_gs.tap_count == 0) {
      /* End of a single button push */
      if (main_gs.button_hold_ticks  < LONG_PRESS_TICKS ) {
        event_flags |= EV_FLAG_SINGLE_BTN_PRESS_END;
      } else {
        event_flags |= EV_FLAG_LONG_BTN_PRESS_END;
      }
      main_gs.button_hold_ticks = 0;
    } else {
      /* TODO -- multi-tap support */
    }
  } else if (new_btn_state == BUTTON_DOWN &&
      main_gs.button_state == BUTTON_UP) {
    /* button has been pushed down */
    main_gs.button_state = BUTTON_DOWN;
  } else {
    /* button state has not changed */
    if (main_gs.button_state == BUTTON_DOWN) {
      main_gs.button_hold_ticks++;
      if (main_gs.button_hold_ticks > LONG_PRESS_TICKS) {
        event_flags |= EV_FLAG_LONG_BTN_PRESS;
      }
    }
  }
#endif
  return event_flags;
}


//___ F U N C T I O N S ______________________________________________________
static void main_tic( void ) {
  event_flags_t event_flags = EV_FLAG_NONE;

  main_gs.inactivity_ticks++;
  main_gs.waketicks++;

  event_flags |= button_event_flags();
  event_flags |= accel_event_flags();

  /* Reset inactivity if any button/click event occurs */
  if (IS_ACTIVITY_EVENT(event_flags)) {
     main_gs.inactivity_ticks = 0;
    /* Ensure the display is not dim */
    main_gs.brightness = MAX_BRIGHT_VAL;
    led_set_max_brightness( main_gs.brightness );
  }

  switch (main_gs.state) {
    case STARTUP:
      /* Stay in startup until animation is finished */
      if (anim_is_finished(sleep_wake_anim)) {
        main_gs.state = RUNNING;
        anim_release(sleep_wake_anim);
      }
      return;
    case ENTERING_SLEEP:
      /* Wait until animation is finished to sleep */
      if (anim_is_finished(sleep_wake_anim)) {
        anim_release(sleep_wake_anim);

        /* Reset control mode to main (time display) mode */
        control_state_set(ctrl_state_main);
        prepare_sleep();

        accel_sleep();

        /* we will stay in standby mode now until an interrupt wakes us
         * from sleep (and we continue from this point) */
        do {
          system_sleep();
        } while(!accel_wakeup_check());

        wakeup();

        //main_set_current_sensor(sensor_light);
        //main_start_sensor_read();
        //main_gs.light_sensor_scaled_at_wakeup = adc_light_value_scale(
        //    main_read_current_sensor(true) );

        if (ctrl_mode_active->wakeup_cb)
          ctrl_mode_active->wakeup_cb();

        display_comp_show_all();
        led_clear_all();

        main_gs.modeticks = 0;
        main_gs.waketicks = 0;
        main_gs.inactivity_ticks = 0;
        main_gs.state = RUNNING;
      }
      return;
    case RUNNING:
#if ENABLE_LIGHT_SENSE
#ifdef NOT_NOW
      if( IS_CONTROL_MODE_SHOW_TIME() ) {
        /* we are in main time display mode */
        main_set_current_sensor(sensor_light);
        main_start_sensor_read();
        if ( adc_light_value_scale( main_read_current_sensor(false) ) +
            LIGHT_SENSOR_REDUCTION_SHUTOFF < main_gs.light_sensor_scaled_at_wakeup ) {
          /* sleep the watch */
          main_gs.inactivity_ticks = ctrl_mode_active->sleep_timeout_ticks;
        }
      }
#endif
#endif
      /* Check for inactivity timeout */
      if (
#ifdef ALWAYS_ACTIVE
          true
#else
          main_gs.inactivity_ticks > \
          ctrl_mode_active->sleep_timeout_ticks ||
          (IS_CONTROL_MODE_SHOW_TIME() && event_flags & EV_FLAG_ACCEL_Z_LOW &&
           main_get_waketime_ms() > 300)
#endif

          ) {
        /* A sleep event has occurred */

        main_gs.state = ENTERING_SLEEP;

        /* Notify control mode of sleep event and reset control mode */
        ctrl_mode_active->tic_cb(EV_FLAG_SLEEP, main_gs.modeticks);
        if (ctrl_mode_active->about_to_sleep_cb)
          ctrl_mode_active->about_to_sleep_cb();

        display_comp_hide_all();

        sleep_wake_anim = anim_swirl(0, 5, MS_IN_TICKS(5), 57, false);
        return;
      }

      /* Call mode's main tic loop/event handler */
      if (ctrl_mode_active->tic_cb(event_flags, main_gs.modeticks++)){

        /* Control mode has returned true indicating it is finished,
         * so transition to next control mode */
        main_gs.modeticks = 0;
        main_gs.button_hold_ticks = 0;
        main_gs.inactivity_ticks = 0;

        if (IS_CONTROL_STATE_EE()) {
          control_state_set(ctrl_state_main);

        }
        else if ( IS_CONTROL_MODE_SHOW_TIME() &&
            ( event_flags & EV_FLAG_ACCEL_7CLICK_X ||
              event_flags & EV_FLAG_ACCEL_8CLICK_X ||
              event_flags & EV_FLAG_ACCEL_9CLICK_X )) {
            /* Enter util state */
          control_state_set(ctrl_state_util);

        }
        else if ( IS_CONTROL_MODE_SHOW_TIME() &&
              event_flags & EV_FLAG_ACCEL_NCLICK_X ) {
          /* Enter easter egg mode */
          control_state_set(ctrl_state_ee);
        } else {

          /* begin transition to next control mode */
          main_gs.state = MODE_TRANSITION;

          /* animate slow snake from current mode to next. */
          uint8_t mode_gap = 60/control_mode_count();
          uint8_t tail_len = 4;
          mode_trans_swirl = anim_swirl(
              mode_gap*control_mode_index(ctrl_mode_active),
              tail_len, MS_IN_TICKS(5 + 40/control_mode_count()),
              mode_gap - tail_len, true);
        }
      }

      break;
    case MODE_TRANSITION:
      if (mode_trans_swirl) {
        if(anim_is_finished(mode_trans_swirl)) {
          anim_release(mode_trans_swirl);
          mode_trans_swirl = NULL;
          control_mode_next();
          uint8_t mode_gap = 60/control_mode_count();
          mode_disp_point = display_point(
              mode_gap*(control_mode_index(ctrl_mode_active)) % 60,
              BRIGHT_DEFAULT);
          mode_trans_blink = anim_blink(mode_disp_point,
              BLINK_INT_MED, MS_IN_TICKS(800), false);
        }
      } else if (mode_trans_blink && anim_is_finished(mode_trans_blink)) {
        display_comp_release(mode_disp_point);
        mode_disp_point = NULL;
        anim_release(mode_trans_blink);
        mode_trans_blink = NULL;
        main_gs.state = RUNNING;
      }

      break;
  }
}

static void main_init( void ) {
  /* Initalize main state */
  main_gs.modeticks = 0;
  main_gs.waketicks = 0;
  main_gs.button_hold_ticks = 0;
  main_gs.tap_count = 0;
  main_gs.inactivity_ticks = 0;
  main_gs.button_state = BUTTON_UP;
  main_gs.light_sensor_scaled_at_wakeup = 0;
  main_gs.brightness = MAX_BRIGHT_VAL;
  main_gs.state = STARTUP;

  /* Configure main timer counter */
  config_main_tc();

  /* Initialize NVM controller for data storage */
  struct nvm_config config_nvm;
  nvm_get_config_defaults(&config_nvm);
  nvm_set_config(&config_nvm);

  uint32_t data = 0xffffffff;

  /* Set row address to first open space by
   * iterating through log data until an
   * empty (4 bytes of 1s) row is found */
  while (nvm_row_addr < NVM_LOG_ADDR_MAX) {

    nvm_read_buffer(nvm_row_addr, (uint8_t *) &data, sizeof(data));
    if (data == 0xffffffff)
      break;

    nvm_row_addr+=NVMCTRL_ROW_SIZE;
  }

  /* Read configuration data stored in nvm */
  nvm_read_buffer(NVM_CONF_ADDR, (uint8_t *) &main_nvm_conf_data,
      sizeof(nvm_conf_t));
}

uint32_t main_get_waketime_ms( void ) {
  return TICKS_IN_MS(main_gs.waketicks);
}

void main_inactivity_timeout_reset( void ) {
  main_gs.inactivity_ticks = 0;
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
    _led_on_full( ((uint8_t) error_group));
    delay_ms(100);
    _led_off_full( ((uint8_t) error_group));
    delay_ms(100);
  }
}

void main_log_data( uint8_t *data, uint16_t length, bool flush) {
  /* Store data in NVM flash */
  enum status_code error_code;
  uint16_t  i, p;


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
        /* rollover to beginning of storage buffer */
        nvm_row_addr = NVM_LOG_ADDR_START;
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
  return main_gs.running_avg_vbatt;
}

uint8_t main_get_multipress_count( void ) {
  return main_gs.tap_count;
}


uint32_t main_get_button_hold_ticks ( void ) {
  return main_gs.button_hold_ticks;
}

int main (void) {
  PM_RCAUSE_Type rcause_bf;
  uint32_t reset_cause;

  system_init();
  system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);

  delay_init();
  main_init();
  aclock_init();
  led_controller_init();
  led_controller_enable();
  control_init();
  display_init();
  anim_init();
  accel_init();

  /* Log reset cause */
  rcause_bf.reg = system_get_reset_cause();
  if( rcause_bf.bit.POR || rcause_bf.bit.BOD12 || rcause_bf.bit.BOD33 ) {
    /* TODO : jump into set time mode instead of general startup */
  }
  reset_cause = ((uint32_t) 0xBADBAD00) | (0x000000FF & (uint32_t) rcause_bf.reg);
  main_log_data((uint8_t *)&reset_cause, sizeof(uint32_t), true);

  /* Errata 39.3.2 -- device may not wake up from
   * standby if nvm goes to sleep. Not needed
   * for revision D or later */
#ifdef __SAMD21E16A__
  NVMCTRL->CTRLB.bit.SLEEPPRM = NVMCTRL_CTRLB_SLEEPPRM_DISABLED_Val;
#endif

  /* Read light and vbatt sensors on startup */
#if ENABLE_VBATT
  main_set_current_sensor(sensor_vbatt);
  main_start_sensor_read();
  main_gs.running_avg_vbatt = main_read_current_sensor(true);
#endif


  LOG_WAKEUP();

  /* Show a startup LED swirl */
  sleep_wake_anim = anim_swirl(0, 8, MS_IN_TICKS(4), 172, true);

  /* get intial time */
  configure_input();
  system_interrupt_enable_global();
#ifdef ENABLE_BUTTON
  while (!port_pin_get_input_level(BUTTON_PIN)) {
    //if btn down at startup, zero out time
    //and dont continue until released
    aclock_set_time(0, 0, 0);
  }
#endif

  while (1) {
    if (tc_get_status(&main_tc) & TC_STATUS_COUNT_OVERFLOW) {
      tc_clear_status(&main_tc, TC_STATUS_COUNT_OVERFLOW);
      main_tic();
      anim_tic();
      display_tic();
    }
  }
}

// vim:shiftwidth=2
