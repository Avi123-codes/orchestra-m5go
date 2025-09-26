#pragma once
#include "orchestra.h"   // for song_type_t if needed

void display_init(void);
void display_idle(void);
void display_start_animation(song_type_t type);
void display_stop_animation(void);
