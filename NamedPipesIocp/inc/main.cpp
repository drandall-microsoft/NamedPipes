#include <tool_server.h>
#include <system_error>
#include <vector>
#include <iostream>

using buffer = std::vector<byte>;



void async_write_loop(beam::pipe& pipe, const std::shared_ptr<buffer>& write_buf)
{
	pipe.async_write(write_buf->data(), 100, [&pipe, write_buf](const std::error_code& ec, size_t len)
	{
		if (ec)
			return;
		// hammer the client w/ writes.
		std::cout << "wrote: " << len << "bytes; error_code: " << ec.value() << std::endl;
		async_write_loop(pipe, write_buf);
	});
}

void do_server()
{
	tool_server server("\\\\.\\pipe\\beam", [](const std::string& message, std::ostream& err)
	{
		return false;
	});
	while (1);
}

void do_client(beam::pipe& pipe)
{
	pipe.async_connect("\\\\.\\pipe\\beam", [&](const std::error_code& ec, size_t)
	{
		std::shared_ptr<buffer> read_buf = std::make_shared<buffer>(4096, 'B');
		std::shared_ptr<buffer> write_buf = std::make_shared<buffer>(100, 'A');
		async_write_loop(pipe, write_buf);
	});
}

int main(int argc, const char** argv)
{
	if (argc < 2)
		return -1;

	
	if (std::string(argv[1]) == "server")
	{
		do_server();
	}
	else
	{
		beam::iocp context;
		beam::pipe server("\\\\.\\pipe\\beam", context);
		beam::pipe client(context);
		do_client(client);
		context.run();
	}

	return 0;
}