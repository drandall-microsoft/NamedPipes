#include "CompletionServer.h"

#include "BaseNamedPipe.h"
#include <set>

namespace {
	std::vector<Message*> WriteHandles;

	void RemoveWriter(size_t handleIndex) {
		std::swap(WriteHandles[handleIndex], WriteHandles.back());
		WriteHandles.pop_back();
	}

	void WriteMessage(const std::string& message, size_t handleIndex) {
		auto remainingBytes = static_cast<Bytes>(message.size());
		auto handle = WriteHandles[handleIndex];
		Bytes actualWritten;

		while (remainingBytes > 0) {
			if (!WriteFile(handle->Pipe, message.data(), remainingBytes, &actualWritten, handle)) {
				if (GetLastError() == ERROR_IO_PENDING) {
					GetOverlappedResult(handle->Pipe, handle, &actualWritten, true);
				}
				else {
					WriteError("WriteFile");
				}
			}
			if (actualWritten > remainingBytes) {
				remainingBytes = 0;
			}
			else {
				remainingBytes -= actualWritten;
			}
		}
	}

	void WINAPI HandleRead(Bytes Err, Bytes ReadBytes, BaseMessage* Base) {
		if (Base == nullptr) {
			printf("Received null message");
			return;
		}
		auto message = static_cast<Message*>(Base);
		if (Err != 0) {
			DisconnectAndClose(message);
			return;
		}

		std::string request(message->Message, ReadBytes);
		request.erase(std::find_if(request.rbegin(), request.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), request.end());

		auto ReadNext = [&]() {
			if (!ReadFileEx(message->Pipe, message->Message, MaxMessageLength, message, HandleRead)) {
				WriteError("ReadFileEx");
				DisconnectAndClose(message);
			}
		};

		std::stringstream errors;
		bool success = false;
		if (request == "listener") {
			auto newListener = new Message();
			newListener->Pipe = message->Pipe;
			WriteHandles.push_back(newListener);
			//WriteMessage(std::to_string(BufferSize), WriteHandles.size() - 1);
		}
		else if (!OnRequestFree(request, errors)) {
			Outbox.push_back(errors.str());
		}
		ReadNext();
	}
}

struct CompletionPipeServer::Impl {
	Impl(const char* pipeName, std::vector<std::string>& messages)
		: PipeName(ToPipeName(pipeName))
		, Messages(messages) 
	{}

	bool Start() {
		NewConnectionMessage.hEvent = CreateEvent(nullptr, true, true, nullptr);
		if (NewConnectionMessage.hEvent == nullptr) {
			WriteError("CreateEvent");
			return false;
		}

		ListenForNewConnection();
		return true;
	}

	void Stop() {
		for (auto writer : WriteHandles) {
			DisconnectAndClose(writer);
		}
		WriteHandles.clear();

		if (DisconnectNamedPipe(NewConnectionMessage.Pipe)) {
			CloseHandle(NewConnectionMessage.Pipe);
		}
	}

	void WriteAllMessages() {
		if (Messages.empty() || WriteHandles.empty()) return;

		for (const auto& message : Messages) {
			for (int i = static_cast<int>(WriteHandles.size()) - 1; i >= 0; i--) {
				if (IsPipeClosed(WriteHandles[i])) {
					RemoveWriter(i);
					continue;
				}

				WriteMessage(message, i);
			}
		}
		Messages.clear();
	}

	void Tick() {
		WriteAllMessages();
		//returns 0 (false) if the event fired
		if (WaitForSingleObjectEx(NewConnectionMessage.hEvent, 0, true)) {
			return;
		}
		HandleNewConnection();
	}

	void HandleNewConnection() {
		if (NewConnectionMessage.AwaitingIo) {
			Bytes responseSize;
			if (!GetOverlappedResult(NewConnectionMessage.Pipe, &NewConnectionMessage, &responseSize, false)) {
				WriteError("GetOverlappedResult");
				return;
			}
		}

		//NewConnectionMessage has a pipe which someone connected to
		//Create a reader and a writer for the connection
		auto reader = new Message{};
		//auto writer = new Message{};
		//Assign this pipe to both (it's a duplexed connection)
		reader->Pipe = NewConnectionMessage.Pipe;
		//writer->Pipe = NewConnectionMessage.Pipe;
		
		//Generate a new pipe for the next connection
		ListenForNewConnection();

		//Start listening to incoming messages from the pipe
		HandleRead(0, 0, reader);

		//Register the pipe to receive our messages
		//WriteHandles.push_back(writer);
	}

	void ListenForNewConnection() {
		NewConnectionMessage.Pipe = CreateNamedPipe(PipeName.c_str(), PipeMode, OpenMode, ChannelCount, MaxMessageLength, MaxMessageLength, 5000, nullptr);
		if (NewConnectionMessage.Pipe == INVALID_HANDLE_VALUE) {
			WriteError("CreateNamedPipe");
			return;
		}

		ConnectToNewClient(NewConnectionMessage);
	}

	std::string PipeName{ "Unset" };

	Message NewConnectionMessage{};
	std::vector<std::string>& Messages;
};

void CompletionPipeServer::Start() {
	pImpl->Start();
}
void CompletionPipeServer::Stop() {
	pImpl->Stop();
}

void CompletionPipeServer::Tick() {
	pImpl->Tick();
}


CompletionPipeServer::CompletionPipeServer(const char* pipeName, OnRequestFunc onRequest)
	: INamedPipeServer(onRequest)
	, pImpl(std::make_unique<CompletionPipeServer::Impl>(pipeName, Outbox))
{
	OnRequestFree = onRequest;
}