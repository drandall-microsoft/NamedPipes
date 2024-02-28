#include <tool_server.h>

#include <sstream>

tool_server::tool_server(const std::string& pipe_name, tool_server::message_fn on_incoming_message)
	:pipe_name_(pipe_name), runner_([this]() { run(); })
{
	listeners_.reserve(10);
	async_accept_next();
}

void tool_server::stop()
{
	context_.stop();
}

void tool_server::publish_message(const std::string& message)
{

}

void tool_server::async_accept_next()
{
	listeners_.emplace_back(pipe_name_, *this);
	listeners_.back().async_accept([this]()
	{
		listeners_.back().async_read();
		// async_accept_next();
	});
}

void tool_server::run()
{
	while (1)
	{
		context_.run();
	}
}


void tool_server::on_incoming_message(const std::string& message)
{
	if (on_incoming_message_) {
		std::stringstream stream{};
		on_incoming_message_(message, stream);
	}
}


////////////////////////////////////////////////////////////////////////
// tool_server::tool_listener


tool_server::tool_listener::tool_listener(const std::string& name, tool_server& server)
	:server_pipe_(name, server.context()), server_(server)
{
	buffer_.reserve(1024);
}

void tool_server::tool_listener::async_accept(std::function<void()> on_complete)
{
	server_pipe_.async_accept([on_complete](const std::error_code& ec, size_t)
	{
		if (ec)
			return;
		on_complete();
	});
}

void tool_server::tool_listener::async_read()
{
	server_pipe_.async_read(buffer_.data(), buffer_.capacity(), [this](const std::error_code& ec, size_t len)
	{
		if (ec)
			return;
		std::cout << "read: " << len << " bytes; error_code: " << ec.value() << std::endl;
		std::string message(reinterpret_cast<char*>(buffer_.data()), len);
		server_.on_incoming_message(message);
		async_read();
	});
}