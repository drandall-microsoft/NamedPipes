#pragma once

#include <ostream>
#include <memory>
#include <sstream>

class NamedPipeOstream : public std::ostream {
public:
	NamedPipeOstream();
	~NamedPipeOstream();
private:
	struct StringBuf;

	NamedPipeOstream(std::unique_ptr<StringBuf>&& buffer);

	std::unique_ptr<StringBuf> Buffer{};
};
