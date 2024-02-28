#pragma once

#include <iosfwd>
#include <functional>
#include <string>

using OnMessageFunc = std::function<bool(const std::string&, std::ostream&)>;

namespace Np2 {
	void StartServer(std::string_view pipeName, OnMessageFunc onRequest);
	void StopServer();
	void StartClient(std::string_view pipeName, OnMessageFunc onResponse);
	void StopClient();

	void Write(std::string_view message);
}