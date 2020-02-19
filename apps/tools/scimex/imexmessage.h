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



#ifndef SEISCOMP_APPLICATIONS_IMEXMESSAGE_H__
#define SEISCOMP_APPLICATIONS_IMEXMESSAGE_H__

#include <seiscomp/core/baseobject.h>
#include <seiscomp/core/message.h>
#include <seiscomp/datamodel/notifier.h>


namespace Seiscomp {
namespace Applications {

DEFINE_SMARTPOINTER(IMEXMessage);

class IMEXMessage : public Core::Message {
	DECLARE_SC_CLASS(IMEXMessage);
	
	// ----------------------------------------------------------------------
	//  Xstruction
	// ----------------------------------------------------------------------
	public:
		//! Constructor
		IMEXMessage() {}


	// ----------------------------------------------------------------------
	//  Public Interface
	// ----------------------------------------------------------------------
	public:
		/**
		 * @return Returns the number of objects attached to a message
		 */
		virtual int size() const;
	
		/**
		 * Checks whether a message is empty or not.
		 * @retval true The message is empty
		 * @retval false The message is not empty
		 */
		virtual bool empty() const;
	
		/**
		 * Erases the content of the message.
		 */
		virtual void clear();
	
		DataModel::NotifierMessage& notifierMessage();
		
		virtual void serialize(Archive& ar);
		
		
	// ----------------------------------------------------------------------
	// Private data members 
	// ----------------------------------------------------------------------
	private:
		DataModel::NotifierMessage _notifierMessage;
};

}
}

#endif
