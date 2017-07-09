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
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "chord.h"

enum
{
  MM_DOM = 0,
  MM_MAJ = 1 << 0,
  MM_MIN = 1 << 1,
  MM_DIM = 1 << 2,
  MM_AUG = 1 << 3,
  MM_SUS = 1 << 4
};

struct _MMChord
{
  char *name;
  int root;
  int quality;
  int notes[12];
};

static int dom_scale[7] = {0, 2, 4, 5, 7, 9, 10};

static int parse_root (const char *, char **);
static int parse_quality (const char *, char **);
static int parse_extension (const char *, char **);
static void set_quality (MMChord *, const char *, char **);
static void set_extension (MMChord *, const char *, char **);
static void add_alteration (MMChord *, const char *, char **);
static void set_bass (MMChord *, const char *, char **);

MMChord *
mm_chord_new (const char *name)
{
  MMChord *chord;
  char *suffix, *endptr = NULL;

  assert (name != NULL);
  chord = calloc (1, sizeof (MMChord));
  assert (chord != NULL);
  chord->name = strndup (name, 16);
  assert (chord->name != NULL);

  chord->root = parse_root (chord->name, &endptr);
  if (endptr == NULL)
    {
      mm_chord_free (chord);
      return NULL;
    }

  suffix = endptr;
  set_quality (chord, suffix, &endptr);
  suffix = endptr;
  set_extension (chord, suffix, &endptr);

  do
    {
      suffix = endptr;
      add_alteration (chord, suffix, &endptr);
    }
  while (suffix != endptr);

  suffix = endptr;
  set_bass (chord, suffix, &endptr);

  return chord;
}

void
mm_chord_free (MMChord *chord)
{
  if (chord != NULL)
    {
      if (chord->name != NULL)
        free (chord->name);
      free (chord);
    }
}

static int
parse_root (const char *note, char **endptr)
{
  int n = -1;
  char *c = (char *) note;

  switch (tolower (note[0]))
    {
    case 'c':
      n = 0;
      break;
    case 'd':
      n = 2;
      break;
    case 'e':
      n = 4;
      break;
    case 'f':
      n = 5;
      break;
    case 'g':
      n = 7;
      break;
    case 'a':
      n = 9;
      break;
    case 'b':
    case 'h':
      n = 11;
      break;
    default:
      /* Set ENDPTR to NULL to indicate error as -1 could mean Cb, Dbbb etc.  */
      *endptr = NULL;
      return n;
      break;
    }

  for (++c;;++c)
    {
      if (*c == '#')
        ++n;
      else if (tolower (*c) == 'b')
        --n;
      else
        break;
    }

  *endptr = c;

  return n;
}

static int
parse_quality (const char *quality, char **endptr)
{
  if (strncmp (quality, "maj", 3) == 0)
    {
      *endptr = (char *) quality + 3;
      return MM_MAJ;
    }
  else if (strncmp (quality, "mMaj", 4) == 0)
    {
      *endptr = (char *) quality + 4;
      return MM_MIN | MM_MAJ;
    }
  else if (strncmp (quality, "dim", 3) == 0)
    {
      *endptr = (char *) quality + 3;
      return MM_DIM;
    }
  else if (strncmp (quality, "aug", 3) == 0)
    {
      *endptr = (char *) quality + 3;
      return MM_AUG;
    }
  else if (strncmp (quality, "sus", 3) == 0)
    {
      *endptr = (char *) quality + 3;
      return MM_SUS;
    }
  else if (strncmp (quality, "m", 1) == 0)
    {
      *endptr = (char *) quality + 1;
      return MM_MIN;
    }

  *endptr = (char *) quality;
  return MM_DOM;
}

static int
parse_extension (const char *extension, char **endptr)
{
  int i;
  switch (extension[0])
    {
    case '2':
    case '4':
    case '5':
    case '6':
    case '7':
    case '9':
      i = extension[0] - '0';
      break;
    case '1':
      switch (extension[1])
        {
        case '1':
          i = 11;
          break;
        case '3':
          i = 13;
          break;
        default:
          i = 0;
          break;
        }
      break;
    default:
      i = 0;
      break;
    }

  *endptr = (char *) extension + (i > 9 ? 2 : (i > 0 ? 1 : 0));

  return i;
}

static void
set_quality (MMChord *chord, const char *quality, char **endptr)
{
  chord->quality = parse_quality (quality, endptr);
  chord->notes[0] = 1;
  chord->notes[4] = 1;
  chord->notes[7] = 1;

  if (chord->quality & (MM_MIN | MM_DIM | MM_SUS))
    chord->notes[4] = 0;
  if (chord->quality & (MM_MIN | MM_DIM))
    chord->notes[3] = 1;
  if (chord->quality & MM_MAJ)
    chord->notes[11] = 1;
  if (chord->quality & (MM_AUG | MM_DIM))
    chord->notes[7] = 0;
  if (chord->quality & MM_AUG)
    chord->notes[8] = 1;
  if (chord->quality & MM_DIM)
    chord->notes[6] = 1;
}

static void
set_extension (MMChord *chord, const char *extension, char **endptr)
{
  int ext = parse_extension (extension, endptr);
  switch (ext)
    {
    case 13:
      chord->notes[9] = 2;
    case 11:
      chord->notes[5] = 2;
    case 9:
      chord->notes[2] = 2;
    case 7:
      if ((chord->quality & MM_DIM) && ext == 7)
        chord->notes[9] = 1;
      else if (~chord->quality & MM_MAJ)
        chord->notes[10] = 1;
      break;
    case 6:
      chord->notes[9] = 1;
      break;
    case 5:
      if (~chord->quality & MM_AUG)
        chord->notes[4] = 0;
      break;
    case 4:
      if (chord->quality & MM_SUS)
        chord->notes[5] = 1;
      break;
    case 2:
      chord->notes[2] = 1;
      break;
    default:
      if (chord->quality & MM_SUS)
        chord->notes[5] = 1;
      break;
    }
}

static void
add_alteration (MMChord *chord, const char *alt, char **endptr)
{
  char *c = (char *) alt;
  int d, note, offset = 0;
  bool add = false, omit = false;

  *endptr = (char *) alt;

  if (strncmp (c, "add", 3) == 0)
    {
      add = true;
      c += 3;
    }
  else if (strncmp (c, "no", 2) == 0)
    {
      omit = true;
      c += 2;
    }

  while (*c == 'b' || *c == '#')
    {
      offset += (*c == 'b') ? -1 : 1;
      ++c;
    }

  if (!add && !omit && offset == 0)
    return;

  if (*c < '1' || *c > '9')
    return;

  d = *c - '0';
  ++c;
  if (d == 1)
    {
      if (*c != '1' && *c != '3')
        return;
      d = 10 + (*c - '0');
      ++c;
    }

  note = dom_scale[(d - 1) % 7];
  if (note == 4 && (chord->quality & (MM_MIN | MM_DIM)))
    --note;
  else if (note == 10 && (chord->quality & MM_DIM))
    --note;
  else if (note == 10 && (chord->quality & MM_MAJ))
    ++note;

  if (!add)
    chord->notes[note] = 0;

  d += offset;
  note += offset;
  while (note < 0)
    note += 12;

  if (!omit)
    chord->notes[note % 12] = d > 7 ? 2 : 1;

  *endptr = c;
}

static void
set_bass (MMChord *chord, const char *bass, char **endptr)
{
  char *c = (char *) bass;
  int note;

  *endptr = c;
  if (*c != '/')
    return;

  ++c;
  note = parse_root (c, endptr) - chord->root;
  if (*endptr == NULL)
    {
      *endptr = c;
      return;
    }

  while (note < 0)
    note += 12;

  chord->notes[note % 12] = -1;
}
