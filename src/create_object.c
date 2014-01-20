/***********************************************************************************
 *                                                                                 *
 * Copyright (C) 2006-2013 by                                                      *
 * The Salk Institute for Biological Studies and                                   *
 * Pittsburgh Supercomputing Center, Carnegie Mellon University                    *
 *                                                                                 *
 * This program is free software; you can redistribute it and/or                   *
 * modify it under the terms of the GNU General Public License                     *
 * as published by the Free Software Foundation; either version 2                  *
 * of the License, or (at your option) any later version.                          *
 *                                                                                 *
 * This program is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                  *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                   *
 * GNU General Public License for more details.                                    *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License               *
 * along with this program; if not, write to the Free Software                     *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. *
 *                                                                                 *
 ***********************************************************************************/

#include "create_object.h"
#include "logging.h"
#include "sym_table.h"
#include "mem_util.h"

#include <stdlib.h>



/*************************************************************************
 push_object_name:
    Append a name component to the name list. For instance, consider this
    portion of an MDL:
    
    INSTANTIATE Scene OBJECT
    {
      MetaBox OBJECT {
        MyBox1 OBJECT MyBox{}
        MyBox2 OBJECT MyBox{TRANSLATE = [1, 0, 0]}
      }
    }

    When parsing the line beginning with My_Box1, return Scene.MetaBox.MyBox1
    

 In:  obj_creation: information about object being created
      name: new name component
 Out: object name stack is updated, returns new qualified object name
*************************************************************************/
char *
push_object_name(struct object_creation *obj_creation, char *name)
{

  // Initialize object name list 
  if (obj_creation->object_name_list == NULL)
  {
    obj_creation->object_name_list = CHECKED_MALLOC_STRUCT(
      struct name_list, "object name stack");
    if (obj_creation->object_name_list == NULL) {
      return NULL;
    }

    obj_creation->object_name_list->name = NULL;
    obj_creation->object_name_list->prev = NULL;
    obj_creation->object_name_list->next = NULL;
    obj_creation->object_name_list_end = obj_creation->object_name_list;
  }

  /* If the last element is available, just use it.  This typically occurs only
   * for the first item in the list. */
  if (obj_creation->object_name_list_end->name == NULL)
  {
    obj_creation->object_name_list_end->name = name;
    return obj_creation->object_name_list_end->name;
  }

  // If we've run out of name list components, create a new one
  struct name_list *name_list_ptr;
  if (obj_creation->object_name_list_end->next == NULL)
  {
    name_list_ptr = CHECKED_MALLOC_STRUCT(
      struct name_list, "object name stack");
    if (name_list_ptr == NULL) {
      return NULL;
    }

    name_list_ptr->next = NULL;
    name_list_ptr->prev = obj_creation->object_name_list_end;
    obj_creation->object_name_list_end->next = name_list_ptr;
  }
  else {
    name_list_ptr = obj_creation->object_name_list_end->next;
  }

  // Create new name 
  name_list_ptr->name = CHECKED_SPRINTF(
    "%s.%s",
    obj_creation->object_name_list_end->name,
    name);
  if (name_list_ptr->name == NULL) {
    return NULL;
  }

  obj_creation->object_name_list_end = name_list_ptr;

  return name_list_ptr->name;
}



/*************************************************************************
 make_new_object:
    Create a new object, adding it to the global symbol table.

 In:  state: system state
      obj_name: fully qualified object name
 Out: the newly created object
*************************************************************************/
struct object *
make_new_object(MCELL_STATE* state, char *obj_name)
{

  struct sym_table *symbol;
  if ((symbol = store_sym(obj_name, OBJ, state->obj_sym_table, NULL)) == NULL) {
    return NULL;
  }

  return (struct object *) symbol->value;
}