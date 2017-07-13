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

#include <portmidi.h>

#include "app.h"
#include "input.h"
#include "player.h"
#include "program.h"
#include "program_factory.h"
#include "print.h"

#define MM_INPUT_DEVICE "/dev/input/js0"

static PmDeviceID
mm_get_output_device_id ()
{
  for (PmDeviceID id = Pm_CountDevices () - 1; id >= 0; --id)
    {
      const PmDeviceInfo *dev = Pm_GetDeviceInfo (id);
      if (dev->output == 1)
        return id;
    }
  return pmNoDevice;
}

int
main (int argc, char **argv)
{
  MMApp *app;
  MMInput *input;
  MMPlayer *player;

  PmError err;
  PmDeviceID device;

  if (argc <= 1)
    {
      MMERR ("No input file");
      return EXIT_FAILURE;
    }

  input = mm_input_new ();
  if (!mm_input_connect (input, MM_INPUT_DEVICE))
    MMERR ("Could not open " MMCY (MM_INPUT_DEVICE));

  err = Pm_Initialize ();
  if (err < pmNoError)
    {
      MMERR ("Failed to initialize: " MMCY ("%s"), Pm_GetErrorText (err));
      return EXIT_FAILURE;
    }

  device = mm_get_output_device_id ();
  if (device == pmNoDevice)
    {
      MMERR ("No output device found");
      Pm_Terminate ();
      return EXIT_FAILURE;
    }

  player = mm_player_new (device);
  if (player == NULL)
    {
      Pm_Terminate ();
      return EXIT_FAILURE;
    }

  app = mm_app_new (input, player);

  for (int arg = 1; arg < argc; ++arg)
    {
      MMProgram *program = mm_program_factory (argv[arg]);
      if (program == NULL)
          continue;

      printf ("\n========\n"
              " OPENED: " MMCG ("%s")
              "\n========\n\n", argv[arg]);

      mm_app_run (app, program);
      mm_program_free (program);
    }

  mm_app_free (app);

  Pm_Terminate ();

  return EXIT_SUCCESS;
}