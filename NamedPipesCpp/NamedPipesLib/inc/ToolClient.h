#pragma once

#include "iocp.h"

#include <string_view>

struct ToolClient {
	ToolClient(std::string_view pipeName, std::function<void(const std::string&)> onRequest);
};

namespace IOCP {
	ToolClient& CreateClient(std::string_view pipeName, std::function<void(const std::string&)> onResponse);
	void SendToServer(std::string_view message);
}