/**
 * \file main.c
 *
 * \brief main application entry point for armio
 *
 */

#include <asf.h>

int main (void)
{
	system_init();

	while (1) {
            port_pin_set_output_level(18, 0);
	}
}
