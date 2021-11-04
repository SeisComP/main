/***************************************************************************
 * Copyright (C) 2020 by gempa GmbH                                        *
 * All rights reserved.                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/


#define SEISCOMP_COMPONENT DataAvailabilityCollector

#include "collector.h"

#include <seiscomp/core/exceptions.h>
#include <seiscomp/core/interfacefactory.ipp>
#include <seiscomp/core/system.h>

#include <seiscomp/logging/log.h>

#include <boost/filesystem/convenience.hpp>

IMPLEMENT_INTERFACE_FACTORY(Seiscomp::DataAvailability::Collector, SC_DAPLUGIN_API);

namespace Seiscomp {
namespace DataAvailability {

IMPLEMENT_SC_ABSTRACT_CLASS_DERIVED(Collector,
                                    Seiscomp::Core::InterruptibleObject,
                                    "Collector");
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
CollectorException::CollectorException()
: Core::GeneralException("data availability collector error") {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
CollectorException::CollectorException(std::string what)
: Core::GeneralException(std::move(what)) {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Collector::RecordIterator::handleInterrupt(int) {
	_abortRequested = true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Collector::setSource(const char *source) {
	reset();
	_source = source;
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Collector::reset() {
	_abortRequested = false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Core::Time Collector::chunkMTime(const std::string &/*chunk*/) {
	return {};
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Collector *Collector::Create(const char *service) {
	if ( service == nullptr )
		return nullptr;

	return CollectorFactory::Create(service);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Collector* Collector::Open(const char *url) {
	const char *tmp;
	std::string service;
	std::string source;

	tmp = strstr(url, "://");
	if ( tmp ) {
		std::copy(url, tmp, std::back_inserter(service));
		source = tmp + 3;
	}
	else {
		service = "sds";
		source = url;
	}

	SEISCOMP_DEBUG("trying to open data availability collector %s://%s",
	               service.c_str(), source.c_str());

	Collector *dac;

	dac = Create(service.c_str());
	if ( dac ) {
		try {
			if ( !dac->setSource(source.c_str()) ) {
				delete dac;
				dac = nullptr;
			}
		}
		catch ( CollectorException &e ) {
			SEISCOMP_ERROR("data availability collector exception: %s", e.what());
			delete dac;
			dac = nullptr;
		}
	}
	return dac;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Collector::handleInterrupt(int) {
	_abortRequested = true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} // ns DataAvailability
} // ns Seiscomp
