#include "ChatMessage.h"

namespace TelepathyIM
{
	ChatMessage::ChatMessage(ChatSessionParticipant* originator, const QTime& time_stamp, const QString &text) : originator_(originator), time_stamp_(time_stamp), text_(text)
	{

	}

	Communication::ChatSessionParticipantInterface* ChatMessage::GetOriginator() const
	{
		return originator_;
	}

	QTime ChatMessage::GetTimeStamp() const
	{
		return time_stamp_;
	}

	QString ChatMessage::GetText() const
	{
		return text_;
	}

} // end of namespace: TelepathyIM 
