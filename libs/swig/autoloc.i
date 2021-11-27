%module (package="seiscomp") autoloc

%newobject Seiscomp::Publication::Util::loadEventParametersForEvent;
%newobject Seiscomp::Publication::Util::readEventParametersFromXML;

%{
/* Put headers and other declarations here */
#include <string>
#include "seiscomp/core.h"
#include "seiscomp/core/baseobject.h"
#include "seiscomp/core/interruptible.h"
#include "seiscomp/core/exceptions.h"
#include "seiscomp/datamodel/eventparameters.h"
#include "seiscomp/math/geo.h"
#include "seiscomp/autoloc/autoloc.h"
#include "seiscomp/autoloc/config.h"
%}

%include "std_string.i"
%include "std_vector.i"
%include "exception.i"
%include "typemaps.i"

%import  "base.i"
%include "seiscomp/core.h"
%include "seiscomp/core/baseobject.h"
%include "seiscomp/autoloc/autoloc.h"
%include "seiscomp/autoloc/config.h"
