/* Copyright (C) 2017 Henrik Hedelund.

   This file is part of MemfisMIDI.

   MemfisMIDI is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   MemfisMIDI is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with MemfisMIDI.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef MM_TIMER_H
#define MM_TIMER_H 1

#include <stdbool.h>

typedef struct _MMTimer MMTimer;

MMTimer *mm_timer_new ();
void mm_timer_free (MMTimer *);
bool mm_timer_reset (MMTimer *);
unsigned int mm_timer_get_age (const MMTimer *);
void mm_timer_tap (MMTimer *);
void mm_timer_reset_tap (MMTimer *);
double mm_timer_get_bpm (const MMTimer *);

void mm_sleep (unsigned int);

#endif /* ! MM_TIMER_H */
