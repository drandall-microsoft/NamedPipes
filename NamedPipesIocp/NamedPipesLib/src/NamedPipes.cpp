#include "NamedPipes.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <sstream>
#include <set>
#include <chrono>

namespace {
	using Bytes = DWORD;
	using BaseMessage = OVERLAPPED;
	using PipeHandle = HANDLE;

	constexpr size_t BufferSize = 4096;
	//constexpr size_t BufferSize = 1024 * 1024 * 10;
	constexpr Bytes MaxMessageLength = BufferSize * sizeof(char);
	constexpr auto PipePrefix = R"(\\.\pipe\)";
	constexpr auto PipeMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
	constexpr auto OpenMode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;

	struct Message : BaseMessage {
		PipeHandle Pipe{ nullptr };
		char Message[BufferSize];
		Bytes MessageSize{ 0 };
		bool AwaitingIo{ false };
	};

	std::set<PipeHandle> ClosedHandles;
	std::vector<Message*> WriteHandles;

	OnRequestFunc OnRequest{ nullptr };
	std::vector<std::string> Outbox;

	constexpr std::string ToPipeName(const char* name) {
		return PipePrefix + std::string(name);
	}

	constexpr void TrimEnd(std::string& str) {
		str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), str.end());
	}

	void WriteError(const std::string& failedMethod) {
		printf("%s failed with error code: %d\n", failedMethod.c_str(), GetLastError());
	}

	constexpr void RemoveWriter(size_t handleIndex) {
		std::swap(WriteHandles[handleIndex], WriteHandles.back());
		WriteHandles.pop_back();
	}

	void ConnectToPipe(Message& message) {
		message.AwaitingIo = false;
		if (ConnectNamedPipe(message.Pipe, &message)) {
			WriteError("ConnectNamedPipe");
			return;
		}
		switch (GetLastError()) {
		case ERROR_IO_PENDING: message.AwaitingIo = true; break;
		case ERROR_PIPE_CONNECTED: SetEvent(message.hEvent); break;
		default: WriteError("ConnectNamedPipe"); break;
		}
	}

	void DisconnectAndClose(Message* message) {
		if (DisconnectNamedPipe(message->Pipe)) {
			CloseHandle(message->Pipe);
			ClosedHandles.insert(message->Pipe);
			message->Pipe = INVALID_HANDLE_VALUE;
		}
		delete message;
	}

	bool IsPipeClosed(Message* message) {
		if (ClosedHandles.find(message->Pipe) != ClosedHandles.end()) {
			message->Pipe = INVALID_HANDLE_VALUE;
			ClosedHandles.erase(message->Pipe);
			return true;
		}
		return false;
	}

	void WINAPI HandleRead(Bytes err, Bytes readBytes, BaseMessage* base) {
		if (!base) {
			WriteError("HandleRead");
			return;
		}
		auto message = static_cast<Message*>(base);
		if (IsPipeClosed(message)) return;
		if (err != 0) {
			DisconnectAndClose(message);
			return;
		}

		std::string request(message->Message, readBytes);
		TrimEnd(request);
		if (request == "listener") {
			auto newListener = new Message();
			newListener->Pipe = message->Pipe;
			WriteHandles.push_back(newListener);
		}
		else if (!request.empty()) {
			std::stringstream errors;
			if (!OnRequest(request, errors)) {
				Outbox.push_back(errors.str());
			}
		}

		//ReadNext
		if (!ReadFileEx(message->Pipe, message->Message, MaxMessageLength, message, HandleRead)) {
			WriteError("ReadFileEx");
			DisconnectAndClose(message);
		}
	}
}

struct NamedPipeServer::Impl {
	constexpr Impl(const char* pipeName, size_t timeoutMs, size_t maxInstances)
		: PipeName(ToPipeName(pipeName))
		, TimeoutMs(static_cast<Bytes>(timeoutMs))
		, MaxInstances(static_cast<Bytes>(maxInstances)) 
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

	void Tick() {
		WriteAllMessages();
		//returns 0 (false) if the event fires
		if (WaitForSingleObjectEx(NewConnectionMessage.hEvent, 0, true)) {
			return;
		}
		HandleNewConnection();
	}

	void HandleNewConnection() {
		if (NewConnectionMessage.AwaitingIo) {
			if (!GetOverlappedResult(NewConnectionMessage.Pipe, &NewConnectionMessage, &NewConnectionMessage.MessageSize, false)) {
				WriteError("GetOverlappedResult");
				return;
			}
		}

		//NewConnectionMessage has a pipe which someone connected to
		//Create a reader for the connection, and give it the pipe
		auto reader = new Message{};
		reader->Pipe = NewConnectionMessage.Pipe;

		//Generate a new pipe for the next connection
		ListenForNewConnection();

		//Start the reader
		HandleRead(0, 0, reader);
	}

	void ListenForNewConnection() {
		NewConnectionMessage.Pipe = CreateNamedPipe(PipeName.c_str(), PipeMode, OpenMode, MaxInstances, MaxMessageLength, MaxMessageLength, TimeoutMs, nullptr);
		if (NewConnectionMessage.Pipe == INVALID_HANDLE_VALUE) {
			WriteError("CreateNamedPipe");
			return;
		}

		ConnectToPipe(NewConnectionMessage);
	}

	void WriteMessage(const std::string& message, size_t handleIndex) {
		auto remainingBytes = static_cast<Bytes>(message.size());
		auto handle = WriteHandles[handleIndex];
		
		while (remainingBytes > 0) {
			if (!WriteFile(handle->Pipe, message.data(), remainingBytes, &handle->MessageSize, handle)) {
				if (GetLastError() == ERROR_IO_PENDING) {
					//note: blocking call, but owuld require more complexity to avoid
					GetOverlappedResult(handle->Pipe, handle, &handle->MessageSize, true);
				}
				else {
					WriteError("WriteFile");
					DisconnectAndClose(handle);
				}
			}
			remainingBytes -= (std::min)(handle->MessageSize, remainingBytes);
		}
	}

	constexpr void WriteAllMessages() {
		if (Outbox.empty() || WriteHandles.empty()) return;

		for (const auto& message : Outbox) {
			for (int i = static_cast<int>(WriteHandles.size()) - 1; i >= 0; i--) {
				if (IsPipeClosed(WriteHandles[i])) {
					RemoveWriter(i);
					continue;
				}

				WriteMessage(message, i);
			}
		}

		Outbox.clear();
	}

	std::string PipeName{ "Unset" };
	Bytes TimeoutMs{ 0 };
	Bytes MaxInstances{ 0 };

	Message NewConnectionMessage{};
};

void NamedPipeServer::Start() {
	pImpl->Start();
}
void NamedPipeServer::Stop() {
	pImpl->Stop();
}
void NamedPipeServer::Tick() {
	pImpl->Tick();
}

NamedPipeServer::NamedPipeServer(const char* pipeName, size_t timeoutMs, size_t maxInstances)
	: pImpl(std::make_unique<NamedPipeServer::Impl>(pipeName, timeoutMs, maxInstances))
{}

NamedPipeServer::~NamedPipeServer() = default;

struct NamedPipeClient::Impl {
	constexpr Impl(const char* pipeName)
		: PipeName(ToPipeName(pipeName))
	{}

	bool Start() {
		using namespace std::chrono_literals;

		Reader.hEvent = CreateEvent(nullptr, true, true, nullptr);
		if (!Reader.hEvent) {
			WriteError("CreateEvent");
			return false;
		}

		Reader.Pipe = INVALID_HANDLE_VALUE;
		while (Reader.Pipe == INVALID_HANDLE_VALUE) {
			Reader.Pipe = CreateFile(PipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, nullptr);
			if (Reader.Pipe != INVALID_HANDLE_VALUE) break;

			std::this_thread::sleep_for(5s);
		}

		Bytes readMode = PIPE_READMODE_MESSAGE;
		SetNamedPipeHandleState(Reader.Pipe, &readMode, nullptr, nullptr);

		Writer.Pipe = Reader.Pipe;
		//ConnectToPipe(Reader);
		HandleRead(0, 0, &Reader);
		return true;
	}

	void Stop() {
		DisconnectAndClose(&Reader);
		DisconnectAndClose(&Writer);
	}

	void Tick() {
		/*
		if (!Connected) {
			if (WaitForSingleObjectEx(Reader.hEvent, 0, true)) {
				return;
			}
			Connect();
		}
		*/
		WaitForSingleObjectEx(Reader.hEvent, 0, true);
		WriteAllMessages();
	}
	/*
	void Connect() {
		if (Reader.AwaitingIo) {
			if (!GetOverlappedResult(Reader.Pipe, &Reader, &Reader.MessageSize, false)) {
				WriteError("GetOverlappedResult");
				return;
			}
		}

		Connected = true;

		//Start the reader
		HandleRead(0, 0, &Reader);
	}
	*/

	void WriteMessage(const std::string& message) {
		auto remainingBytes = static_cast<Bytes>(message.size());

		while (remainingBytes > 0) {
			if (!WriteFile(Writer.Pipe, message.data(), remainingBytes, &Writer.MessageSize, &Writer)) {
				if (GetLastError() == ERROR_IO_PENDING) {
					//note: blocking call, but owuld require more complexity to avoid
					GetOverlappedResult(Writer.Pipe, &Writer, &Writer.MessageSize, true);
				}
				else {
					WriteError("WriteFile");
					DisconnectAndClose(&Writer);
				}
			}
			remainingBytes -= (std::min)(Writer.MessageSize, remainingBytes);
		}
	}

	constexpr void WriteAllMessages() {
		if (Outbox.empty()) return;

		for (const auto& message : Outbox) {
			WriteMessage(message);
		}

		Outbox.clear();
	}

	std::string PipeName{ "Unset" };
	size_t TimeoutMs{ 0 };
	size_t MaxInstances{ 0 };
	
	//bool Connected{ false };
	Message Reader{};
	Message Writer{};
};

void NamedPipeClient::Start() {
	pImpl->Start();
}
void NamedPipeClient::Stop() {
	pImpl->Stop();
}
void NamedPipeClient::Tick() {
	pImpl->Tick();
}

NamedPipeClient::NamedPipeClient(const char* pipeName)
	: pImpl(std::make_unique<NamedPipeClient::Impl>(pipeName))
{}

NamedPipeClient::~NamedPipeClient() = default;

namespace NamedPipes {
	std::unique_ptr<NamedPipeServer>& GetServer() {
		static std::unique_ptr<NamedPipeServer> server;
		return server;
	}

	std::unique_ptr<NamedPipeClient>& GetClient() {
		static std::unique_ptr<NamedPipeClient> client;
		return client;
	}

	void Write(const std::string& message) {
		Outbox.push_back(message);
	}

	NamedPipeServer& StartServer(const char* pipeName, OnRequestFunc onRequest, size_t timeoutMs, size_t maxInstances) {
		if (GetClient()) throw "Either client or server, not both";
		OnRequest = onRequest;
		GetServer().reset(new NamedPipeServer(pipeName, timeoutMs, maxInstances));
		GetServer()->Start();
		return *GetServer();
	}

	void StopServer() {
		if (GetServer()) {
			GetServer()->Stop();
			GetServer().reset();
		}
	}

	NamedPipeClient& StartClient(const char* pipeName, OnRequestFunc onRequest) {
		if (GetServer()) throw "Either client or server, not both";
		OnRequest = onRequest;
		GetClient().reset(new NamedPipeClient(pipeName));
		GetClient()->Start();
		return *GetClient();
	}
}