#include "NamedPipeOstream.h"
#include "NamedPipeServer.h"
#include "OverlapServer.h"
#include "MultithreadedServer.h"
#include "CompletionServer.h"

#include <sstream>

struct NamedPipeOstream::StringBuf : public std::stringbuf {
	int sync() {
		constexpr int SyncSuccess = 0;
		constexpr int SyncFailure = -1;

		NamedPipes::Write<OverlapPipeServer>(str());
		//NamedPipes::Write<CompletionPipeServer>(str());
		//NamedPipes::Write<MtPipeServer>(str());
		//NamedPipes::Write<ServerType>(str());
		str("");
		return SyncSuccess;
	}
};

NamedPipeOstream::NamedPipeOstream(std::unique_ptr<NamedPipeOstream::StringBuf>&& buffer)
	: std::ostream(buffer.get())
	, Buffer(std::move(buffer)) {}

NamedPipeOstream::NamedPipeOstream()
	: NamedPipeOstream(std::make_unique<NamedPipeOstream::StringBuf>())
{}

NamedPipeOstream::~NamedPipeOstream() {
	Buffer.reset();
}