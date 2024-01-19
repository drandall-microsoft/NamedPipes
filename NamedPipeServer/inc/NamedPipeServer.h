#pragma once

#include <iosfwd>
#include <string>
#include <memory>
#include <functional>

using OnRequestFunc = std::function<bool(const std::string&, std::ostream&)>;

struct INamedPipeServer {
public:
	INamedPipeServer(OnRequestFunc onRequest) : OnRequest(onRequest) {}
	virtual ~INamedPipeServer() = default;

	virtual void Start() = 0;
	virtual void Stop() = 0;

	virtual void Tick() = 0;
	virtual void Write(const std::string& message) {
		/*
		if (message.back() == '\n') {
			Outbox.push_back(message);
		}
		else {
			Outbox.push_back(message + '\n');
		}
		*/
		Outbox.push_back(message);
	}

	OnRequestFunc OnRequest;
	std::vector<std::string> Outbox;
};

namespace NamedPipes {
	template<typename ServerType>
	std::unique_ptr<ServerType>& GetServer() {
		static std::unique_ptr<ServerType> server;
		return server;
	}

	template<typename ServerType>
	ServerType& StartServer(const char* pipeName, OnRequestFunc onRequest) {
		GetServer<ServerType>().reset(new ServerType(pipeName, onRequest));
		GetServer<ServerType>()->Start();
		return *GetServer<ServerType>();
	}

	template<typename ServerType>
	void Write(const std::string& message) {
		GetServer<ServerType>()->Write(message);
	}

	template<typename ServerType>
	void StopServer() {
		if (GetServer<ServerType>()) {
			GetServer<ServerType>()->Stop();
			GetServer<ServerType>().reset();
		}
	}
}

/*
namespace NamedPipes {
	INamedPipeServer& StartServer(const char* pipeName, OnRequestFunc onRequest);

	void Write(const std::string& message);
	void StopServer();
}
*/