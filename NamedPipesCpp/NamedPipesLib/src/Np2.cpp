#include "Np2.h"
#include <memory>
#include <thread>
#include <sstream>

#include "iocp.h"

namespace {

	beam::iocp Context{};
	std::unique_ptr<beam::pipe> Pipe;
	std::thread Thread;
	std::string PipeName{ "Unset" };
	OnMessageFunc OnMessage;
	bool Stopping{ false };

	struct Listener {
		void AcceptAsync(std::function<void()> onAccept) {
			Pipe->async_accept([onAccept](std::error_code ec, size_t) {if (!ec) onAccept();});
		}
		void ConnectAsync(std::function<void()> onConnect) {
			Pipe->async_connect(PipeName, [onConnect](std::error_code ec, size_t) { if (!ec) onConnect(); });
		}

		void ReadAsync() {
			Pipe->async_read(Buffer.data(), Buffer.size(), [this](std::error_code ec, size_t msgLength) {
				if (ec) return;
				std::string msg = std::string(reinterpret_cast<char*>(Buffer.data()), msgLength);
				std::stringstream errors{};
				if (msgLength > 0) {
					OnMessage(msg, errors);
				}
				
				ReadAsync();
			});
		}
		void WriteAsync(std::string_view message) {
			auto* msg = reinterpret_cast<const byte*>(message.data());
			auto onFinished = [](std::error_code, size_t) {};
			Pipe->async_write(msg, message.size(), onFinished);
		}

		std::vector<byte> Buffer = std::vector<byte>(1024ull, byte{});
		bool Active{ false };
	};

	std::vector<Listener> Listeners{};
	std::unique_ptr<Listener> Client;

	void AcceptConnection() {
		Listeners.emplace_back();
		auto& next = Listeners.back();
		next.AcceptAsync([&next]() {
			next.Active = true;
			next.ReadAsync();
			//AcceptConncetion();
		});
	}
}

namespace Np2 {
	void Write(std::string_view msg) {
		if (Client) Client->WriteAsync(msg);

		for (auto& listener : Listeners) {
			if (listener.Active) {
				listener.WriteAsync(msg);
			}
		}
	}

	void StartServer(std::string_view pipeName, OnMessageFunc onRequest){
		if (Pipe) throw "Bad";
		Stopping = false;
		OnMessage = onRequest;
		PipeName = std::string(R"(\\.\pipe\)") + std::string(pipeName);
		Pipe = std::make_unique<beam::pipe>(PipeName, Context);
		AcceptConnection();
		
		Thread = std::thread([]() {while (!Stopping) Context.run(); });
	}

	void StopServer(){
		Stopping = true;
		Thread.join();
		Pipe.reset();
	}

	void StartClient(std::string_view pipeName, OnMessageFunc onResponse){
		if (Pipe) throw "Bad";
		Stopping = false;
		OnMessage = onResponse;
		PipeName = std::string("\\\\.\\pipe\\") + std::string(pipeName);
		Pipe = std::make_unique<beam::pipe>(PipeName, Context);
		Client = std::make_unique<Listener>();
		Client->ConnectAsync([]() {
			Client->ReadAsync();
		});

		Thread = std::thread([]() {while (!Stopping) Context.run(); });
	}

	void StopClient() {
		Stopping = true;
		Thread.join();
		Pipe.reset();
		Client.reset();
	}
}