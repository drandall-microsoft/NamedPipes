#pragma once

#include "NamedPipeServer.h"

struct MtPipeServer : public INamedPipeServer {
	MtPipeServer(const char* pipeName, OnRequestFunc onRequest);

	void Start() override;
	void Stop() override;

	void Tick() override;

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;
};
