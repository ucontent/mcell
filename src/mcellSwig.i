// ===========================================================================
// Main SWIG directive file for MCELL (taken from gamer) 
// ===========================================================================

%module  mcellSwig

%{
#define SWIG_FILE_WITH_INIT
#include "mcell_init.h"
#include "mcell_misc.h"
#include "mcell_objects.h"
#include "mcell_react_out.h"
#include "mcell_reactions.h"
#include "mcell_release.h"
#include "mcell_species.h"
#include "mcell_viz.h"
#include "mcell_surfclass.h"
#include "react_output.h"
#include "deps/config.h"
#include "strfunc.h"


%}

// Code included verbatim into the generated wrapper where the Python module
//  gets initialized
%init%{
import_array();
%}

%include "mcell_init.i"
%include "mcell_misc.i"
%include "mcell_objects.i"
%include "mcell_react_out.i"
%include "mcell_reactions.i"
%include "mcell_release.i"
%include "mcell_species.i"
%include "mcell_viz.i"
%include "mcell_surfclass.i"
%include "react_output.i"
%include "deps/config.h"
%include "strfunc.i"

// Generate docstrings
%feature("autodoc", "0");

// No extra constructors for default arguments
%feature("compactdefaultargs");

