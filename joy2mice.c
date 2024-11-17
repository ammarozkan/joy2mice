#include <fcntl.h> // ioctl, fcntl
#include <linux/input.h> // struct input_event
#include <unistd.h> // open(), close()

#include <sys/time.h> // gettimeofday()

#include <errno.h>
#include <stdint.h>

#include <stdlib.h> // abs()
#include <stdio.h>

#define JOYSTICK_PATH "/dev/input/by-id/usb-Logitech_Wireless_Gamepad_F710_A6ED9788-event-joystick"
#define MOUSE_PATH "/dev/input/event6"


#define COUNTERLIMIT_BASE 9000.0f 			// time of signal sending before decreasing it by
											// the effect of joystick

#define COUNTERLIMIT_MAXDECREASE 7000.0f 	// maximum decrease effect of stick

#define SENSITIVITY_CONSTANT 8.0f			// how much more sensitive it should become after
											// pressing the "sensitive" button?

// I want to reach them from outside of the main function
int js; // joystick file definitor
int ms; // mouse file definitor

int sensitive = 0;

// getting a time corrected event structure
struct input_event getNewEvent()
{
	struct input_event result;
	gettimeofday(&(result.time),NULL);
	return result;
}

void
sendEvent(uint16_t type, uint16_t code, uint32_t value)
{
	struct input_event src = getNewEvent();
	src.type = type;
	src.code = code;
	src.value = value;
	printf("%u:%u:%u sending\n", type,code,value);
	write(ms, &src, sizeof(src));

	// sync?
	src.type = 0;
	src.code = 0;
	src.value = 0;
	write(ms, &src, sizeof(src));
}

int laststick[18]; // just using more memory instead of doing some if or switch cases to use sticks

int
conv_stick(struct input_event ev)
{	
	int move = *((int*)&(ev.value));
	printf("%uth STICK:%i!\n",ev.code, move);

	laststick[ev.code] = move; // storing the stick values
}

void
conv_key(uint16_t code, uint32_t value)
{
	printf("%uth key:%u\n",code,value);

	switch (code) {
		case 304: sendEvent(1, 0x110, value); break;
		case 305: sensitive = value; break; // I'll use this button for making
											// all sticks more sensitive (or we
											// can say hard to move)
		case 307: sendEvent(1, 0x111, value); break;
	}
}

void
conv_move(uint16_t code, uint32_t value)
{
	int move = *((int*)&value);
	printf("%uth move:%i\n", code, move);

}


// mouse movement requires active movement, but analog sticks signals are "change" signals.
// So, we need to have something to create active signals from those "change" signals.
int
stickToMouseMovement(struct timeval timer, struct timeval current, int stickvalue, unsigned int code)
{
	// counter limit can be decreased when stick is getting higher for increasing the speed
	// counter limit is the microseconds that should be passed for sending another signal of
	// mouse movement.
	float counterlimit = COUNTERLIMIT_BASE - COUNTERLIMIT_MAXDECREASE*(float)abs(stickvalue)/(float)INT16_MAX;
	if(sensitive) counterlimit *= SENSITIVITY_CONSTANT;
	int timestate = (float)(current.tv_usec - timer.tv_usec) > counterlimit || (current.tv_sec - timer.tv_sec) > 0;

	if (abs(stickvalue) > 128 && timestate) {
		float fvalue = 5.0f * (float)stickvalue / (float)INT16_MAX;
		//if(sensitive) fvalue *= 0.25f;
		int value = (int)fvalue;
		sendEvent(2, code, *(uint32_t*)(&value));
		return 1;
	}
	return 0;
}

// Exact same thing as the stickToMouseMovement function but MouseScroll needs
// some adjustments to work.
int
stickToMouseScroll(struct timeval timer, struct timeval current, int stickvalue,
				   unsigned int code)
{
	float counterlimit = 144000.0f;
	if (sensitive) counterlimit *= SENSITIVITY_CONSTANT;
	int timestate = (float)(current.tv_usec - timer.tv_usec) > counterlimit || (current.tv_sec - timer.tv_sec) > 0;

	if (abs(stickvalue) > 0 && timestate) {
		int value = stickvalue*120; // maybe a normal stick could be usen here instead of dpad
		sendEvent(2, code, *(uint32_t*)(&value));
		return 1;
	}
	return 0;
}

int
main()
{
	int size; // readed size variable

	char* ms_n = MOUSE_PATH; // mouse devicepath
	char* js_n = JOYSTICK_PATH; // joystick devicepath

	// resetting the last stick values
	for (unsigned int i = 0 ; i < sizeof(laststick)/sizeof(int) ; i += 1) laststick[i] = 0;

	printf("Opening the joystick to read.\n");
	js = open(js_n, O_RDONLY);

	if (js <= 0) {
		perror("Damn. Controller couldn't opened.\n");
		return -1;
	}

	printf("Setting flags.\n");
	int flags = fcntl(js, F_GETFL, 0);
	fcntl(js, F_SETFL, flags|O_NONBLOCK); // I don't want to reading to stop my program,
										  // I don't want to use threads either.

	printf("Mouse getting to be ready.\n");
	ms = open(ms_n, O_WRONLY);

	if (ms <= 0) {
		perror("Nooo. But at least we got a error.\n");
		return -2;
	}
	
	struct input_event ev; // lets allocate the readed struct here

	struct timeval lastturboclick; // turbo clicking is cool, theres a timer for it
	gettimeofday(&(lastturboclick),NULL); // and resetting the timer

	struct timeval timers[6]; // timers for one time to active signal converters

	// we have current time on lastturboclick, I'll copy it instead of getting it
	// again from the kernel (I'm not sure its really the right thing to do. But I guess it is)
	for(unsigned int i = 0 ; i < 6 ; i += 1) timers[i] = lastturboclick;

	int turboclick = 0; // turboclick active, or not?

	struct timeval current; // current time storage
	
	while (1) {
		// Getting the current time in the start of every loop is required for timers.
		gettimeofday(&(current),NULL);


		// Passive-Active conversion

		// doing the x,y movement
		for (unsigned int i = 0 ; i < 2 ; i += 1)
			if(stickToMouseMovement(timers[i], current, laststick[i], i)) gettimeofday(&(timers[i]), NULL);

		// for holding the gamepad vertically, some original adjustments needs to be done for the other joystick.
		if(stickToMouseMovement(timers[3], current, +laststick[3], 1)) gettimeofday(&(timers[3]), NULL);
		if(stickToMouseMovement(timers[4], current, -laststick[4], 0)) gettimeofday(&(timers[4]), NULL);

		// scrolling for vertical and horizontal controls
		if(stickToMouseScroll(timers[5], current, -laststick[17], 11)) gettimeofday(&(timers[5]), NULL);
		if(stickToMouseScroll(timers[6], current, -laststick[16], 11)) gettimeofday(&(timers[6]), NULL);



		// Commands

		// Turbo Click!
		if (turboclick && (current.tv_usec - lastturboclick.tv_usec > 30000 || current.tv_sec - lastturboclick.tv_sec > 0)) {
			sendEvent(1, 0x110, 1);
			sendEvent(1, 0x110, 0);
			lastturboclick = current;
		}

		// reading from joystick
		size = read(js, &ev, sizeof(ev));
		if (size != sizeof(struct input_event) && errno == EAGAIN) continue;
		else if(size != sizeof(struct input_event)) break;
		//else printf("okay brother %i <= %i\n",size,sizeof(struct input_event));

		// eham! what we got, mr. joystick?
		printf("(%i):%u.%u My type is %u:%u:%u!\n",size,ev.time.tv_sec,ev.time.tv_usec,ev.type, ev.code, ev.value);


		// Calling basic conversion functions
		
		if (ev.type == 1) conv_key(ev.code, ev.value);
		else if (ev.type == 2) conv_move(ev.code, ev.value);
		else if (ev.type == 3) conv_stick(ev);



		// Command Control

		// Turbo Click controls!

		if (ev.type == 1) {
			if(ev.code == 308 && ev.value == 1) {
				if (turboclick) turboclick = 0; else turboclick = 1;
			}
		}
	}

	// In a case of problem on reading, loop will be broken. Then, there's a problem.
	perror("\nError on event reading");
	printf("errno:%u\n",errno);
	return -3;
}
