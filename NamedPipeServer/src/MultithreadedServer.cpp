#include "MultithreadedServer.h"
#include "BaseNamedPipe.h"

namespace {
	void ReadTick(PipeHandle pipe) {
		bool success = false;
		Chars buffer[BufferSize];
		std::stringstream errors;
		std::string message;

		while (true) {
			if (ReadSync(pipe, buffer, message)) {
				if (!OnRequestFree(message, errors)) {
					NamedPipes::Write<MtPipeServer>(errors.str());
				}
			}
			else {
				printf("%s", message.c_str());
				break;
			}
		}
	}

	void WriteTick(PipeHandle pipe) {
		bool success = false;
		Chars buffer[BufferSize];
		Bytes bytesWritten;

		while (true) {
			if (!Outbox.empty()) {
				auto copy = Outbox;
				for (const auto& message : copy) { //yup, not safe.  WriteFile blocks
					auto messageSize = static_cast<Bytes>(message.size());
					strcpy_s(buffer, message.c_str());
					success = WriteFile(pipe, buffer, messageSize, &bytesWritten, nullptr);
					if (!success || bytesWritten != message.size()) {
						printf("WriteFile failed with error %d", GetLastError());
					}
				}
			}
		}
	}
}

struct MtPipeServer::Impl {
	Impl(const char* pipeName)
		: PipeName(ToPipeName(pipeName)) {}

	void Start() {}

	void Stop() {
		for (auto& thread : ClientThreads) {
			thread.join();
		}
	}

	void Tick() {
		ListenPipe = CreateNamedPipe(PipeName.c_str(), PipeMode, OpenMode, ChannelCount, 0, BufferSize, 0, nullptr);
		if (ListenPipe == INVALID_HANDLE_VALUE) {
			printf("Failed to create named pipe: %d", GetLastError());
			return;
		}

		if (ConnectNamedPipe(ListenPipe, nullptr)) {
			ClientThreads.push_back(std::thread(ReadTick, ListenPipe));
			ClientThreads.push_back(std::thread(WriteTick, ListenPipe));
		}
	}

	std::string PipeName{ "Unset" };

	PipeHandle ListenPipe{};

	std::stringstream Errors;
	std::vector<std::thread> ClientThreads;
};

void MtPipeServer::Start() {
	pImpl->Start();
}

void MtPipeServer::Stop() {
	pImpl->Stop();
}

void MtPipeServer::Tick() {
	pImpl->Tick();
}

MtPipeServer::MtPipeServer(const char* pipeName, OnRequestFunc onRequest)
	: INamedPipeServer(onRequest)
	, pImpl(std::make_unique<MtPipeServer::Impl>(pipeName))
{
	OnRequestFree = onRequest;
}
