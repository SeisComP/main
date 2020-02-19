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



#include "imexmessage.h"

namespace Seiscomp {
namespace Applications {

IMPLEMENT_SC_CLASS_DERIVED(IMEXMessage, Core::Message, "IMEXMessage");

int IMEXMessage::size() const
{
	return _notifierMessage.size();
}




bool IMEXMessage::empty() const
{
	return _notifierMessage.empty();
}




void IMEXMessage::clear()
{
	return _notifierMessage.clear();
}




DataModel::NotifierMessage& IMEXMessage::notifierMessage()
{
	return _notifierMessage;
}




void IMEXMessage::serialize(Archive& ar)
{
	ar & TAGGED_MEMBER(notifierMessage);
}


} // namespace Applications
} // namespace Seiscomp
