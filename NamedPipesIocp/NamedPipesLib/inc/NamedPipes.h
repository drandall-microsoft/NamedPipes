#pragma once

#include <iosfwd>
#include <memory>
#include <functional>
#include <string>

using OnRequestFunc = std::function<bool(const std::string&, std::ostream&)>;

class NamedPipeServer {
public:
	NamedPipeServer(const char* pipeName, size_t timeoutMs, size_t maxInstances);
	~NamedPipeServer();
	void Start();
	void Stop();
	void Tick();

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;
};

class NamedPipeClient {
public:
	NamedPipeClient(const char* pipeName);
	~NamedPipeClient();
	void Start();
	void Stop();
	void Tick();

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;
};

namespace NamedPipes {
	NamedPipeServer& StartServer(const char* pipeName, OnRequestFunc onRequest, size_t timeoutMs = 5000, size_t maxInstances = 255);
	void StopServer();

	NamedPipeClient& StartClient(const char* pipeName, OnRequestFunc onRequest);
	void StopClient();

	void Write(const std::string& message);
}
