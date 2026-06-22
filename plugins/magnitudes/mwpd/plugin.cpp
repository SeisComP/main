/***************************************************************************
 * SeisComP duration-amplitude moment-magnitude plugin (mwpd)              *
 *                                                                         *
 * Plugin entry point. Registers the Mwpd amplitude and magnitude          *
 * processors (a port of Early-est 1.2.9's Mwpd, Lomax & Michelini 2009).  *
 *                                                                         *
 * GNU Affero General Public License Usage - see LICENSE.                  *
 ***************************************************************************/


#include <seiscomp/core/plugin.h>

#include "version.h"


ADD_SC_PLUGIN(
	"Duration-amplitude moment magnitude Mwpd: per-station moment magnitude "
	"from the integral of the displacement over the source duration T0 "
	"(port of Early-est 1.2.9, Lomax & Michelini 2009).",
	"Early-est port",
	MWPD_VERSION_MAJOR, MWPD_VERSION_MINOR, MWPD_VERSION_PATCH
)
