#include "ToolServer.h"

#include <sstream>
#include <iostream>

namespace IOCP {
	ToolServer::ToolServer(std::string_view pipeName, ToolServer::TOnRequest onRequest)
		:PipeName(pipeName), Thread([this]() { Run(); })
	{
		Listeners.reserve(10);
		AcceptNextAsync();
	}

	void ToolServer::Stop()
	{
		Context.stop();
	}

	void ToolServer::SendToClients(std::string_view message)
	{
		for (auto& listener : Listeners) {
			listener.WriteAsync(message);
		}
	}

	void ToolServer::AcceptNextAsync()
	{
		Listeners.emplace_back(PipeName, *this);
		Listeners.back().AcceptAsync([this]()
			{
				Listeners.back().ReadAsync();
				// async_accept_next();
			});
	}

	void ToolServer::Run()
	{
		while (true)
		{
			Context.run();
		}
	}


	void ToolServer::OnRequest(const std::string& message)
	{
		if (OnRequestFn) {
			std::stringstream stream{};
			OnRequestFn(message, stream);
		}
	}


	////////////////////////////////////////////////////////////////////////
	// ToolServer::Listener


	ToolServer::Listener::Listener(const std::string& name, ToolServer& server)
		:Pipe(name, server.Context), Server(server)
	{
		Buffer.reserve(1024);
	}

	void ToolServer::Listener::AcceptAsync(std::function<void()> on_complete)
	{
		Pipe.async_accept([on_complete](const std::error_code& ec, size_t)
			{
				if (ec)
					return;
				on_complete();
			});
	}

	void ToolServer::Listener::ReadAsync()
	{
		Pipe.async_read(Buffer.data(), Buffer.capacity(), [this](const std::error_code& ec, size_t len)
			{
				if (ec)
					return;
				std::cout << "read: " << len << " bytes" << std::endl;
				std::string message(reinterpret_cast<char*>(Buffer.data()), len);
				Server.OnRequest(message);
				ReadAsync();
			});
	}

	void ToolServer::Listener::WriteAsync(std::string_view message)
	{
		auto* msg = reinterpret_cast<const byte*>(message.data());
		auto onFinished = [](std::error_code, size_t) {};
		Pipe.async_write(msg, message.size(), onFinished);
	} 
}