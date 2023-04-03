/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
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


#include <seiscomp/datamodel/eventparameters.h>
#include <seiscomp/datamodel/utils.h>
#include <seiscomp/seismology/regions.h>
#include <seiscomp/gui/core/application.h>
#include <seiscomp/gui/core/flowlayout.h>
#include <seiscomp/gui/core/inspector.h>
#include <seiscomp/gui/core/utils.h>
#include <seiscomp/gui/datamodel/locatorsettings.h>
#include <seiscomp/gui/datamodel/originlocatormap.h>

#include <QDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <set>

#include "associator.h"
#include "settings.h"
#include "tracemarker.h"


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::DataModel;
using namespace Seiscomp::Gui;


namespace Seiscomp {
namespace Applications {
namespace TraceView {


namespace {


QString waveformIDToString(const WaveformStreamID& id) {
	return (id.networkCode() + "." + id.stationCode() + "." +
	        id.locationCode() + "." + id.channelCode()).c_str();
}


class FlexLayout : public Seiscomp::Gui::FlowLayout {
	public:
		FlexLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1)
		: FlowLayout(margin, hSpacing, vSpacing) {}

	protected:
		int doLayout(const QRect &rect, bool testOnly) const override {
			int spaceX = qMax(0, horizontalSpacing());
			int spaceY = qMax(0, verticalSpacing());

			const int x0 = rect.left() + contentsMargins().left();
			const int x1 = rect.width() - contentsMargins().right();

			int x = x0;
			int y = rect.top() + contentsMargins().top();

			int lineHeight = 0;

			for ( auto item : _itemList ) {
				int itemWidth = item->sizeHint().width();
				int itemHeight;

				if ( item->hasHeightForWidth() ) {
					itemHeight = item->heightForWidth(itemWidth);
				}
				else {
					itemHeight = item->sizeHint().height();
				}

				int nx = x + itemWidth;
				if ( nx > x1 ) {
					y += lineHeight + spaceY;

					lineHeight = itemHeight;
					x = x0;
					nx = x + itemWidth;
				}
				else {
					lineHeight = qMax(lineHeight, itemHeight);
				}

				if ( !testOnly ) {
					item->setGeometry(
						QRect(
							QPoint(x, y),
							QSize(itemWidth, itemHeight)
						)
					);
				}

				if ( lineHeight ) {
					x = nx + spaceX;
				}
			}

			return y + lineHeight + contentsMargins().bottom();
		}
};


class Badge : public QFrame {
	public:
		Badge(TraceMarker *marker, QMap<QString, QWidget*> &badges) {
			setLayout(new QHBoxLayout);
			layout()->setMargin(0);
			setFrameShape(QFrame::StyledPanel);
			setFrameShadow(QFrame::Raised);

			auto content = new QWidget;
			layout()->addWidget(content);

			content->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
			content->setLayout(new QHBoxLayout);
			content->layout()->setMargin(0);

			QString labelText = waveformIDToString(marker->pick->waveformID());
			try {
				labelText += QString("- %1").arg(marker->pick->phaseHint().code().c_str());
			}
			catch ( ... ) {}

			if ( badges.contains(labelText) ) {
				auto pal = content->palette();
				pal.setColor(content->backgroundRole(), QColor(0xff, 0xfa, 0xf3));
				pal.setColor(content->foregroundRole(), QColor(0x57, 0x3a, 0x08));
				content->setPalette(pal);
			}
			else {
				content->setBackgroundRole(QPalette::Mid);
				content->setForegroundRole(QPalette::ButtonText);
				badges[labelText] = this;
			}

			content->setAutoFillBackground(true);
			content->setToolTip(marker->toolTip());
			content->setProperty("pickID", QString(marker->pick->publicID().c_str()));

			_colorLabel = new QLabel;
			_colorLabel->setBackgroundRole(QPalette::Window);
			_colorLabel->setAutoFillBackground(true);
			_colorLabel->setFixedWidth(4);
			content->layout()->addWidget(_colorLabel);

			setColor(marker->color());

			auto label = new QLabel(labelText);
			label->setForegroundRole(content->foregroundRole());
			content->layout()->addWidget(label);

			_closeButton = new QPushButton();
			_closeButton->setIcon(QIcon(":/icons/icons/remove.png"));
			_closeButton->setFlat(true);

			content->layout()->addWidget(_closeButton);
		}

		void setColor(QColor color) {
			auto pal = _colorLabel->palette();
			pal.setColor(QPalette::Window, color);
			_colorLabel->setPalette(pal);
		}

		QPushButton *buttonClose() const {
			return _closeButton;
		}

	private:
		QLabel      *_colorLabel;
		QPushButton *_closeButton;
};


}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Associator::Associator(QWidget *parent) : QWidget(parent) {
	_ui.setupUi(this);
	setupOriginInfo();
	unsetMessage();

	_ui.framePicks->setLayout(new QVBoxLayout);
	_ui.framePicks->layout()->setMargin(0);

	QScrollArea *area = new QScrollArea();
	_ui.framePicks->layout()->addWidget(area);

	_pickContainer = new QWidget(area);
	_pickContainer->setLayout(new FlexLayout);

	area->setWidget(_pickContainer);
	area->setWidgetResizable(true);

	_ui.frameMap->setLayout(new QVBoxLayout);
	_mapWidget = new OriginLocatorMap(SCApp->mapsDesc(), _ui.frameMap);
	auto sizePolicy = _mapWidget->sizePolicy();
	sizePolicy.setHeightForWidth(true);
	_mapWidget->setSizePolicy(sizePolicy);
	// _mapWidget->setDrawStationAnnotations(true);
	_ui.frameMap->layout()->setMargin(0);
	_ui.frameMap->layout()->addWidget(_mapWidget);

	connect(_mapWidget, SIGNAL(arrivalChanged(int, bool)),
	        this, SLOT(arrivalChanged(int, bool)));

	auto locators = Seismology::LocatorInterfaceFactory::Services();
	if ( locators ) {
		for ( auto locator : *locators ) {
			_ui.cbLocator->addItem(locator.c_str());
		}
		delete locators;
		if ( _ui.cbLocator->count() > 0 ) {
			_ui.cbLocator->setEnabled(true);

			int defaultIndex = _ui.cbLocator->findText(Settings::global.defaultLocator.c_str());
			_ui.cbLocator->setCurrentIndex(defaultIndex >= 0 ? defaultIndex: 0);

			connect(_ui.cbLocatorProfile, SIGNAL(currentTextChanged(QString)),
			        this, SLOT(profileChanged(QString)));

			locatorChanged(_ui.cbLocator->currentText());
		}
	}

	for ( auto depth : Settings::global.depths ) {
		_ui.cbDepth->addItem(QString("%1").arg(depth, 0, 'f', SCScheme.precision.depth));
	}
	_ui.cbDepth->clearEditText();

	connect(_ui.cbLocator, SIGNAL(currentTextChanged(QString)),
	        this, SLOT(locatorChanged(QString)));

	connect(_ui.btnLocatorSettings, SIGNAL(clicked()),
	        this, SLOT(configureLocator()));

	connect(_ui.btnShowOrigin, SIGNAL(clicked()), this, SLOT(showOrigin()));
	connect(_ui.btnInspect, SIGNAL(clicked()), this, SLOT(inspect()));
	connect(_ui.btnCommit, SIGNAL(clicked()), this, SLOT(commit()));

	connect(_ui.cbDepth, SIGNAL(currentTextChanged(QString)),
	        this, SLOT(relocate()));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Associator::push(const QVector<TraceMarker *> markers,
                      Seiscomp::Gui::RecordView::SelectionOperation op) {
	bool update = false;

	unsetMessage();

	if ( op == RecordView::Select ) {
		for ( auto marker : _markers ) {
			if ( marker.first->setSelected(false) ) {
				update = true;
			}
		}

		_markers.clear();
		for ( auto marker : markers ) {
			_markers.append(MarkerBadge(marker, nullptr));
		}

		for ( auto marker : _markers ) {
			if ( marker.first->setSelected(true) ) {
				update = true;
			}
		}
	}
	else if ( op == RecordView::SelectPlus ) {
		for ( auto marker : markers ) {
			auto it = std::find_if(_markers.begin(), _markers.end(), [marker](MarkerBadge badge) {
				return badge.first == marker;
			});
			if ( it == _markers.end() ) {
				if ( marker->setSelected(true) ) {
					update = true;
				}
				_markers.push_back(MarkerBadge(marker, nullptr));
			}
		}
	}
	else if ( op == RecordView::SelectMinus ) {
		for ( auto marker : markers ) {
			auto it = std::find_if(_markers.begin(), _markers.end(), [marker](MarkerBadge badge) {
				return badge.first == marker;
			});
			if ( it != _markers.end() ) {
				if ( it->first->setSelected(false) ) {
					update = true;
				}
				_markers.erase(it);
			}
		}
	}

	syncPicksView();

	// Relocate
	relocate();

	return update;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool Associator::updatedMarker(TraceMarker *marker) {
	for ( int i = 0; i < _markers.count(); ++i ) {
		if ( _markers[i].first == marker ) {
			if ( _markers[i].second ) {
				static_cast<Badge*>(_markers[i].second)->setColor(marker->color());
			}
			return true;
		}
	}

	return false;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::syncPicksView() {
	// Clear pick container
	while ( auto w = _pickContainer->findChild<QWidget*>() ) {
		delete w;
	}

	QMap<QString, QWidget*> badges;

	// Populate pick container
	for ( auto &marker : _markers ) {
		marker.second = new Badge(marker.first, badges);
		connect(static_cast<Badge*>(marker.second)->buttonClose(),
		        SIGNAL(clicked()), this, SLOT(removePick()));
		_pickContainer->layout()->addWidget(marker.second);
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::unsetMessage() {
	_ui.labelMessage->setVisible(false);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::setMessage(const QString &message, StatusLabel::Level level) {
	_ui.labelMessage->setLevel(level);
	_ui.labelMessage->setText(message);
	_ui.labelMessage->setVisible(true);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::setSuccess(const QString &message) {
	setMessage(message, StatusLabel::Success);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::setInfo(const QString &message) {
	setMessage(message, StatusLabel::Info);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::setWarning(const QString &message) {
	setMessage(message, StatusLabel::Warning);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::setError(const QString &message) {
	setMessage(message, StatusLabel::Error);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::setupOriginInfo() {
	_ui.label_15->setFont(SCScheme.fonts.normal);
	_ui.label_12->setFont(SCScheme.fonts.normal);
	_ui.label_10->setFont(SCScheme.fonts.normal);
	_ui.label_11->setFont(SCScheme.fonts.normal);
	_ui.label_8->setFont(SCScheme.fonts.normal);
	_ui.label_13->setFont(SCScheme.fonts.normal);
	_ui.label_7->setFont(SCScheme.fonts.normal);
	_ui.label_9->setFont(SCScheme.fonts.normal);
	_ui.labelTime->setFont(SCScheme.fonts.highlight);
	_ui.labelDepth->setFont(SCScheme.fonts.highlight);
	_ui.labelDepthUnit->setFont(SCScheme.fonts.normal);
	_ui.labelDepthError->setFont(SCScheme.fonts.normal);
	_ui.labelDepthErrorUnit->setFont(SCScheme.fonts.normal);
	_ui.labelLatitude->setFont(SCScheme.fonts.highlight);
	_ui.labelLatitudeUnit->setFont(SCScheme.fonts.normal);
	_ui.labelLatitudeError->setFont(SCScheme.fonts.normal);
	_ui.labelLatitudeErrorUnit->setFont(SCScheme.fonts.normal);
	_ui.labelLongitude->setFont(SCScheme.fonts.highlight);
	_ui.labelLongitudeUnit->setFont(SCScheme.fonts.normal);
	_ui.labelLongitudeError->setFont(SCScheme.fonts.normal);
	_ui.labelLongitudeErrorUnit->setFont(SCScheme.fonts.normal);
	_ui.labelNumPhases->setFont(SCScheme.fonts.highlight);
	_ui.labelNumPhasesUnit->setFont(SCScheme.fonts.normal);
	_ui.labelNumPhasesError->setFont(SCScheme.fonts.normal);
	_ui.labelStdError->setFont(SCScheme.fonts.normal);
	_ui.labelStdErrorUnit->setFont(SCScheme.fonts.normal);
	_ui.lbComment->setFont(SCScheme.fonts.normal);
	_ui.labelComment->setFont(SCScheme.fonts.normal);
	_ui.labelAzimuthGap->setFont(SCScheme.fonts.normal);
	_ui.labelAzimuthGapUnit->setFont(SCScheme.fonts.normal);
	_ui.labelMinDist->setFont(SCScheme.fonts.normal);
	_ui.labelMinDistUnit->setFont(SCScheme.fonts.normal);

	// For the time being ...
	_ui.lbComment->setVisible(false);
	_ui.labelComment->setVisible(false);

	_ui.lbEventID->setVisible(false);
	_ui.labelEventID->setVisible(false);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::updateOriginInfo() {
	if ( !_origin ) {
		_ui.labelTime->setText(QString());
		_ui.labelDepth->setText(tr("-"));
		_ui.labelDepthUnit->setText(tr("km"));
		_ui.labelDepthError->setText(QString());
		_ui.labelDepthErrorUnit->setText(QString());
		_ui.labelLatitude->setText(tr("-"));
		_ui.labelLatitude->setToolTip(QString());
		_ui.labelLatitudeUnit->setText(tr("\302\260"));
		_ui.labelLatitudeError->setText(tr("-"));
		_ui.labelLatitudeErrorUnit->setText(tr("km"));
		_ui.labelLongitude->setText(tr("-"));
		_ui.labelLongitude->setToolTip(QString());
		_ui.labelLongitudeUnit->setText(tr("\302\260"));
		_ui.labelLongitudeError->setText(tr("-"));
		_ui.labelLongitudeErrorUnit->setText(tr("km"));
		_ui.labelNumPhases->setText(tr("-"));
		_ui.labelNumPhasesUnit->setText(tr("/"));
		_ui.labelNumPhasesError->setText(tr("-"));
		_ui.labelStdError->setText("-");
		_ui.labelStdErrorUnit->setText(tr("s"));
		_ui.labelComment->setText(tr("-"));
		_ui.labelAzimuthGap->setText(tr("-"));
		_ui.labelAzimuthGapUnit->setText(tr("\302\260"));
		_ui.labelMinDist->setText(tr("-"));
		_ui.labelMinDistUnit->setText(tr("\302\260"));
		_ui.labelModified->setText(QString());
		_ui.labelAgency->setText(QString());
		_ui.labelAgency->setToolTip(QString());
		_ui.labelUser->setText(QString());
		_ui.labelUser->setToolTip(QString());
		_ui.labelEvaluation->setPalette(palette());
		_ui.labelEvaluation->setText(tr("- (-)"));
		_ui.labelMethod->setText(QString());
		_ui.labelEarthModel->setText(QString());

		return;
	}

	std::string format = "%Y-%m-%d %H:%M:%S";
	if ( SCScheme.precision.originTime > 0 ) {
		format += ".%";
		format += Core::toString(SCScheme.precision.originTime);
		format += "f";
	}

	timeToLabel(_ui.labelTime, _origin->time(), format.c_str());

	_ui.labelLatitude->setText(latitudeToString(_origin->latitude(), true, false, SCScheme.precision.location));
	_ui.labelLatitudeUnit->setText(latitudeToString(_origin->latitude(), false, true));
	//_ui.labelLatitudeUnit->setText("deg");
	try {
		_ui.labelLatitudeError->setText(QString("+/- %1").arg(quantityUncertainty(_origin->latitude()), 0, 'f', SCScheme.precision.uncertainties));
		_ui.labelLatitudeErrorUnit->setText("km");
	}
	catch ( Core::ValueException& ) {
		_ui.labelLatitudeError->setText("");
		_ui.labelLatitudeErrorUnit->setText("");
	}

	_ui.labelLongitude->setText(longitudeToString(_origin->longitude(), true, false, SCScheme.precision.location));
	_ui.labelLongitudeUnit->setText(longitudeToString(_origin->longitude(), false, true, SCScheme.precision.location));
	//_ui.labelLongitudeUnit->setText("deg");
	try {
		_ui.labelLongitudeError->setText(QString("+/- %1").arg(quantityUncertainty(_origin->longitude()), 0, 'f', SCScheme.precision.uncertainties));
		_ui.labelLongitudeErrorUnit->setText("km");
	}
	catch ( Core::ValueException& ) {
		_ui.labelLongitudeError->setText("");
		_ui.labelLongitudeErrorUnit->setText("");
	}

	_ui.labelLatitude->setToolTip(Regions::getRegionName(_origin->latitude(), _origin->longitude()).c_str());
	_ui.labelLongitude->setToolTip(_ui.labelLatitude->toolTip());

	try {
		_ui.labelDepth->setText(depthToString(_origin->depth(), SCScheme.precision.depth));
		_ui.labelDepthUnit->setText("km");
	}
	catch ( Core::ValueException& ) {
		_ui.labelDepth->setText("-");
	}

	try {
		_ui.labelDepth->setToolTip(tr("Type: %1").arg(_origin->depthType().toString()));
	}
	catch ( Core::ValueException& ) {
		_ui.labelDepth->setToolTip(tr("Type: unset"));
	}

	try {
		double err_z = quantityUncertainty(_origin->depth());
		if ( err_z == 0.0 ) {
			_ui.labelDepthError->setText(QString("fixed"));
			_ui.labelDepthErrorUnit->setText("");
		}
		else {
			_ui.labelDepthError->setText(QString("+/- %1").arg(err_z, 0, 'f', SCScheme.precision.uncertainties));
			_ui.labelDepthErrorUnit->setText("km");
		}
	}
	catch ( Core::ValueException& ) {
		_ui.labelDepthError->setText(QString("fixed"));
		_ui.labelDepthErrorUnit->setText("");
	}

	try {
		_ui.labelStdError->setText(QString("%1").arg(_origin->quality().standardError(), 0, 'f', SCScheme.precision.rms));
	}
	catch ( Core::ValueException& ) {
		_ui.labelStdError->setText("-");
	}

	try {
		_ui.labelAzimuthGap->setText(QString("%1").arg(_origin->quality().azimuthalGap(), 0, 'f', 0));
		//_ui.labelAzimuthGapUnit->setText("deg");
	}
	catch ( Core::ValueException& ) {
		_ui.labelAzimuthGap->setText("-");
	}

	try {
		if ( SCScheme.unit.distanceInKM )
			_ui.labelMinDist->setText(QString("%1").arg(Math::Geo::deg2km(_origin->quality().minimumDistance()), 0, 'f', SCScheme.precision.distance));
		else
			_ui.labelMinDist->setText(QString("%1").arg(_origin->quality().minimumDistance(), 0, 'f', 1));
		//_ui.labelMinDistUnit->setText("deg");
	}
	catch ( Core::ValueException& ) {
		_ui.labelMinDist->setText("-");
	}

	try {
		try {
			timeToLabel(_ui.labelModified, _origin->creationInfo().modificationTime(), "%Y-%m-%d %H:%M:%S");
			try {
				_ui.labelModified->setToolTip(tr("Creation time: %1").arg(timeToString(_origin->creationInfo().creationTime(), "%Y-%m-%d %H:%M:%S")));
			}
			catch ( ... ) {}
		}
		catch ( ... ) {
			timeToLabel(_ui.labelModified, _origin->creationInfo().creationTime(), "%Y-%m-%d %H:%M:%S");
			_ui.labelModified->setToolTip(tr("That is actually the creation time"));
		}
	}
	catch ( Core::ValueException& ) {
		_ui.labelModified->setText(QString());
	}

	try {
		_ui.labelAgency->setText(_origin->creationInfo().agencyID().c_str());
		_ui.labelAgency->setToolTip(_origin->creationInfo().agencyID().c_str());
	}
	catch ( Core::ValueException & ) {
		_ui.labelAgency->setText(QString());
		_ui.labelAgency->setToolTip(QString());
	}

	try {
		_ui.labelUser->setText(_origin->creationInfo().author().c_str());
		_ui.labelUser->setToolTip(_origin->creationInfo().author().c_str());
	}
	catch ( Core::ValueException & ) {
		_ui.labelUser->setText(QString());
		_ui.labelUser->setToolTip(QString());
	}

	QPalette pal = _ui.labelEvaluation->palette();
	pal.setColor(QPalette::WindowText, palette().color(QPalette::WindowText));
	_ui.labelEvaluation->setPalette(pal);

	QString evalMode;
	try {
		evalMode = _origin->evaluationStatus().toString();
		if ( _origin->evaluationStatus() == REJECTED ) {
			QPalette pal = _ui.labelEvaluation->palette();
			pal.setColor(QPalette::WindowText, Qt::red);
			_ui.labelEvaluation->setPalette(pal);
		}
	}
	catch ( ... ) {
		evalMode = "-";
	}

	try {
		if ( _origin->evaluationMode() == AUTOMATIC ) {
			evalMode += " (A)";
		}
		else if ( _origin->evaluationMode() == MANUAL ) {
			evalMode += " (M)";
		}
		else {
			evalMode += " (-)";
		}
	}
	catch ( ... ) {
		evalMode += " (-)";
	}

	_ui.labelEvaluation->setText(evalMode);
	_ui.labelMethod->setText(_origin->methodID().c_str());
	_ui.labelEarthModel->setText(_origin->earthModelID().c_str());

	int activeArrivals = 0;
	for ( size_t i = 0; i < _origin->arrivalCount(); ++i ) {
		int flags = Seismology::arrivalToFlags(_origin->arrival(i));
		if ( flags == Seismology::LocatorInterface::F_NONE ) {
			// If the timeUsed attribute is not set then it looks like an origin
			// created with an older version. So use the weight value to decide
			// whether the pick is active or not.
			double weight = 0.0;
			try {
				weight = abs(_origin->arrival(i)->weight());
			}
			catch ( ... ) {}

			if ( weight >= 1E-6 ) {
				++activeArrivals;
			}
		}
		else {
			++activeArrivals;
		}
	}

	_ui.labelNumPhases->setText(QString("%1").arg(activeArrivals));
	_ui.labelNumPhasesError->setText(QString("%1").arg(_origin->arrivalCount()));
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::locatorChanged(QString locator) {
	_ui.cbLocatorProfile->clear();
	_locator = Seismology::LocatorInterface::Create(qPrintable(locator));

	_ui.btnLocatorSettings->setEnabled(_locator != nullptr);

	if ( !_locator ) {
		_ui.cbLocatorProfile->setEnabled(false);
		setError(tr("Failed to create locator %1").arg(locator));
		return;
	}

	if ( !_locator->init(SCApp->configuration()) ) {
		_ui.cbLocatorProfile->setEnabled(false);
		setError(tr("%1 failed to initialize").arg(locator));
		return;
	}

	set<string> models;
	auto profiles = _locator->profiles();
	for ( auto it = profiles.begin(); it != profiles.end(); ++it ) {
		if ( models.find(*it) != models.end() ) {
			continue;
		}

		_ui.cbLocatorProfile->addItem(it->c_str());
	}

	int defaultIndex = _ui.cbLocatorProfile->findText(Settings::global.defaultEarthModel.c_str());
	if ( defaultIndex >= 0 ) {
		_ui.cbLocatorProfile->setCurrentIndex(defaultIndex);
	}
	else {
		_ui.cbLocatorProfile->setCurrentIndex(0);
	}

	_ui.cbLocatorProfile->setEnabled(_ui.cbLocatorProfile->count() > 0);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::profileChanged(QString profile) {
	if ( !_locator ) {
		setError(tr("No locator active"));
		return;
	}

	unsetMessage();
	_locator->setProfile(profile.toStdString());
	relocate();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::arrivalChanged(int id, bool state) {
	if ( state ) {
		return;
	}

	auto arrival = _origin->arrival(id);

	for ( auto it = _markers.begin(); it != _markers.end(); ++it ) {
		auto marker = *it;
		if ( marker.first->pick->publicID() == arrival->pickID() ) {
			_markers.erase(it);
			syncPicksView();
			relocate();
			break;
		}
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::configureLocator() {
	if ( !_locator ) {
		QMessageBox::critical(this, tr("Locator settings"),
		                      tr("No locator selected."));
		return;
	}

	Seismology::LocatorInterface::IDList params = _locator->parameters();
	if ( params.empty() ) {
		QMessageBox::information(this, tr("Locator settings"),
		                         tr("%1 does not provide any "
		                            "parameters to adjust.")
		                         .arg(_locator->name().c_str()));
		return;
	}

	LocatorSettings dlg;
	dlg.setWindowTitle(tr("%1 settings").arg(_locator->name().c_str()));

	for ( Seismology::LocatorInterface::IDList::iterator it = params.begin();
	      it != params.end(); ++it ) {
		dlg.addRow(it->c_str(), _locator->parameter(*it).c_str());
	}

	if ( dlg.exec() != QDialog::Accepted ) {
		return;
	}

	auto res = dlg.content();
	for ( auto it = res.begin(); it != res.end(); ++it ) {
		_locator->setParameter(it->first.toStdString(), it->second.toStdString());
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::removePick() {
	QPushButton *btnClose = static_cast<QPushButton*>(sender());
	QWidget *badge = btnClose->parentWidget();
	auto pickID = badge->property("pickID").toString().toStdString();
	auto it = find_if(_markers.begin(), _markers.end(), [&pickID](MarkerBadge badge) {
		return badge.first->pick->publicID() == pickID;
	});

	if ( it != _markers.end() ) {
		(*it).first->setSelected(false);
		(*it).first->parent()->update();
		delete (*it).second;
		_markers.erase(it);
		relocate();
	}
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::relocate() {
	_mapWidget->setOrigin(nullptr);
	_ui.btnCommit->setEnabled(false);
	_ui.btnShowOrigin->setEnabled(false);
	_ui.btnInspect->setEnabled(false);

	bool isNumber = false;
	double depth = -1;
	if ( !_ui.cbDepth->currentText().isEmpty() ) {
		depth = _ui.cbDepth->currentText().toDouble(&isNumber);
		if ( !isNumber ) {
			_origin = nullptr;
			setError(QString("Invalid depth: %1").arg(_ui.cbDepth->currentText()));
			updateOriginInfo();
			return;
		}
	}

	if ( !_markers.empty() ) {
		qApp->setOverrideCursor(Qt::BusyCursor);
		try {
			Seismology::LocatorInterface::PickList picks;
			for ( auto marker : _markers ) {
				picks.push_back(
					Seismology::LocatorInterface::PickItem(
						marker.first->pick.get()
					)
				);
			}

			sort(picks.begin(), picks.end(), [](const Seismology::LocatorInterface::PickItem &first,
			                                    const Seismology::LocatorInterface::PickItem &second) {
				return first.pick->time().value() < second.pick->time().value();
			});

			auto sloc = _locator->getSensorLocation(picks.front().pick.get());
			if ( !sloc ) {
				throw Core::GeneralException("station '" + picks.front().pick->waveformID().networkCode() +
				                             "." + picks.front().pick->waveformID().stationCode() + "' not found");
			}

			OriginPtr tmpOrigin = Origin::Create();
			tmpOrigin->setLatitude(sloc->latitude());
			tmpOrigin->setLongitude(sloc->longitude());
			tmpOrigin->setDepth(DataModel::RealQuantity(11.0));
			tmpOrigin->setTime(picks.front().pick->time());

			for ( auto item : picks ) {
				Arrival *arr = new Arrival;
				arr->setPickID(item.pick->publicID());
				arr->setTimeUsed(true);
				arr->setBackazimuthUsed(false);
				arr->setHorizontalSlownessUsed(false);
				arr->setWeight(1);
				try {
					arr->setPhase(item.pick->phaseHint());
				}
				catch ( ...) {
					arr->setPhase(Phase("P"));
				}

				auto loc = _locator->getSensorLocation(item.pick.get());
				if ( !loc ) {
					throw Core::GeneralException("station '" + item.pick->waveformID().networkCode() +
					                             "." + item.pick->waveformID().stationCode() + "' not found");
				}

				double dist, az, baz;
				Math::Geo::delazi(sloc->latitude(), sloc->longitude(),
				                  loc->latitude(), loc->longitude(),
				                  &dist, &az, &baz);

				arr->setDistance(dist);

				tmpOrigin->add(arr);
			}

			if ( isNumber ) {
				_locator->setFixedDepth(depth);
			}
			else {
				_locator->releaseDepth();
			}

			_origin = _locator->relocate(tmpOrigin.get());
			if ( !_origin ) {
				setError("Relocation failed for unknown reason");
			}
			else {
				_ui.btnCommit->setEnabled(!Settings::global.offline);
				_ui.btnShowOrigin->setEnabled(!Settings::global.offline);
				_ui.btnInspect->setEnabled(true);
				unsetMessage();
				touch(*_origin);
				_origin->setDepthType(isNumber ? OriginDepthType(OPERATOR_ASSIGNED) : OriginDepthType(FROM_LOCATION));
				_origin->creationInfo().setAgencyID(SCApp->agencyID());
				_origin->creationInfo().setAuthor(SCApp->author());
				_origin->setEvaluationMode(EvaluationMode(DataModel::MANUAL));
				_origin->setEvaluationStatus(EvaluationStatus(DataModel::PRELIMINARY));
				_mapWidget->setOrigin(_origin.get());

				double mapWidth = 10;
				try {
					mapWidth = _origin->quality().maximumDistance();
				}
				catch ( ... ) {}

				_mapWidget->canvas().displayRect(
					QRectF(
						_origin->longitude() - mapWidth * 0.5,
						_origin->latitude() - mapWidth * 0.5,
						mapWidth, mapWidth
					)
				);
			}
		}
		catch ( exception &e ) {
			_origin = nullptr;
			setError(e.what());
		}
		qApp->restoreOverrideCursor();
	}
	else {
		setInfo(tr("No picks added yet"));
		_origin = nullptr;
	}

	updateOriginInfo();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::showOrigin() {
	if ( !_origin ) {
		return;
	}

	SCApp->sendCommand(Gui::CM_OBSERVE_LOCATION, "", _origin.get());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::inspect() {
	if ( !_origin ) {
		return;
	}

	Inspector *w = new Inspector(this, Qt::Tool);
	w->setObject(_origin.get());

	static QByteArray state;

	QDialog dlg;
	dlg.restoreGeometry(state);
	dlg.setLayout(new QVBoxLayout);
	dlg.layout()->setMargin(0);
	dlg.layout()->addWidget(w);
	dlg.exec();
	state = dlg.saveGeometry();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void Associator::commit() {
	if ( !_origin ) {
		return;
	}

	// Set markers as associated
	for ( auto marker : _markers ) {
		if ( marker.first->setAssociated(true) ) {
			marker.first->update();
		}
	}

	// Update view to update the badges tooltip
	syncPicksView();

	{
		bool wasEnabled = Notifier::IsEnabled();
		Notifier::Enable();
		EventParameters tmp;
		tmp.add(_origin.get());
		Notifier::SetEnabled(wasEnabled);
	}

	NotifierMessagePtr msg = Notifier::GetMessage();

	if ( !SCApp->sendMessage(Settings::global.groupLocation.c_str(), msg.get()) ) {
		setError("Failed to send origin");
		return;
	}

	_ui.btnCommit->setEnabled(false);
	_ui.btnShowOrigin->setEnabled(!Settings::global.offline);
	_ui.btnInspect->setEnabled(true);

	setSuccess(
		tr("Origin %1 sent to group %2")
		.arg(_origin->publicID().c_str())
		.arg(Settings::global.groupLocation.c_str())
	);

	// Clear the cart
	push(QVector<TraceMarker*>(), RecordView::Select);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
}
