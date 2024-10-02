#ifndef CLASSIC_H
#define CLASSIC_H

#define PS2_PORT 0x64
#define PS2_COMMAND_ENABLE_PORT1 0xAE
#define PS2_KEYBOARD_KEY_RELEASED 0x80
#define ISR_KEYBOARD 0x21
#define KEYBOARD_INPUT_PORT 0x60



struct keyboard* classic_init();

#endif
