#include "actualizer.h"

#include <stdio.h>

#include "json.h"

#define STRNCMP_LIMIT 50

#define EVENT_KEY "events"


/* Frame methods */

Frame* Frame_new(uint8_t* image_data, int linesize, int width, int higth, long timestamp) {
	Frame* this = malloc(sizeof(Frame));
	this->image_data = image_data;
	this->linesize = linesize;
	this->width = width;
	this->higth = higth;
	this->timestamp = timestamp;
	return this;
}

void Frame_destroy(Frame* this) {
	free(this);
}

void colorizePixel(Frame* this, Coordinate* coord, RGBA_color* touch_color) {
	if (coord->y > this->higth || coord->y < 0 || 
		 coord->x > this->width || coord->x < 0) {
		return;
	}

	uint8_t* pixel;
	pixel = this->image_data + coord->y*this->linesize + coord->x*4;

	pixel[0] = touch_color->r; // Red
	pixel[1] = touch_color->g; // Green
	pixel[2] = touch_color->b; // Blue
	pixel[3] = 255; 				// Alpha
}

void invertPixel(Frame* this, Coordinate* coord) {
	if (coord->y > this->higth || coord->y < 0 || 
		 coord->x > this->width || coord->x < 0) {
		return;
	}

	uint8_t* pixel;
	pixel = this->image_data + coord->y*this->linesize + coord->x*4;

	pixel[0] ^= 0xFF; // Red
	pixel[1] ^= 0xFF; // Green
	pixel[2] ^= 0xFF; // Blue
	pixel[3] ^= 0x00; // Alpha
}


/* RGBA_color methods */

RGBA_color* RGBA_color_new(int r, int g, int b, int a) {
	RGBA_color* this = malloc(sizeof(RGBA_color));
	this->r = r;
	this->g = g;
	this->b = b;
	this->a = a;
	return this;
}

void RGBA_color_destroy(RGBA_color* this) {
	free(this);
}


/* Coordinate methods */

Coordinate* Coordinate_new(int x, int y) {
	Coordinate* this = malloc(sizeof(Coordinate));
	this->x = x;
	this->y = y;
	return this;
}

void Coordinate_destroy(Coordinate* this) {
	free(this);
}


/* Event methods */

enum ACTION parse_action(const char* str) {
	enum ACTION action;
	if (strncmp(str, "move", STRNCMP_LIMIT) == 0) {
		action = move;
	} else if (strncmp(str, "down", STRNCMP_LIMIT) == 0 ||
	           strncmp(str, "5", STRNCMP_LIMIT) == 0 ||
	           strncmp(str, "7", STRNCMP_LIMIT) == 0 ||
	           strncmp(str, "9", STRNCMP_LIMIT) == 0) {
		action = down;
	} else if (strncmp(str, "up", STRNCMP_LIMIT) == 0 ||
	           strncmp(str, "6", STRNCMP_LIMIT) == 0 ||
	           strncmp(str, "8", STRNCMP_LIMIT) == 0 ||
	           strncmp(str, "10", STRNCMP_LIMIT) == 0) {
		action = up;
	} else {
		action = up;
	}
	return action;
}

Event* Event_new(enum ACTION action, int x, int y) {
	Event* this = malloc(sizeof(Event));
	this->action = action;
	this->coord = Coordinate_new(x, y);
	return this;
}

void Event_destroy(Event* this) {
	if (this == NULL) return;
	free(this->coord);
	free(this);
}


/* TouchData methods */

TouchData* TouchData_new(const char* filename) {
	TouchData* this = malloc(sizeof(TouchData));
	this->root = read_json((char*)filename);
	this->json_events = json_object_get(this->root, EVENT_KEY);
	json_t* color = json_object_get(this->root, "color");
	int r = json_integer_value(json_object_get(color, "r"));
	int g = json_integer_value(json_object_get(color, "g"));
	int b = json_integer_value(json_object_get(color, "b"));
	int a = json_integer_value(json_object_get(color, "a"));
	this->touch_color = RGBA_color_new(r, g, b, a);
	this->n_events = json_array_size(this->json_events);
	this->next_event = 0;
	return this;
}

void TouchData_destroy(TouchData* this) {
	if (this == NULL) return;
    RGBA_color_destroy(this->touch_color);
	json_decref(this->root);
	free(this);
}

void parse_next_event(TouchData* this, int* index, enum ACTION* action, long* timestamp, int* x, int* y) {
	json_t* event;
	const char* action_str;

	event = json_array_get(this->json_events, this->next_event);
	action_str = json_string_value( json_object_get(event, "action"));
	*index     = json_integer_value(json_object_get(event, "index"));
	*timestamp = json_integer_value(json_object_get(event, "timestamp"));
	*x         = json_integer_value(json_object_get(event, "x"));
	*y         = json_integer_value(json_object_get(event, "y"));
	*action    = parse_action(action_str);
}


/* TouchMask methods */

TouchMask* TouchMask_new(int radius) {
	TouchMask* this = malloc(sizeof(TouchMask));
	this->radius = radius;
	this->mask  = malloc((2*radius+1)*(2*radius+1)*sizeof(uint8_t));

	for (int y=-radius; y<=radius; y++) {
		for (int x=-radius; x<=radius; x++) {
			int y_index = (y+radius)*2*radius;
			if (x*x+y*y <= radius*radius) {
				this->mask[y_index+x+radius] = 255;
			} else {
				this->mask[y_index+x+radius] = 0;
			}
		}
	}

	return this;
}

void TouchMask_destroy(TouchMask* this) {
	if (this == NULL) return;
	free(this->mask);
	free(this);
}


/* TouchActualizer methods */

TouchActualizer* TouchActualizer_new(const char* filename,
		int width, int higth) {
	TouchActualizer* this = malloc(sizeof(TouchActualizer));
	this->active_events = malloc(N_ACTIVE_EVENTS*sizeof(Event*));
	for (int i=0; i<N_ACTIVE_EVENTS; i++) {
		this->active_events[i] = NULL;
	}
	this->touch_data = TouchData_new(filename);
	int min_size = width < higth ? width : higth;

	this->move_touch_mask = TouchMask_new(min_size / R_MOVE_TOUCH_RADIUS);
	this->down_touch_mask = TouchMask_new(min_size / R_DOWN_TOUCH_RADIUS);
	return this;
}

void TouchActualizer_destroy(TouchActualizer* this) {
	if (this == NULL) return;
	for (int i=0; i<N_ACTIVE_EVENTS; i++) {
		Event_destroy(this->active_events[i]);
	}
	free(this->active_events);
	TouchData_destroy(this->touch_data);
	TouchMask_destroy(this->move_touch_mask);
	TouchMask_destroy(this->down_touch_mask);
	free(this);
}

void update_active_events(TouchActualizer* this, Frame* frame) {
	int index;
	enum ACTION action;
	long timestamp;
	int x, y;

	TouchData* td = this->touch_data;
	while (td->next_event < td->n_events) {
		parse_next_event(td, &index, &action, &timestamp, &x, &y);
		if (timestamp > frame->timestamp) break;
		if (index >= td->n_events) break; // out of bounds.

		if (action == up) {
			Event_destroy(this->active_events[index]);
			this->active_events[index] = NULL;
		} else if (this->active_events[index] == NULL) {
			this->active_events[index] = Event_new(action, x, y);
		} else {
			this->active_events[index]->action = action;
			this->active_events[index]->coord->x = x;
			this->active_events[index]->coord->y = y;
		}

		td->next_event++;
	}
}

void actualizeEvent(TouchActualizer* this, Event* event, Frame* frame) {
	Coordinate coord;
	TouchMask* touch_mask = this->move_touch_mask;
	if (event->action == down) {
		touch_mask = this->down_touch_mask;
	}

	for (int y=0; y<=2*touch_mask->radius; y++) {
		int y_index = y*2*touch_mask->radius;
		for (int x=0; x<=2*touch_mask->radius; x++) {
			if (touch_mask->mask[y_index+x]) {
				coord.x = event->coord->x - touch_mask->radius + x;
				coord.y = event->coord->y - touch_mask->radius + y;
				#ifdef INVERTED_TOUCH_COLOR
					invertPixel(frame, &coord);
				#else // user defined touch color.
					colorizePixel(frame, &coord, this->touch_data->touch_color);
				#endif
			}
		}
	}
}

void actualizeEvents(TouchActualizer* this, Frame* frame) {
	for (int i=0; i<N_ACTIVE_EVENTS; i++) {
		if (this->active_events[i] == NULL) continue;
		actualizeEvent(this, this->active_events[i], frame);
	}
}

void actualize(TouchActualizer* this, Frame* frame) {
	update_active_events(this, frame);
	actualizeEvents(this, frame);
}

void revert_actualize(TouchActualizer* this, Frame* frame) {
	actualizeEvents(this, frame);
}
