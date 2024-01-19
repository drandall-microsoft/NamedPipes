#include "CompletionServer.h"
#include "OverlapServer.h"
#include "MultithreadedServer.h"
#include "NamedPipeOstream.h"

#include <sstream>
#include <iostream>
#include "Options.test.h"
#include <thread>

static long ticks = 0;
static long requests = 0;

bool HandleRequest(const std::string& request, std::ostream& errors) {
	/*
	requests++;

	auto stream = std::stringstream();
	stream << "Ticks: " << ticks << " Requests: " << requests << std::endl;
	stream << request << std::endl;
	std::cout << stream.str();

	return true;
	*/

	if (request.empty()) {
		return true;
	}

	size_t resultSize;
	std::stringstream stream(request);
	stream >> resultSize;

	std::string message;
	message.reserve(resultSize);
	for (auto i = 0; i < resultSize; i++) {
		message.push_back((rand() % 26) + 'a');
	}

	printf("Writing %s\n", message.c_str());
	NamedPipes::Write<CompletionPipeServer>(message);
	return true;
}

void WriteMessages() {
	using namespace std::chrono_literals;
	NamedPipeOstream stream;

	for (auto i = 0; i < 100; i++) {
		std::this_thread::sleep_for(1000ms);
		stream << "Message from server " << i << std::flush;
	}
}
int main() {
	/*
	if (TestOptions()) {
		std::cout << "Test pass";
	}
	else {
		std::cout << "Test Fail";
	}
	*/
	
	//auto& server = NamedPipes::Start1ChannelServer("ExamplePipeName", HandleRequest);
	//auto& server = OverlapPipes::StartServer("ReadPipeName", "WritePipeName", HandleRequest);
	//auto& server = OverlapPipes::StartServer("ExamplePipeName", HandleRequest);
	//auto& server = NamedPipes::StartServer<MtPipeServer>("ExamplePipeName", HandleRequest);
	
	auto& server = NamedPipes::StartServer<CompletionPipeServer>("ExamplePipeName", HandleRequest);
	printf("Server started\n");
	//auto& server = NamedPipes::StartServer<OverlapPipeServer>("ExamplePipeName", HandleRequest);

	//auto writeThread = std::thread(WriteMessages);

	//CompletionServer server("ExamplePipeName");
	/*
	if (!server.Start(errors)) {
		std::cout << "Start failed:\n\t" << errors.str();
		return 0;
	}
	*/

	while (true) {
		ticks++;
		server.Tick();
	}
	//writeThread.join();

	return 0;
	
}
