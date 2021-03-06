/******************************************************************************
 *
 * Copyright (C) 2006-2017 by
 * The Salk Institute for Biological Studies and
 * Pittsburgh Supercomputing Center, Carnegie Mellon University
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
******************************************************************************/

#include "config.h"

#include <stdlib.h>

#include "mcell_init.h"
#include "mcell_misc.h"
#include "mcell_run.h"
#include "init.h"

#include "dump_state.h"
#include "mcell3_world_converter.h"

#define CHECKED_CALL_EXIT(function, error_message)                             \
  {                                                                            \
    if (function) {                                                            \
      mcell_print(error_message);                                              \
      exit(1);                                                                 \
    }                                                                          \
  }

int main(int argc, char **argv) {
  u_int procnum = 0;

  // initialize the mcell simulation
  MCELL_STATE *state = mcell_create();
  CHECKED_CALL_EXIT(!state, "Failed to initialize MCell simulation.");

  // Parse the command line arguments and print out errors if necessary.
  if (mcell_argparse(argc, argv, state)) {
    if (procnum == 0) {
      mcell_print_version();
      mcell_print_usage(argv[0]);
    }
    exit(1);
  }

  // Somehow the state.dump_level is correct here, but becomes 0 in mcell_init_state, so save it and restore it.
  long saved_dump_level = state->dump_level;
  double saved_bond_angle = state->bond_angle; // Might as well do the same for the bond angle!!

  CHECKED_CALL_EXIT(
      mcell_init_state(state),
      "An error occured during set up of the initial simulation state");

  state->dump_level = saved_dump_level;
  state->bond_angle = saved_bond_angle; // Might as well do the same for the bond angle!!

  if (state->notify->progress_report != NOTIFY_NONE) {
    mcell_print_version();
  }

  CHECKED_CALL_EXIT(parse_input(state),
                    "An error occured during parsing of the mdl file.");

  CHECKED_CALL_EXIT(mcell_init_simulation(state),
                    "An error occured during simulation creation.");

  CHECKED_CALL_EXIT(
      mcell_init_read_checkpoint(state),
      "An error occured during initialization and reading of checkpoint.");

  CHECKED_CALL_EXIT(mcell_init_output(state),
                    "An error occured during setting up of output.");

  if (state->dump_mcell4) {
    dump_volume(state, "initial", DUMP_EVERYTHING);
  }
  //state->use_mcell4 = 1;

  if (state->use_mcell4) {
    if (!mcell4_convert_mcell3_volume(state)) {
      exit(EXIT_FAILURE);
    }

    mcell4_run_simulation();

    mcell4_delete_world();
  }
  else {
    CHECKED_CALL_EXIT(mcell_run_simulation(state),
                      "Error running mcell simulation.");

    if (state->notify->progress_report != NOTIFY_NONE) {
      mcell_print("Done running.");
    }

    mcell_print_stats();
  }
  exit(0);
}
