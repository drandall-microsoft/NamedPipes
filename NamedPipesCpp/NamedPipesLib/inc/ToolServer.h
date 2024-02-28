#pragma once

#include <functional>
#include <string>
#include <thread>

#include "iocp.h"

namespace IOCP {
	struct ToolServer {
		using TOnRequest = std::function<bool(const std::string&, std::ostream&)>;

		ToolServer(std::string_view pipeName, TOnRequest onRequest);

		void Stop();
		void SendToClients(std::string_view message);
		void OnRequest(const std::string& message);

		beam::iocp Context{};

	private:

		void Run();
		void AcceptNextAsync();

		struct Listener {
			Listener(const std::string& name, ToolServer& server);

			void AcceptAsync(std::function<void()> onAccept);
			void ReadAsync();
			void WriteAsync(std::string_view message);

			beam::pipe Pipe;
			ToolServer& Server;
			std::vector<byte> Buffer{};
		};

		std::string PipeName;
		std::vector<Listener> Listeners;
		std::thread Thread;
		TOnRequest OnRequestFn;
	};
}
