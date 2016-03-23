/** file:       utils.c
  * created:    2015-02-25 11:18:42
  * author:     Richard Bryan
  */

//___ I N C L U D E S ________________________________________________________
#include "accel.h"
#include "leds.h"
#include "main.h"
#include "utils.h"

#include <math.h>

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define PI 3.14159265

#define RAD_2_DEG(rad) (180 * rad/PI )

/* Given an angle in degrees, round to nearest clock minute hand position */
#define CLOCK_POS(angle_deg) ((((int32_t)(90 + (360 - angle_deg)) % 360) + 3)/6)

/* Given a pos 0-59, give unit circle angle in degrees (e.g. 15 is 0 deg) */
#define POS_2_DEG(pos) (90 + 360 - (pos* 6.0))

/* Convert angle difference to shortest distance on circle */
#define ANGLE_DELTA_SHORTEST(a) ((a) > 180 ? (a - 360) : (a) < -180 ? (a + 360) :a)

/* Exponential average weight for updating angular velocity */
#define W_ALPHA 0.75

/* Minimum angular velocity value */
#define MIN_ANG_VEL 2.0

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

//___ V A R I A B L E S ______________________________________________________
static float tracker_pos_angle;
static int8_t tracker_pos;
static uint16_t z_avg = ACCEL_VALUE_1G;
static float angular_velocity = 0; /* in degrees/tick */

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

//___ F U N C T I O N S ______________________________________________________

void utils_spin_tracker_start( uint8_t initial_pos ) {
    tracker_pos = initial_pos;
    tracker_pos_angle = POS_2_DEG(initial_pos);
}

void utils_spin_tracker_end ( void ) {
  //nothing to do currently
  //may use higher resolution/frequency accelerometer
  //mode in future that we'd revert back to default here
}

uint8_t utils_spin_tracker_update ( void ) {
    int16_t x, y, z;
    float angle, angle_delta;

    if (!accel_data_read(&x, &y, &z)) {
        return tracker_pos;
    }

    /* Exp average z to filter out high frequency vals
     * This allows us to still update position at
     * small tilts, but also to keep the position stable
     * when flat (i.e. z close to 1G) */
    z_avg/=8;
    z_avg+=abs(z)*7/8;

    if (abs(z) > ACCEL_VALUE_1G*7/8 ||
        abs(z_avg) > ACCEL_VALUE_1G*7/8) {
        return tracker_pos;
    }

    if (abs(x) + abs(y) < 5) return tracker_pos;

    angle = RAD_2_DEG(atan2(-y, -x));
    /* atan return [-180,180], normalize to [0, 360] */

    if (angle < 0) angle +=360;
    angle_delta = ANGLE_DELTA_SHORTEST(angle - tracker_pos_angle);

    /* Ignore small changes */
    if (abs(angle_delta) < 1) {
        angular_velocity = 0;
        return tracker_pos;
    }

    /* Update angular velocity */
    angular_velocity *= (1 - W_ALPHA);
    angular_velocity += W_ALPHA*angle_delta * (abs(y) + abs(x));

    if (abs(angular_velocity) < MIN_ANG_VEL)
        angular_velocity = angular_velocity < 0 ? -MIN_ANG_VEL : MIN_ANG_VEL;

    tracker_pos_angle += angular_velocity/1000.0;

    /* Ensure angle is in [0, 360] */
    while(tracker_pos_angle > 360) tracker_pos_angle-=360;
    while(tracker_pos_angle < 0) tracker_pos_angle+=360;

    tracker_pos = CLOCK_POS(tracker_pos_angle);
    /* Check for edge case rounding errors */
    if (tracker_pos > 59) tracker_pos = 59;
    if (tracker_pos < 0) tracker_pos = 0;

    return (uint8_t)tracker_pos;
}

uint8_t adc_light_value_scale ( uint16_t value ) {
    /* Assumes 16-bit averaged adc read */

    /* Decimate down to 12-bit */
    value >>= 4;

    if (value >= 2048)
        return 50 + ((value - 2048 ) >> 8);

    if (value >= 1024)
        return 42 + ((value - 1024) >> 7);

    if (value >= 512)
        return 34 + ((value - 512) >> 6);

    if (value >= 256)
        return 26 + ((value - 256) >> 5);

    if (value >= 128)
        return 18 + ((value - 128) >> 4);

    if (value >= 64)
        return 11 + ((value - 64) >> 3);

    if (value >= 32)
        return 7 + ((value - 32) >> 3);

    return value >> 2;
}

uint8_t adc_vbatt_value_scale ( uint16_t value ) {
    /* Assumes 16-bit averaged adc read */
    /* Decimate down to 12-bit */
    value >>= 4;

    /* Full */
    if (value >= 3000) //> 3V --> 3/4*4096
        return 60;

    /* Greater than 3/4 full */
    if (value >= 2900) // ~2.9V 
        return 45 + 14*(value - 2900)/100;

    /* Greater than 1/2 full */
    if (value >= 2800) // ~2.8V --> 2.8/4*4096
        return 30 + 14*(value - 2800)/100;

    /* between 1/4 and 1/2 full */
    if (value >= 2700) // ~2.7V
        return 15 + 14*(value - 2700)/100;

    /* between 1/8 and 1/4 full */
    if (value >= 2600) // ~2.5
        return 7 + 7*(value - 2600)/100;

    if (value <= 2048) // < 2V
        return 1;

    return 1 + 7*(value - 2048)/(2600 - 2048);
}

#if RTC_CALIBRATE

struct tcc_module osc32k_tcc_instance;
struct tcc_module ref_tcc_instance;
struct events_resource ref_rise_event_resource;
struct events_resource osc_period_start_event_resource;
struct events_resource osc_period_stop_event_resource;
struct tcc_events ref_tcc_inc_events_tcc;
struct tcc_events osc_tcc_events_tcc;

static void rtc_cal_init( void ) {
  /* Configure SWDIO as input for ref signal */
    struct port_config pin_conf;
    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_INPUT;
    port_pin_set_config(PIN_PA31, &pin_conf);

    struct extint_chan_conf eint_chan_conf;
    eint_chan_conf.gpio_pin             = PIN_PA31A_EIC_EXTINT11;
    eint_chan_conf.gpio_pin_mux         = PINMUX_PA31A_EIC_EXTINT11;
    eint_chan_conf.gpio_pin_pull        = EXTINT_PULL_NONE;
    eint_chan_conf.detection_criteria   = EXTINT_DETECT_RISING;
    eint_chan_conf.filter_input_signal  = false;
    eint_chan_conf.wake_if_sleeping     = false;

    extint_chan_set_config(11, &eint_chan_conf);

  
    struct extint_events config_extint_events = {
        .generate_event_on_detect[11] = true
    };

    extint_enable_events(&config_extint_events);
    /* Configure ref signal to generate rising edge events */
    struct events_config config;
    events_get_config_defaults(&config);
    config.generator      = EVSYS_ID_GEN_EIC_EXTINT_11;
    config.edge_detect    = EVENTS_EDGE_DETECT_NONE;
    config.path           = EVENTS_PATH_ASYNCHRONOUS;
    config.clock_source   = GCLK_GENERATOR_0;
    events_allocate(&ref_rise_event_resource, &config);

    /* Configure TCC0 to count on events from ref signal interrupts */
    struct tcc_config config_tcc;
    tcc_get_config_defaults(&config_tcc, TCC0);
    config_tcc.counter.clock_source = GCLK_GENERATOR_0;
    config_tcc.counter.clock_prescaler = TCC_CLOCK_PRESCALER_DIV1;
    config_tcc.capture.channel_function[1] = TCC_CHANNEL_FUNCTION_CAPTURE;
    tcc_init(&ref_tcc_instance, TCC0, &config_tcc);

    ref_tcc_inc_events_tcc.input_config[0].modify_action = true;
    ref_tcc_inc_events_tcc.input_config[0].action = TCC_EVENT0_ACTION_COUNT_EVENT;
    ref_tcc_inc_events_tcc.on_input_event_perform_action[0] = true;
    ref_tcc_inc_events_tcc.input_config[1].modify_action = true;
    ref_tcc_inc_events_tcc.input_config[1].action = TCC_EVENT1_ACTION_RETRIGGER;
    ref_tcc_inc_events_tcc.on_input_event_perform_action[1] = true;
    ref_tcc_inc_events_tcc.on_event_perform_channel_action[1] = true;

    /* Attach event generated by ref interrupt to TCC0 event channel 0 */
    events_attach_user(&ref_rise_event_resource, EVSYS_ID_USER_TCC0_EV_0);

    /* Configure TCC2 to count the XOSC32K clock */
    tcc_get_config_defaults(&config_tcc, TCC2);
    config_tcc.counter.clock_source = GCLK_GENERATOR_3;
    config_tcc.counter.clock_prescaler = TCC_CLOCK_PRESCALER_DIV1;
    config_tcc.compare.match[0] = 100;
    config_tcc.compare.match[1] = config_tcc.compare.match[0] + 32768;
    config_tcc.counter.period = config_tcc.compare.match[1] + 327; //~10 ms gap

    tcc_init(&osc32k_tcc_instance, TCC2, &config_tcc);

    /* Create event for 1 second period of 32KHz osc */ 
    events_get_config_defaults(&config);
    config.generator      = EVSYS_ID_GEN_TCC2_MCX_0;
    config.edge_detect    = EVENTS_EDGE_DETECT_RISING;
    config.path           = EVENTS_PATH_RESYNCHRONIZED;
    config.clock_source   = GCLK_GENERATOR_0;
    events_allocate(&osc_period_start_event_resource, &config);
    events_attach_user(&osc_period_start_event_resource, EVSYS_ID_USER_TCC0_EV_1);

    config.generator      = EVSYS_ID_GEN_TCC2_MCX_1;
    events_allocate(&osc_period_stop_event_resource, &config);
    events_attach_user(&osc_period_stop_event_resource, EVSYS_ID_USER_TCC0_MC_1);
    osc_tcc_events_tcc.generate_event_on_channel[0] = true;
    osc_tcc_events_tcc.generate_event_on_channel[1] = true;
    osc_tcc_events_tcc.generate_event_on_counter_overflow = true;

    tcc_enable_events(&osc32k_tcc_instance, &osc_tcc_events_tcc);
    tcc_enable_events(&ref_tcc_instance, &ref_tcc_inc_events_tcc);

    tcc_enable(&ref_tcc_instance);

    tcc_enable(&osc32k_tcc_instance);
}

void rtc_cal_run( void ) {
#define MAX_SAMPLE_COUNT 50
#define RTC_CAL_SAMPLES 5
    int32_t capture_val_sum = 0;
    uint8_t samples = 0, valid_samples = 0;

    rtc_cal_init();

    while ( valid_samples < RTC_CAL_SAMPLES ) {
        if (ref_tcc_instance.hw->STATUS.reg & TCC_STATUS_CCBV1) {
            int32_t capture_val = tcc_get_capture_value(&ref_tcc_instance, 1);
            uint8_t status = 0;

            if (capture_val < 5e5) {
                status = 1;
            } else if (capture_val < 6e6 ){
                status = 5;
            } else if (capture_val < 9e6 ){
                status = 6;
            } else if (capture_val < 9997000 ){ /* 300 ppm too fast */
                status = 9;
            } else if (capture_val < 10003000 ){ 
                status = 10;      /* +-300pm */
            } else if (capture_val < 11e6 ){ /* 300 ppm too to slow */
                status = 11;
            } else {
                status = 12;
            }

            main_log_data((uint8_t *)&capture_val, sizeof(capture_val), true);

            _led_on_full(status);
            delay_ms(200);
            _led_off_full(status);
            
            samples++;

            if (samples >= MAX_SAMPLE_COUNT) {
                main_terminate_in_error(error_group_rtc, 0xff);
            }
            

            if (status == 10) { //only include in-bounds samples
                if (valid_samples > 0) {
                    if (capture_val - capture_val_sum/valid_samples > 200)  {
                        /* Reset the run if current capture value 
                         * differs from previous values siginifcantly 
                         */
                        valid_samples = 0;
                        capture_val_sum = 0;
                        _led_on_full(50);
                        delay_ms(100);
                        _led_off_full(50);
                        delay_ms(100);
                        _led_on_full(50);
                        delay_ms(100);
                        _led_off_full(50);

                        continue;
                    }
                }

                valid_samples++;  
                capture_val_sum+=capture_val;
            }
        }
    }
    
    /* Average over sample count */
    capture_val_sum /= valid_samples;

    main_nvm_data.rtc_freq_corr = (capture_val_sum - 10e6)/10;

    if (main_nvm_data.rtc_freq_corr < -127) {
        main_terminate_in_error(error_group_rtc, 0xf0);
    } else if (main_nvm_data.rtc_freq_corr > 127){
        main_terminate_in_error(error_group_rtc, 0x0f);
    }
    
    /* ### the frequency correction sign is actually flipped for the SAMD21 
     * (i.e. a positive val will decrease the frequency and a negative value
     * will increase the frequency).  The datasheet says the opposite
     * but it is wrong according to the experiments we have run 
     */
    main_nvm_data.rtc_freq_corr*=-1;
    nvm_update_buffer(NVM_DATA_ADDR, (uint8_t *) &main_nvm_data, 0,
             sizeof(nvm_data_t));
    
    /* Display final RTC cal val by sequencing digits on led hours */
    uint8_t digit = 0;
    uint8_t divisor = 100;
    while(1) {
        if (digit == 0) {
            /* For the first digit either display a 0 (for positive) or
             * 55 (for negative)
             */
            delay_ms(500);
            _led_on_full(main_nvm_data.rtc_freq_corr < 0 ? 55 : 0);
            delay_ms(200);
            _led_off_full(main_nvm_data.rtc_freq_corr < 0 ? 55 : 0);
            divisor = 100;
        } else {
            int8_t disp_val = main_nvm_data.rtc_freq_corr / divisor;
            disp_val = disp_val < 0 ? -1*disp_val : disp_val;
            _led_on_full(5*(disp_val % 10));
            delay_ms(200);
            _led_off_full(5*(disp_val % 10));
            divisor/=10;
        }

        digit++;
        digit%=4; //only display 4 digits 
        delay_ms(100);
    }
    
}

#endif /* RTC_CALIBRATE */
