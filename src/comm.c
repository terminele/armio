/** file:       comm.c
  * author:     <<AUTHOR>>
  */

//___ I N C L U D E S ________________________________________________________
#include <asf.h>
#include "comm.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

//___ V A R I A B L E S ______________________________________________________

struct usart_module usart_instance;

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

//___ F U N C T I O N S ______________________________________________________

void comm_init(void) {
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

// vim: shiftwidth=2
