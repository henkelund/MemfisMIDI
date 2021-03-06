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

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "player.h"
#include "timer.h"
#include "print.h"

struct _MMPlayer
{
  PortMidiStream *stream;
  MMTimer *timer;
  int notes[12];
  int nnotes;
  double bpm;
  unsigned int last_sync;
  double sync_frac;
  unsigned int pulse_count;
};

static int beats_to_ms (const MMPlayer *, double);
static double ms_to_beats (const MMPlayer *, int);
static void send_notes_on (MMPlayer *, int *, int, double, double);
static void send_notes_off (MMPlayer *, int *, int);
static int array_diff_int (int *, int, int *, int, int *);

static int
mm_player_time_proc (void *time_info)
{
  MMPlayer *player = (MMPlayer *) time_info;
  /* Signed 32bit int overflows in about 25 days.  */
  return (int) mm_timer_get_age (player->timer);
}

MMPlayer *
mm_player_new (PmDeviceID device)
{
  MMPlayer *player = NULL;
  PmError err;

  player = calloc (1, sizeof (MMPlayer));
  assert (player != NULL);
  player->timer = mm_timer_new ();
  player->bpm = 120.;
  player->last_sync = 0;
  player->sync_frac = 0.;
  player->pulse_count = 0;

  err = Pm_OpenOutput (&player->stream, device, NULL, 32,
                       mm_player_time_proc, player, 1);
  if (err < pmNoError || player->stream == NULL)
    {
      mm_player_free (player);
      MMERR ("MIDI Device " MMCY ("%d") " could not be opened: " MMCY ("%s"),
             device, Pm_GetErrorText (err));
      return NULL;
    }

  return player;
}

void
mm_player_free (MMPlayer *player)
{
  if (player != NULL)
    {
      if (player->stream != NULL)
        Pm_Close (player->stream);
      mm_timer_free (player->timer);
      free (player);
    }
}

bool
mm_player_send (MMPlayer *player, int status, int data1, int data2, int delay)
{
  PmEvent event;
  PmError err;

  if (player == NULL)
    return false;

  event.message = Pm_Message (status, data1, data2);
  event.timestamp = mm_player_time_proc (player) + delay;
  err = Pm_Write (player->stream, &event, 1);
  if (err < pmNoError)
    {
      MMERR ("Message " MMCY ("0x%X") " returned " MMCY ("%s"),
             event.message, Pm_GetErrorText (err));
      return false;
    }

  return true;
}

void
mm_player_play (MMPlayer *player, const MMChord *chord)
{
  int nnotes = 12;
  int notes[nnotes];

  if (player == NULL || chord == NULL)
    return;

  mm_print_cmd ("PLAYING", true);
  printf (MMCB ("%s") "\n", mm_chord_get_name (chord));

  nnotes = mm_chord_get_notes (chord, notes, nnotes);

  if (mm_chord_get_lift (chord))
    {
      send_notes_off (player, player->notes, player->nnotes);
      send_notes_on (player, notes, nnotes,
                     mm_chord_get_delay (chord),
                     mm_chord_get_broken (chord));
    }
  else
    {
      int diff[12];
      int ndiff;

      ndiff = array_diff_int (player->notes, player->nnotes,
                              notes, nnotes,
                              diff);
      send_notes_off (player, diff, ndiff);

      ndiff = array_diff_int (notes, nnotes,
                              player->notes, player->nnotes,
                              diff);
      send_notes_on (player, diff, ndiff,
                     mm_chord_get_delay (chord),
                     mm_chord_get_broken (chord));
    }

  mm_print_cmd_end ();

  memcpy (player->notes, notes, sizeof (int) * nnotes);
  player->nnotes = nnotes;
}

bool
mm_player_killall (MMPlayer *player)
{
  if (player == NULL)
    return false;
  mm_print_cmd ("KILL ALL", false);
  player->nnotes = 0;
  memset (player->notes, 0, sizeof (int) * 12);
  return mm_player_send (player, 0xB0, 0x7B, 0x00, 0);
}

void
mm_player_set_bpm (MMPlayer *player, double bpm)
{
  if (player != NULL && bpm > 0.)
    {
      player->bpm = bpm;
      mm_print_cmd ("BPM", true);
      printf (MMCB ("%.2f") "\n", player->bpm);
      mm_print_cmd_end ();
    }
}

void
mm_player_sync_clock (MMPlayer *player)
{
  double toi; /* integral timeout part.  */
  double tof; /* fractional timeout part.  */
  unsigned int now;

  if (player == NULL || player->bpm <= 0.)
    return;

  now = mm_timer_get_age (player->timer);
  if (player->last_sync > now)
    return;

  tof = modf (2500. / player->bpm, &toi); /* 24 ppqn.  */
  while (player->last_sync <= now)
    {
      player->last_sync += toi;
      player->sync_frac += tof;
      if (player->sync_frac >= 1.)
        {
          player->last_sync += 1;
          player->sync_frac -= 1.;
        }
    }

  mm_player_send (player, 0xF8, 0x00, 0x00, player->last_sync - now);
  ++player->pulse_count;
}

bool
mm_player_get_beat (const MMPlayer *player, MMBeat *beat)
{
  int sync_dist;

  if (player == NULL || beat == NULL || player->bpm <= 0.)
    return false;

  sync_dist = (int) mm_timer_get_age (player->timer) - player->last_sync;
  beat->i = player->pulse_count / 24;
  beat->f = (double) (player->pulse_count % 24) / 24.;
  beat->f += ms_to_beats (player, sync_dist);
  beat->f -= ms_to_beats (player, 1) * player->sync_frac;
  for (; beat->f < 0.; beat->i -= 1, beat->f += 1.);

  return true;
}

int
mm_player_get_time_to_beat (const MMPlayer *player, MMBeat *beat)
{
  double diff;
  MMBeat now;

  if (beat == NULL || mm_player_get_beat (player, &now) == false)
    return 0;

  diff = (double) (beat->i - now.i) + (beat->f - now.f);

  return beats_to_ms (player, diff);
}

static int
beats_to_ms (const MMPlayer *player, double beats)
{
  if (player->bpm <= 0. || beats == 0.)
    return 0;
  return (int) ((60000. / player->bpm) * beats);
}

static double
ms_to_beats (const MMPlayer *player, int ms)
{
  if (player->bpm <= 0. || ms == 0)
    return 0.;
  return (double) ms / (60000. / player->bpm);
}

static void
send_notes_on (MMPlayer *player, int *notes, int nnotes, double delay,
               double broken)
{
  bool up = (broken >= 0) ? true : false;
  int offset = beats_to_ms (player, fmax (delay, 0.));
  int delta = beats_to_ms (player, (up ? broken : -broken));

  mm_print_cmd ("ON", true);
  for (int i = (up ? 0 : nnotes - 1);
       (up && (i < nnotes)) || (!up && (i >= 0));
       i += (up ? 1 : -1))
    {
      printf (MMCG ("%d") " ", notes[i]);
      if (offset > 0)
        printf ("+%d ", offset);
      mm_player_send (player, 0x90, notes[i], 0x7F, offset);
      offset += delta;
    }
  printf ("\n");
}

static void
send_notes_off (MMPlayer *player, int *notes, int nnotes)
{
  mm_print_cmd ("OFF", true);
  for (int i = 0; i < nnotes; ++i)
    {
      mm_player_send (player, 0x80, notes[i], 0x40, 0);
      printf (MMCY ("%d") " ", notes[i]);
    }
  printf ("\n");
}

static int
array_diff_int (int *a, int alen, int *b, int blen, int *diff)
{
  int dlen = 0;
  for (int ai = 0; ai < alen; ++ai)
    {
      bool missing = true;
      for (int bi = 0; bi < blen; ++bi)
        {
          if (b[bi] == a[ai])
            {
              missing = false;
              break;
            }
        }
      if (missing)
        diff[dlen++] = a[ai];
    }
  return dlen;
}
