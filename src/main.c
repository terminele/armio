/**
 * \file main.c
 *
 * \brief main application entry point for armio
 *
 */

#include <asf.h>
#include <leds.h>

int main (void)
{
	system_init();

	while (1) {
            led_enable(0);
	}
}
