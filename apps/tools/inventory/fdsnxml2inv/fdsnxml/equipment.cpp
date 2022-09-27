/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
 * Contact: gempa GmbH (seiscomp-dev@gempa.de)                             *
 *                                                                         *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/


#define SEISCOMP_COMPONENT SWE
#include <fdsnxml/equipment.h>
#include <fdsnxml/datetype.h>
#include <fdsnxml/identifier.h>
#include <algorithm>
#include <seiscomp/logging/log.h>


namespace Seiscomp {
namespace FDSNXML {


Equipment::MetaObject::MetaObject(const Core::RTTI *rtti, const Core::MetaObject *base) : Core::MetaObject(rtti, base) {
	addProperty(Core::simpleProperty("Type", "string", false, false, false, false, false, false, NULL, &Equipment::setType, &Equipment::type));
	addProperty(Core::simpleProperty("Description", "string", false, false, false, false, false, false, NULL, &Equipment::setDescription, &Equipment::description));
	addProperty(Core::simpleProperty("Manufacturer", "string", false, false, false, false, false, false, NULL, &Equipment::setManufacturer, &Equipment::manufacturer));
	addProperty(Core::simpleProperty("Vendor", "string", false, false, false, false, false, false, NULL, &Equipment::setVendor, &Equipment::vendor));
	addProperty(Core::simpleProperty("Model", "string", false, false, false, false, false, false, NULL, &Equipment::setModel, &Equipment::model));
	addProperty(Core::simpleProperty("SerialNumber", "string", false, false, false, false, false, false, NULL, &Equipment::setSerialNumber, &Equipment::serialNumber));
	addProperty(Core::simpleProperty("InstallationDate", "datetime", false, false, false, false, true, false, NULL, &Equipment::setInstallationDate, &Equipment::installationDate));
	addProperty(Core::simpleProperty("RemovalDate", "datetime", false, false, false, false, true, false, NULL, &Equipment::setRemovalDate, &Equipment::removalDate));
	addProperty(arrayClassProperty<DateType>("CalibrationDate", "FDSNXML::DateType", &Equipment::calibrationDateCount, &Equipment::calibrationDate, static_cast<bool (Equipment::*)(DateType*)>(&Equipment::addCalibrationDate), &Equipment::removeCalibrationDate, static_cast<bool (Equipment::*)(DateType*)>(&Equipment::removeCalibrationDate)));
	addProperty(Core::simpleProperty("resourceId", "string", false, false, false, false, false, false, NULL, &Equipment::setResourceId, &Equipment::resourceId));
	addProperty(arrayClassProperty<Identifier>("identifier", "FDSNXML::Identifier", &Equipment::identifierCount, &Equipment::identifier, static_cast<bool (Equipment::*)(Identifier*)>(&Equipment::addIdentifier), &Equipment::removeIdentifier, static_cast<bool (Equipment::*)(Identifier*)>(&Equipment::removeIdentifier)));
}


IMPLEMENT_RTTI(Equipment, "FDSNXML::Equipment", Core::BaseObject)
IMPLEMENT_RTTI_METHODS(Equipment)
IMPLEMENT_METAOBJECT(Equipment)


Equipment::Equipment() {
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Equipment::Equipment(const Equipment &other)
 : Core::BaseObject() {
	*this = other;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Equipment::~Equipment() {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Equipment::operator==(const Equipment &rhs) const {
	if ( !(_type == rhs._type) )
		return false;
	if ( !(_description == rhs._description) )
		return false;
	if ( !(_manufacturer == rhs._manufacturer) )
		return false;
	if ( !(_vendor == rhs._vendor) )
		return false;
	if ( !(_model == rhs._model) )
		return false;
	if ( !(_serialNumber == rhs._serialNumber) )
		return false;
	if ( !(_installationDate == rhs._installationDate) )
		return false;
	if ( !(_removalDate == rhs._removalDate) )
		return false;
	if ( !(_resourceId == rhs._resourceId) )
		return false;
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Equipment::setType(const std::string& type) {
	_type = type;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& Equipment::type() const {
	return _type;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Equipment::setDescription(const std::string& description) {
	_description = description;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& Equipment::description() const {
	return _description;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Equipment::setManufacturer(const std::string& manufacturer) {
	_manufacturer = manufacturer;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& Equipment::manufacturer() const {
	return _manufacturer;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Equipment::setVendor(const std::string& vendor) {
	_vendor = vendor;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& Equipment::vendor() const {
	return _vendor;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Equipment::setModel(const std::string& model) {
	_model = model;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& Equipment::model() const {
	return _model;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Equipment::setSerialNumber(const std::string& serialNumber) {
	_serialNumber = serialNumber;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& Equipment::serialNumber() const {
	return _serialNumber;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Equipment::setInstallationDate(const OPT(DateTime)& installationDate) {
	_installationDate = installationDate;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DateTime Equipment::installationDate() const {
	if ( _installationDate )
		return *_installationDate;
	throw Seiscomp::Core::ValueException("Equipment.InstallationDate is not set");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Equipment::setRemovalDate(const OPT(DateTime)& removalDate) {
	_removalDate = removalDate;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DateTime Equipment::removalDate() const {
	if ( _removalDate )
		return *_removalDate;
	throw Seiscomp::Core::ValueException("Equipment.RemovalDate is not set");
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Equipment::setResourceId(const std::string& resourceId) {
	_resourceId = resourceId;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const std::string& Equipment::resourceId() const {
	return _resourceId;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Equipment& Equipment::operator=(const Equipment &other) {
	_type = other._type;
	_description = other._description;
	_manufacturer = other._manufacturer;
	_vendor = other._vendor;
	_model = other._model;
	_serialNumber = other._serialNumber;
	_installationDate = other._installationDate;
	_removalDate = other._removalDate;
	_resourceId = other._resourceId;
	_calibrationDates = other._calibrationDates;
	_identifiers = other._identifiers;
	return *this;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t Equipment::calibrationDateCount() const {
	return _calibrationDates.size();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
DateType* Equipment::calibrationDate(size_t i) const {
	return _calibrationDates[i].get();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Equipment::addCalibrationDate(DateType *obj) {
	if ( obj == NULL )
		return false;

	// Add the element
	_calibrationDates.push_back(obj);
	
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Equipment::removeCalibrationDate(DateType *obj) {
	if ( obj == NULL )
		return false;

	std::vector<DateTypePtr>::iterator it;
	it = std::find(_calibrationDates.begin(), _calibrationDates.end(), obj);
	// Element has not been found
	if ( it == _calibrationDates.end() ) {
		SEISCOMP_ERROR("Equipment::removeCalibrationDate(DateType*) -> child object has not been found although the parent pointer matches???");
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Equipment::removeCalibrationDate(size_t i) {
	// index out of bounds
	if ( i >= _calibrationDates.size() )
		return false;

	_calibrationDates.erase(_calibrationDates.begin() + i);

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
size_t Equipment::identifierCount() const {
	return _identifiers.size();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Identifier* Equipment::identifier(size_t i) const {
	return _identifiers[i].get();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Equipment::addIdentifier(Identifier *obj) {
	if ( obj == NULL )
		return false;

	// Add the element
	_identifiers.push_back(obj);
	
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Equipment::removeIdentifier(Identifier *obj) {
	if ( obj == NULL )
		return false;

	std::vector<IdentifierPtr>::iterator it;
	it = std::find(_identifiers.begin(), _identifiers.end(), obj);
	// Element has not been found
	if ( it == _identifiers.end() ) {
		SEISCOMP_ERROR("Equipment::removeIdentifier(Identifier*) -> child object has not been found although the parent pointer matches???");
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Equipment::removeIdentifier(size_t i) {
	// index out of bounds
	if ( i >= _identifiers.size() )
		return false;

	_identifiers.erase(_identifiers.begin() + i);

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
