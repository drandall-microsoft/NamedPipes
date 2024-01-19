#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <memory>
#include <system_error>

#include <stdexcept>
#include <string>
#include <functional>

using byte = unsigned char;

namespace beam
{
	struct overlapped_op : OVERLAPPED
	{
		template <typename Fn>
		overlapped_op(void* obj, Fn&& fn)
			:obj(obj), on_complete(std::forward<Fn>(fn))
		{
			hEvent = CreateEventA(0, 0, 0, 0);
			Offset = { 0 };
			Pointer = 0;
			Internal = 0;
			InternalHigh = 0;
		}
		~overlapped_op()
		{
			::CloseHandle(hEvent);
		}

		void* obj;
		std::function<void(std::error_code, size_t)> on_complete;

		void complete(std::error_code ec, size_t len)
		{
			if (on_complete)
			{
				on_complete(ec, len);
			}
		}
	};

	class iocp
	{
		HANDLE handle_;
	public:
		iocp(DWORD threads=0)
		{
			handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, threads);
			if (handle_ == nullptr)
			{
				throw std::runtime_error("failed to initialize IoCompletionPort");
			}
		}
		~iocp()
		{
			CloseHandle(handle_);
		}

		void stop()
		{
		}

		template<typename Object>
		void associate(const Object& obj)
		{
			if (CreateIoCompletionPort(obj.handle(), handle_, 0, 0) == nullptr)
				throw std::runtime_error("failed to associate with iocp");
		}

		void run()
		{
			ULONG_PTR key = 0;
			OVERLAPPED* ovi = nullptr;
			while (1)
			{
				ovi = nullptr;
				DWORD bt = { 0 };
				if (GetQueuedCompletionStatus(handle_, &bt, &key, &ovi, INFINITE) != TRUE)
				{
					// TODO: Check stop condition.
					if (ovi == nullptr)
						throw std::runtime_error("failed to GetQueuedCompletionStatus");
					overlapped_op* op = reinterpret_cast<overlapped_op*>(ovi);
					op->complete(std::error_code(::GetLastError(), std::system_category()), bt);
					delete op;
				}

				if (ovi == nullptr)
					continue;
				
				overlapped_op* op = reinterpret_cast<overlapped_op*>(ovi);
				op->complete(std::error_code{}, bt);
				delete op;
			}
		}
	};

	class pipe
	{
	public:
		pipe(const std::string& name, iocp &context)
			:context_(context)
		{
			handle_ = CreateNamedPipeA(
				name.c_str(),
				PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
				PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
				PIPE_UNLIMITED_INSTANCES,
				4096,
				4096,
				0,
				nullptr);
			if (handle_ == INVALID_HANDLE_VALUE)
				throw std::runtime_error("failed to create named pipe");

			context.associate(*this);
		}

		pipe(iocp &context)
			:handle_(INVALID_HANDLE_VALUE), context_(context)
		{
		}

		~pipe()
		{
			::CloseHandle(handle_);
		}

		void connect(const std::string &name)
		{
			handle_ = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
			if (handle_ == INVALID_HANDLE_VALUE)
				throw std::runtime_error("couldn't connect to server");
			//
			DWORD readMode = PIPE_READMODE_MESSAGE;
			SetNamedPipeHandleState(handle_, &readMode, nullptr, nullptr);

			context_.associate(*this);
		}

		template<typename OnCompletion>
		void async_connect(const std::string& name, OnCompletion&& on_complete)
		{
			connect(name);
			overlapped_op* op = new overlapped_op{ this, on_complete };
			PostQueuedCompletionStatus(handle_, 0, 0, op);
		}

		template <typename OnCompletion>
		void async_accept(OnCompletion&& on_complete)
		{
			overlapped_op* op = new overlapped_op{ this, on_complete };
			if (ConnectNamedPipe(handle_, op) == FALSE)
			{
				auto ec = ::GetLastError();
				if (ec != ERROR_IO_PENDING)
				{
					op->complete(std::error_code(ec, std::system_category()), 0);
					delete op;
				}
			}
			else
			{
				op->complete(std::error_code{}, 0);
				delete op;
			}
		}

		template<typename OnCompletion>
		void async_write(const byte* buf, size_t len, OnCompletion&& on_complete)
		{
			overlapped_op* op = new overlapped_op{ this, on_complete };
			DWORD written = { 0 };
			if (WriteFile(handle_, buf, static_cast<DWORD>(len), &written, op) == FALSE)
			{
				auto ec = ::GetLastError();
				if (ec != ERROR_IO_PENDING)
				{
					op->complete(std::error_code(ec, std::system_category()), 0);
					delete op;
				}
			}
			else
			{
				PostQueuedCompletionStatus(handle_, written, 0, op);;
			}
		}

		template<typename OnCompletion>
		void async_read(byte* buf, size_t len, OnCompletion&& on_complete)
		{
			overlapped_op* op = new overlapped_op{ this, on_complete };
			DWORD read = { 0 };
			if (ReadFile(handle_, buf, static_cast<DWORD>(len), &read, op) == FALSE)
			{
				auto ec = ::GetLastError();
				if (ec != ERROR_IO_PENDING)
				{
					op->complete(std::error_code(ec, std::system_category()), 0);
					delete op;
				}
			}
			else
			{
				PostQueuedCompletionStatus(handle_, read, 0, op);
			}
		}

		HANDLE handle() const
		{
			return handle_;
		}
	private:
		iocp& context_;
		HANDLE handle_;
	};
}