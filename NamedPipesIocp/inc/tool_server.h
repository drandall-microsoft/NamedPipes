#pragma once

#include <array>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>


#include <iocp.h>

class tool_server
{
	using message_fn = std::function<bool(const std::string&, std::ostream&)> ;
	class tool_listener
	{
	public:
		tool_listener(const std::string& name, tool_server &server);

		void async_accept(std::function<void()> on_accept);
		void async_read();

	private:
		beam::pipe server_pipe_;
		tool_server& server_;
		std::vector<byte> buffer_{};
	};

public:

	tool_server(const std::string& pipe_name, message_fn on_incoming_message);
	void stop();
	void publish_message(const std::string& message);

	void on_incoming_message(const std::string &message);

	beam::iocp& context() 
	{
		return context_;
	}

private:

	void run();
	void async_accept_next();

private:

	beam::iocp context_{};
	const std::string pipe_name_;
	std::vector<tool_listener> listeners_;
	std::thread runner_;
	message_fn on_incoming_message_;
};