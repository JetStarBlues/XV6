// Constants used for talking to the PS/2 (8042) controller

/*
Based on:
	. https://wiki.osdev.org/%228042%22_PS/2_Controller
	. https://wiki.osdev.org/PS/2_Mouse
	. http://www.brokenthorn.com/Resources/OSDev19.html
*/

#define PS2CTRL 0x64  // Command/status port
#define PS2DATA 0x60  // Data port

/* Data (byte) is transmitted serially.
   Status bits are used to determine if all bits have
   been read in or written out.
*/
#define PS2DINFULL  0x01  // Buffer holding data from device to PS/2 is full; Must be full before we can read
#define PS2DOUTFULL 0x02  // Buffer holding data from PS/2 to device is full; Must be empty before we can write


#define PS2CMD_DEV2EN  0xA8  // Enable the secondary PS/2 device
#define PS2CMD_DEV2SEL 0xD4  // Tell the controller we intend to send data to the secondary device

/* Configuration byte
    0 - device 1 interrupt enabled
    1 - device 2 interrupt enabled
    2 - system flag
    3 - always zero
    4 - device 1 clock enabled
    5 - device 2 clock enabled
    6 - device 1 port translation enabled
    7 - always zero
*/
#define PS2CMD_RDCONFG  0x20  // Read configuration byte
#define PS2CMD_WRCONFG  0x60  // Write configuration byte
#define PS2CONFG_DEV2EI 0x02  // Enable interrupts of secondary device
