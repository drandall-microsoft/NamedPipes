#pragma once

#include "NamedPipeServer.h"
#include "StringUtils.h"

#include <Windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <string>
#include <sstream>
#include <functional>
#include <array>
#include <iostream>
#include <thread>
#include <set>

namespace {
	using Bytes = DWORD;
	using Chars = char;
	using BaseMessage = OVERLAPPED;
	using PipeHandle = HANDLE;

	constexpr size_t ChannelCount = 10;
	//constexpr size_t BufferSize = 4096;
	constexpr size_t BufferSize = 10;
	constexpr Bytes MaxMessageLength = BufferSize * sizeof(Chars);
	constexpr auto PipePrefix = R"(\\.\pipe\)";
	constexpr auto PipeMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
	constexpr auto OpenMode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;

	struct Message : BaseMessage {
		PipeHandle Pipe{ nullptr };
		Chars Message[BufferSize];
		Bytes MessageSize{ 0 };
		bool AwaitingIo{ false };
	};

	OnRequestFunc OnRequestFree;
	std::vector<std::string> Outbox;
	std::set<PipeHandle> ClosedHandles;

	void WriteError(const std::string& failedMethod) {
		printf("%s failed with error code: %d\n", failedMethod.c_str(), GetLastError());
	}

	std::string ToPipeName(const char* name) {
		constexpr auto prefix = R"(\\.\pipe\)";
		std::string result = prefix;
		result += name;
		return result;
	}

	void ConnectToNewClient(Message& message) {
		message.AwaitingIo = false;
		//ConnectNamedPipe returns false if successful
		if (ConnectNamedPipe(message.Pipe, &message)) {
			WriteError("ConnectNamedPipe");
			return;
		}

		auto lastError = GetLastError();
		if (lastError == ERROR_IO_PENDING) {
			message.AwaitingIo = true;
		}
		else if (lastError == ERROR_PIPE_CONNECTED) {
			if (!SetEvent(message.hEvent)) { //signal event
				WriteError("SetEvent");
			}
		}
		else {
			WriteError("ConnectNamedPipe");
		}
	}

	void DisconnectAndClose(Message* message) {
		if (message->Pipe == INVALID_HANDLE_VALUE) {
			//already deleted
			return;
		}
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


	bool ReadSync(PipeHandle pipe, Chars* buffer, std::string& outMessage) {
		Bytes bytesRead;
		bool success = ReadFile(pipe, buffer, MaxMessageLength, &bytesRead, nullptr);
		if (!success || bytesRead == 0) {
			if (GetLastError() == ERROR_BROKEN_PIPE) {
				outMessage = "Client disconnected";
			}
			else {
				outMessage = "Read failed with error code: " + std::to_string(GetLastError());
			}
			return false;
		}

		outMessage = std::string(buffer, bytesRead);
		StringUtils::TrimEnd(outMessage);
		return true;
	}
}
