/* actualizer.h
   Provides TouchActualizer to parse a json-file of touch events.
   Use actualize() to provide a Frame to be drawed on. 
   
   Author: Daniel Bergström, dabergst@kth.se 
   All rights reserved */

#ifndef _TOUCH_ACTUALIZER_H_
#define _TOUCH_ACTUALIZER_H_
#include <stdint.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>

#define N_ACTIVE_EVENTS 10

// Touch radius is relative the image size.
#define R_MOVE_TOUCH_RADIUS 25
#define R_DOWN_TOUCH_RADIUS 15

// Remove to enable user defined touch color.
#define INVERTED_TOUCH_COLOR

/*** Frame ***/

typedef struct Frame {
	uint8_t* image_data;
	int linesize;
	int width;
	int higth;
	long timestamp;
} Frame;

/* Constructor, free with Frame_destroy.
Note: image_data has to be allocated before contruction and freed after
destruction. */
Frame* Frame_new(uint8_t* image_data, int linesize, int width, int higth,
		long timestamp);

/* Destructor. Note: Does not free image_data. */
void Frame_destroy(Frame* this);


/*** TouchActualizer and sub-modules ***/

typedef struct RGBA_color {
	uint8_t r, g, b, a;
} RGBA_color;

enum ACTION {
	down,
	move,
	up
};

typedef struct Coordinate {
	int x, y;
} Coordinate;

typedef struct Event {
	enum ACTION action;
	Coordinate* coord;
} Event;

typedef struct TouchData {
	json_t* root;
	json_t* json_events;
	int n_events;
	int next_event;
	RGBA_color* touch_color;
} TouchData;

typedef struct TouchMask {
	uint8_t* mask;
	int radius;
} TouchMask;

typedef struct TouchActualizer {
	Event** active_events;
	TouchData* touch_data;
	TouchMask* move_touch_mask;
	TouchMask* down_touch_mask;
} TouchActualizer;

/* Contructor, free with TouchActualizer_destroy. */
TouchActualizer* TouchActualizer_new(const char* filename, int width, int higth);

/* Destructor. */
void TouchActualizer_destroy(TouchActualizer* this);

/* Actualizes active events from TouchActualizer into image_data at the given
   image_timestamp. */
void actualize(TouchActualizer* this, Frame* frame);

void revert_actualize(TouchActualizer* this, Frame* frame);

#endif // _TOUCH_ACTUALIZER_H_
