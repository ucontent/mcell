/******************************************************************************
 *
 * Copyright (C) 2019 by
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

#include <iostream>

#include "partition.h"

using namespace std;

namespace mcell {

void subpartition_mask_t::dump() {
  cout << "Indices contained in a subpartition: ";
  int cnt = 0;
  for (uint32_t idx: *this) {
    cout << idx << ", ";

    if (cnt %20 == 0 && cnt != 0) {
      cout << "\n";
    }
    cnt++;
  }
  cout << "\n";
}

void partition_t::dump() {
  for (geometry_object_t& obj:geometry_objects) {
    obj.dump(*this, "  ");
  }

  for (size_t i = 0; i < walls_per_subpart.size(); i++) {
    if (!walls_per_subpart[i].empty()) {
      vec3_t llf, urb;
      get_subpart_llf_point(i, llf);
      urb = llf + vec3_t(world_constants.subpartition_edge_length);

      cout << "subpart: " << i << ", llf: " << llf << ", urb: " << urb << "\n";
      walls_per_subpart[i].dump();
    }
  }
}

} // namespace mcell
