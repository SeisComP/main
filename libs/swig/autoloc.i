%module (package="seiscomp") autoloc

%newobject Seiscomp::Autoloc::inventoryFromStationLocationFile;

%{
/* Put headers and other declarations here */
#include <string>
#include "seiscomp/core.h"
#include "seiscomp/core/baseobject.h"
#include "seiscomp/core/interruptible.h"
#include "seiscomp/core/exceptions.h"
#include "seiscomp/datamodel/publicobject.h"
#include "seiscomp/datamodel/network.h"
#include "seiscomp/datamodel/inventory_package.h"
#include "seiscomp/datamodel/inventory.h"
#include "seiscomp/autoloc/autoloc.h"
#include "seiscomp/autoloc/config.h"
#include "seiscomp/autoloc/stationlocationfile.h"
#include "seiscomp/autoloc/stationconfig.h"
#include "seiscomp/autoloc/objectqueue.h"
%}

%include "std_string.i"
%include "std_vector.i"
%include "exception.i"
%include "typemaps.i"

namespace std {
    %template(StringVector)  vector<string>;
}

%import  "base.i"
%include "seiscomp/core.h"
%include "seiscomp/core/baseobject.h"
%include "seiscomp/datamodel/object.h"
%include "seiscomp/datamodel/publicobject.h"
%import  "seiscomp/datamodel/stationgroup.h"
%import  "seiscomp/datamodel/auxdevice.h"
%import  "seiscomp/datamodel/sensor.h"
%import  "seiscomp/datamodel/datalogger.h"
%import  "seiscomp/datamodel/responsepaz.h"
%import  "seiscomp/datamodel/responsefir.h"
%import  "seiscomp/datamodel/responseiir.h"
%import  "seiscomp/datamodel/responsepolynomial.h"
%import  "seiscomp/datamodel/responsefap.h"
%import  "seiscomp/datamodel/network.h"
%import  "seiscomp/datamodel/inventory.h"
%include "seiscomp/autoloc/autoloc.h"
%include "seiscomp/autoloc/config.h"
%include "seiscomp/autoloc/stationlocationfile.h"
%include "seiscomp/autoloc/stationconfig.h"
%include "seiscomp/autoloc/objectqueue.h"

%pythonbegin %{
import seiscomp.datamodel
%}

%extend Seiscomp::Autoloc::PublicObjectQueue {
  %pythoncode %{
    def __iter__ (self):
        return self

    def next (self):
        if self.empty():
            raise StopIteration
        o = self.front()
        self.pop()

        pick = seiscomp.datamodel.Pick.Cast(o)
        if pick:
            return pick
        amplitude = seiscomp.datamodel.Amplitude.Cast(o)
        if amplitude:
            return amplitude
        origin = seiscomp.datamodel.Origin.Cast(o)
        if origin:
            return origin

    def __next__(self):
        return self.next()
  %}
};

