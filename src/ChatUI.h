#pragma once

#include "PrismaUI_F4_API.h"
#include <string>

namespace FalloutChat
{
	namespace ChatUI
	{
		void Initialize();
		void CreateView();
		
		void ToggleChat();
		bool IsChatOpen();
		
		// Called by ChatClient when new messages arrive
		void OnMessagesReceived();
	}
}
