/**
 * \file main.c
 *
 * \brief main application entry point for armio
 *
 */

#include <asf.h>
#include <leds.h>

void configure_usart(void);
void configure_input(void);

struct usart_module usart_instance;

void configure_usart(void)
{
    struct usart_config config_usart;
    usart_get_config_defaults(&config_usart);

    config_usart.baudrate    = 9600;
    config_usart.mux_setting = USART_RX_3_TX_2_XCK_3;
    config_usart.pinmux_pad0 = PINMUX_UNUSED;
    config_usart.pinmux_pad1 = PINMUX_UNUSED;
    config_usart.pinmux_pad2 = PINMUX_PA30D_SERCOM1_PAD2;
    config_usart.pinmux_pad3 = PINMUX_PA31D_SERCOM1_PAD3;

    while (usart_init(&usart_instance,
                    SERCOM1, &config_usart) != STATUS_OK) {
    }

    usart_enable(&usart_instance);
}

void configure_input(void) {

    struct port_config pin_conf;
    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_INPUT;
    pin_conf.input_pull = PORT_PIN_PULL_NONE;
    port_pin_set_config(PIN_PA31, &pin_conf);
    port_pin_set_config(PIN_PA30, &pin_conf);


}

void led_swirl(int tail_len, int tick_us, int min_duty_cycle_us,
        int swirl_count) {
    int i = -tail_len;
    while(swirl_count) {
        for (i=0; i < 60; i++) {
            int advance_countdown = tick_us;
            while(advance_countdown > 0) {
                int j = 0;
                while( j < tail_len && advance_countdown > 0 ) {
                    int ontime_us = j*min_duty_cycle_us;
                    led_enable((i+j) % 60);
                    delay_us(ontime_us);
                    led_disable((i+j) % 60);
                    delay_us(ontime_us*2);
                    advance_countdown-=ontime_us*3;
                    j++;
                }

                delay_us(20);
                advance_countdown-=20;
            }
        }

    swirl_count--;
    }


}

int main (void)
{
    int btn_down = false;
    int click_count = 0;
    system_init();
    delay_init();
    led_init();


    /* Wait a bit for configuring any thing that uses SWD pins as GPIO */
    /* Show a startup LED ring */

    led_swirl(25, 800, 1, 4 );
    /*    for (i=0; i < 360; i++) {
        led_enable(i % 60);
        delay_ms(5);
        led_disable(i % 60);
    }*/

    configure_input();

    //configure_usart();

    //uint8_t string[] = "Hello World!\r\n";
    //usart_write_buffer_wait(&usart_instance, string, sizeof(string));

    while (1) {
        if (port_pin_get_input_level(PIN_PA31) &&
            port_pin_get_input_level(PIN_PA30)) {
            led_enable(click_count);
            delay_us(20);
            led_disable(click_count);
            delay_ms(300);

            if(!btn_down) {
                btn_down = true;
                click_count++;
            }
            continue;
        }

        btn_down = false;

        led_swirl(25, 800, 1, 1 );

    }

}
