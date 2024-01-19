#include "OverlapServer.h"
#include "BaseNamedPipe.h"

namespace {

	//OnRequestFunc OnRequestFree;

	void DisconnectAndReconnect(Message& message)
	{
		if (!DisconnectNamedPipe(message.Pipe))
		{
			printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
		}

		ConnectToNewClient(message);
		//channel.IsConnected = channel.PendingIO;
	}

}

struct OverlapPipeServer::Impl {
	Impl(const char* pipeName, std::vector<std::string>& outbox)
		: PipeName(ToPipeName(pipeName))
		, Outbox(outbox) {}

	void Start() {

		for (auto i = 0; i < ChannelCount; i++) {
			auto pipe = CreateNamedPipe(PipeName.c_str(), PipeMode, OpenMode, ChannelCount, BufferSize, BufferSize, 5000, nullptr);

			if (pipe == INVALID_HANDLE_VALUE) {
				printf("CreateNamedPipe failed with error %d", GetLastError());
				continue;
			}

			auto& readHandle = ReadHandles[i];
			auto& writeHandle = WriteHandles[i];
			readHandle = CreateEvent(nullptr, true, true, nullptr);
			writeHandle = CreateEvent(nullptr, true, true, nullptr);

			Readers[i].hEvent = readHandle;
			Writers[i].hEvent = writeHandle;
			Readers[i].Pipe = pipe;
			Writers[i].Pipe = pipe;

			ConnectToNewClient(Readers[i]);
			ConnectToNewClient(Writers[i]);
		}
	}

	void HandleAwait(Message& message) {
		constexpr bool doNotWait = false;
		Bytes bytesTransferred;

		if (message.AwaitingIo) {
			if (!GetOverlappedResult(message.Pipe, &message, &bytesTransferred, doNotWait)) {
				printf("GetOverlappedResult failed with error %d\n", GetLastError());
			}
			message.AwaitingIo = false;
		}
		/*
		if (channel.IsConnected) {
			if (!success) DisconnectAndReconnect(channel);
			channel.MessageSize = bytesTransferred;
		}
		else {
			if (success) channel.IsConnected = true;
		}
		*/
	}

	void Tick() {
		constexpr bool waitOne = false;
		constexpr bool shouldWait = false;
		bool success = false;

		//Read from all channels

		while (true) {
			auto i = WaitForMultipleObjects(ChannelCount, &ReadHandles[0], waitOne, 0);
			if (i == WAIT_TIMEOUT) break;
			auto& reader = Readers[i - WAIT_OBJECT_0];
			HandleAwait(reader);

			success = ReadFile(reader.Pipe, reader.Message, MaxMessageLength, nullptr, &reader);
			if (reader.MessageSize > 0) {
				std::string message = std::string(reader.Message, reader.MessageSize);
				StringUtils::TrimEnd(message);
				if (!OnRequestFree(message, Errors)) {
					Outbox.push_back(Errors.str());
				}
				reader.AwaitingIo = true;
				reader.MessageSize = 0;
				reader.Message[0] = '\0';
			}
		}

		while (true) {
			auto i = WaitForMultipleObjects(ChannelCount, &WriteHandles[0], waitOne, 0);
			if (i == WAIT_TIMEOUT) break;
			HandleAwait(Writers[i - WAIT_OBJECT_0]);
		}
		//Write to all channels
		Bytes bytesTransferred;
		auto copy = Outbox;
		Outbox.clear();

		for (const auto& message : copy) {
			for( auto& writer : Writers) {
				if (writer.AwaitingIo) continue;
				strcpy_s(writer.Message, message.c_str());
				writer.MessageSize = static_cast<Bytes>(message.size());

				if (!WriteFile(writer.Pipe, writer.Message, writer.MessageSize, &bytesTransferred, &writer)) {
					printf("WriteFile failed with error code %d\n", GetLastError());
				}
				writer.Message[0] = '\0';
				writer.MessageSize = 0;
				writer.AwaitingIo = true;
			}
		}
		Outbox.clear();
	}

	std::string PipeName{ "Unset" };
	std::array<PipeHandle, ChannelCount> ReadHandles{};
	std::array<PipeHandle, ChannelCount> WriteHandles{};
	std::array<Message, ChannelCount> Readers{};
	std::array<Message, ChannelCount> Writers{};

	std::stringstream Errors;
	std::vector<std::string>& Outbox;
};

void OverlapPipeServer::Start() {
	pImpl->Start();
}
void OverlapPipeServer::Stop() {
	//Maybe not needed?
}

void OverlapPipeServer::Tick() {
	pImpl->Tick();
}

OverlapPipeServer::OverlapPipeServer(const char* pipeName, OnRequestFunc onRequest)
	: INamedPipeServer(onRequest)
	, pImpl(std::make_unique<OverlapPipeServer::Impl>(pipeName, Outbox)) {
	OnRequestFree = onRequest;
}
