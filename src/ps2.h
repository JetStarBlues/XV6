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


#define PS2CMD_MOUSESEL 0xD4  // Tell the controller to address the mouse
#define PS2CMD_MOUSEACTIVATE 0xA8  // Enable the secondary PS/2 device


0x20  // Read Compaq status byte ??
0x60  // Write Compaq status byte ??

0x02  // Enable interrupts of secondary device

0xF4
0xF6

