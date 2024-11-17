# joy2mice
a C program to use a joystick as a mouse

This program doesn't creates a mouse device, just reads signals from the joystick
and convertes them somehow for writing it to a existing mouse device.

## Requirements

- to run: linux, evdev
- to compile: linux, gcc

## Usage

User needs to change the device paths to his own device paths.
On the line 13 and 14.

Then program can be compiled directly if no customization needed.

### Keys

In my Logitech F710 Gamepad:

#### Holding the Gamepad Horizontally:

- Left Analog Stick: Position Control
- A : Left Click
- X : Right Click
- B : Say Program to Be Sensitive (needs to be holden)
- Y : Turbo Left Click! (on/off button)
- D-Pad (up-down) : Scroll. 

#### Holding Vertical!

I wanted also to control holding the gamepad vertically. By rotating
it (ABXY buttons needs to be close to the observer/user) right analog
stick can be used for position and D-Pad (up-down for the user) can be
used for scrolling.

## Customization

### Easy Changable Customization

Line 17, 20, 22 has some definitions with their meanings.

### Direct Source Code Customization

Source code is quite customizable. It's actually really easy to modify
classical signal converting (buttons).

For modifying mouse position controls, stickToMouseMovement function 
needs to understanded.

stickToMouseMovement(timer, currenttime, stickvalue, mouse's active position control signal code)

Little information about the mouse signal codes: 0 and 1 are the x,y position controls of mouse and 11 is the scroll. Stick
value arrays are the analog informations from the joystick.
