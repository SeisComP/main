/***************************************************************************
 * Copyright (C) GFZ Potsdam                                               *
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




#include <QMainWindow>
#include <QStatusBar>
#include <QLabel>
#include <QString>


#include "progressbar.h"
#include "mainwindow.h"

/*
void
PickerMainWindow::setupStatusBar()
{
	statusBar();
	statusBar()->setSizeGripEnabled(false);

	statusBarFile   = new QLabel;
	statusBarFilter = new QLabel(" Filter OFF ");
	statusBarProg   = new MyProgressBar;

	statusBar()->addPermanentWidget(statusBarFilter, 1);
	statusBar()->addPermanentWidget(statusBarFile,   5);
	statusBar()->addPermanentWidget(statusBarProg,   1);
}

void
PickerMainWindow::statusBarUpdate()
{
	if (statusBarFilter)
		statusBarFilter->setText(filtering ?
					 preferences.filter.info() :
					 QString(" Filter OFF "));
	if (statusBarFile) {
		QString str;
		str.sprintf(" %d traces ", data.size());
		statusBarFile->setText(str);
	}
}

void
PickerMainWindow::statusBarShowProgress(float f)
{
	//  cerr<<i<<endl;
	//  statusBarProg->setProgress(i/100.);
	statusBarProg->setValue(f/100.);
	//  statusBarProg->update();
}
*/
