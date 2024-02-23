/*
#include "NamedPipes.h"
#include <chrono>
#include <iostream>

bool Stopping{ false };
bool HandleRequest(const std::string& message, std::ostream&) {
	static int messageCount = 0;
	static auto start = std::chrono::high_resolution_clock::now();
	constexpr size_t expectedCount = 271892; //4k
	//constexpr size_t expectedCount = 17271; //64k
	//constexpr size_t expectedCount = 404; //10Mb
	messageCount++;
	
	if (messageCount % expectedCount == 0) {
		auto now = std::chrono::high_resolution_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
		printf("Elapsed time: %lldms\n", elapsed);
	}
	else if (messageCount % expectedCount == 1) {
		start = std::chrono::high_resolution_clock::now();
	}
	//printf("Message %d", messageCount);
	//printf("Client received message %s\n", message.c_str());
	//printf("Client received message of size %d\n", message.size());
	return true;
}

void TickClient() {
	auto& client = NamedPipes::StartClient("ExamplePipeName", HandleRequest);
	NamedPipes::Write("listener");

	while (!Stopping) {
		client.Tick();
	}
	client.Stop();
}

int main() {
	auto tickThread = std::thread(TickClient);

	std::string request;
	while (true) {
		std::cout << "Type message to send to server\n";
		std::getline(std::cin, request);

		if (request.empty()) {
			Stopping = true;
			break;
		}

		NamedPipes::Write(request);
	}
	tickThread.join();
}
*/


#include "iocp.h"
#include <system_error>
#include <vector>
#include <iostream>

using buffer = std::vector<byte>;

void async_read_loop(beam::pipe& pipe, std::shared_ptr<buffer>& read_buf)
{
	pipe.async_read(read_buf->data(), 4096, [&pipe, read_buf](const std::error_code& ec, size_t len) mutable
	{
		if (ec)
			return;
		std::cout << "read: " << len << " bytes; error_code: " << ec.value() << std::endl;
		async_read_loop(pipe, read_buf);
	});
}

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

void do_client(beam::pipe& pipe)
{
	pipe.connect("\\\\.\\pipe\\ExamplePipeName");
	std::shared_ptr<buffer> read_buf = std::make_shared<buffer>(4096, 'B');
	std::shared_ptr<buffer> write_buf = std::make_shared<buffer>(100, 'A');
	async_read_loop(pipe, read_buf);
	//async_write_loop(pipe, write_buf);
}

#include <thread>

int main(int argc, const char** argv)
{
	beam::iocp context;
	beam::pipe client(context);
	do_client(client);

	std::thread t([&]() { context.run(); });
	t.join();
	return 0;
}
