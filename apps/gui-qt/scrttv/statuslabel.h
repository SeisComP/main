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


#ifndef SEISCOMP_GUI_SCRTTV_STATUSLABEL
#define SEISCOMP_GUI_SCRTTV_STATUSLABEL


#include <QLabel>


namespace Seiscomp {
namespace Applications {
namespace TraceView {


class StatusLabel : public QLabel {
	Q_OBJECT

	public:
		enum Level {
			Success,
			Info,
			Warning,
			Error
		};

	Q_PROPERTY(Level level READ level WRITE setLevel)

	public:
		StatusLabel(QWidget *parent = 0);

		Level level() const;
		void setLevel(Level);

	protected:
		void paintEvent(QPaintEvent *) override;

	private:
		Level _level{Info};
};


inline StatusLabel::Level StatusLabel::level() const {
	return _level;
}


}
}
}


#endif
