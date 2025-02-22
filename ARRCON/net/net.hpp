/**
 * @file	net.hpp
 * @author	radj307, Tiiffi
 * @brief	Contains all of the raw networking functions used by the rcon namespace.
 *\n		These functions were originally created by Tiiffi for mcrcon: (https://github.com/Tiiffi/mcrcon).
 *\n		Not much remains of the original code, and most of it is just basic socket programming, but I've included the original license anyway:
 *\n
 *\n		Copyright (c) 2012-2021, Tiiffi <tiiffi at gmail>
 *\n
 *\n		This software is provided 'as-is', without any express or implied
 *\n		warranty. In no event will the authors be held liable for any damages
 *\n		arising from the use of this software.
 *\n
 *\n		Permission is granted to anyone to use this software for any purpose,
 *\n		including commercial applications, and to alter it and redistribute it
 *\n		freely, subject to the following restrictions:
 *\n
 *\n		  1. The origin of this software must not be misrepresented; you must not
 *\n		  claim that you wrote the original software. If you use this software
 *\n		  in a product, an acknowledgment in the product documentation would be
 *\n		  appreciated but is not required.
 *\n
 *\n		  2. Altered source versions must be plainly marked as such, and must not be
 *\n		  misrepresented as being the original software.
 *\n
 *\n		  3. This notice may not be removed or altered from any source
 *\n		  distribution.
 */
#pragma once
#include "objects/packet.hpp"
#include "../exceptions.hpp"

#include <make_exception.hpp>

#include <optional>
#include <string>
#include <sys/socket.h>
#include <netdb.h>

#ifndef OS_WIN
// include additional headers on non-windows platforms:
#include <cstring>
#include <fcntl.h>
#endif

 /**
  * @namespace	net
  * @brief		Contains functions used to interact with sockets.
  */
namespace net {
#	ifdef OS_WIN
	/**
	 * @brief		Convert a std::chrono millisecond duration to a timeval struct.
	 * @param ms	Duration in milliseconds
	 * @returns		timeval
	 */
	inline timeval make_timeout(const std::chrono::milliseconds& ms)
	{
		return{ 0L, static_cast<long>(static_cast<double>(ms.count()) * 1000L) };
	}
#	define SELECT(nfds, rd, wr, ex, timeout) select(nfds, rd, wr, ex, timeout)
	/// @brief	Returns the last reported socket error code.
#	define LAST_SOCKET_ERROR_CODE() (WSAGetLastError())
#	else // POSIX
	/**
	 * @brief		Convert a std::chrono millisecond duration to a timespec struct.
	 * @param ms	Duration in milliseconds
	 * @returns		timespec
	 */
	inline timespec make_timeout(const std::chrono::milliseconds& ms)
	{
		return{ 0L, static_cast<long>(static_cast<double>(ms.count()) * 1000L) };
	}
#	define SELECT(nfds, rd, wr, ex, timeout) pselect(nfds, rd, wr, ex, timeout, nullptr)
	/// @brief	Returns the last reported socket error code.
#	define LAST_SOCKET_ERROR_CODE() (errno)
#	endif // #ifdef OS_WIN

	/**
	 * @brief	Returns the last reported socket error message.
	 * @returns	std::string
	 */
	inline std::string getLastSocketErrorMessage()
	{
#		ifdef OS_WIN
		constexpr const unsigned BUFFER_SIZE{ 256u };
		char msg[BUFFER_SIZE];
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			0,
			LAST_SOCKET_ERROR_CODE(),
			0,
			msg,
			BUFFER_SIZE,
			0
		);
		return{ msg };
#		else
		return{ strerror(LAST_SOCKET_ERROR_CODE()) };
#		endif
	}

	/**
	 * @brief		Check if the given socket descriptor is valid.
	 * @param sd	A Socket Descriptor to check.
	 * @returns		bool
	 */
	inline bool isValidSocket(const SOCKET& sd)
	{
		return sd != static_cast<SOCKET>(SOCKET_ERROR) && fcntl(sd, F_GETFD) != SOCKET_ERROR && LAST_SOCKET_ERROR_CODE() != EBADF;
	}

	/**
	 * @brief	Call WSAStartup & initialize the winsock dll.
	 *\n		Only necessary on Windows, as the Linux socket library
	 *			performs the startup & cleanup operations automatically.
	 */
	inline void init(void) noexcept(false)
	{
#		ifdef _WIN32
		WSADATA wsaData;
		int rc{ 0 };
		if (rc = WSAStartup(WINSOCK_VERSION, &wsaData); rc != 0)
			throw make_exception("WSAStartup failed with error code ", rc, "! Last Socket Error:  (", LAST_SOCKET_ERROR_CODE(), ") ", getLastSocketErrorMessage());
		else if (LOBYTE(wsaData.wVersion) != LOBYTE(WINSOCK_VERSION) || HIBYTE(wsaData.wVersion) != HIBYTE(WINSOCK_VERSION))
			throw make_exception("Winsock version is invalid!");
#		endif
	}

	/**
	 * @brief		Close the socket & call WSACleanup on windows.
	 * @param sd	The socket descriptor to close.
	 */
	inline void close_socket(const SOCKET& sd)
	{
#		ifdef _WIN32
		closesocket(sd);
		WSACleanup();
#		endif
	}

	/// @brief	Emergency stop handler, should be passed to the std::atexit() function to allow a controlled shutdown of the socket in the event of an interrupt.
	inline void cleanup(void)
	{
		if (Global.socket != static_cast<SOCKET>(SOCKET_ERROR))
			close_socket(Global.socket);
	}

	/**
	 * @brief			Connect the socket to the specified RCON server.
	 * @author			Tiiffi, radj307
	 * @param host		Target server IP address or hostname.
	 * @param port		Target server port.
	 * @throws except	Name resolution/connection failed.
	 * @returns			SOCKET
	*/
	inline SOCKET connect(const std::string& host, const std::string& port)
	{
		SOCKET sd;

		struct addrinfo* server_info, * p;

		struct addrinfo hints;
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		net::init();

		int ret{ getaddrinfo(host.c_str(), port.c_str(), &hints, &server_info) };

		if (ret != 0)
			throw connection_exception("net::connect()", "Name resolution failed!", host, port, LAST_SOCKET_ERROR_CODE(), getLastSocketErrorMessage());

		// Go through the hosts and try to connect
		for (p = server_info; p != NULL; p = p->ai_next) {
			sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

			if (sd == static_cast<SOCKET>(-1))
				continue;

			ret = connect(sd, p->ai_addr, static_cast<int>(p->ai_addrlen));
			if (ret == SOCKET_ERROR) {
				close_socket(sd);
				continue;
			}
			break; // connection successful, break from loop
		}

		freeaddrinfo(server_info); // release address info memory

		if (p == NULL)
			throw connection_exception("net::connect()", "Connection Failed.", host, port, LAST_SOCKET_ERROR_CODE(), getLastSocketErrorMessage());

		return sd;
	}

	/**
	 * @brief			Send a packet to the specified socket.
	 * @param sd		Socket to use.
	 * @param packet	Packet to send to the server.
	 * @returns			bool
	 *\n				true	Successfully sent packet
	 *\n				false	An error occurred while sending the packet.
	 */
	inline bool send_packet(const SOCKET& sd, const packet::Packet& packet)
	{
		int len;
		int total = 0;	// bytes we've sent
		int bytesleft;	// bytes left to send 
		int ret = -1;

		bytesleft = len = packet.size + sizeof(int);

		auto spacket{ packet.serialize() };

		auto* ptr = &spacket;

		while (total < len) {
			ret = send(sd, (char*)ptr + total, bytesleft, 0);
			if (ret == -1) break;
			total += ret;
			bytesleft -= ret;
		}

		return ret != -1;
	}

	/**
	 * @brief					Flush all remaining data from the socket.
	 * @param sd				Target Socket.
	 * @param do_check_first	When true, checks if there is data to flush before actually flushing it, which prevents unintended blocking if the socket is empty.
	 */
	inline void flush(const SOCKET& sd, const bool& do_check_first = true)
	{
		fd_set set;
		FD_ZERO(&set);
		FD_SET(sd, &set);

		const auto timeout{ make_timeout(Global.select_timeout) };
		if (do_check_first && SELECT(sd + 1ull, &set, nullptr, nullptr, &timeout) != 1)
			return;
		do {
			if (recv(sd, std::unique_ptr<char>{}.get(), packet::PSIZE_MAX, 0) == 0ul)
				throw socket_exception("net::flush()", "Connection Lost!", LAST_SOCKET_ERROR_CODE(), getLastSocketErrorMessage());
			std::this_thread::sleep_for(Global.receive_delay);
		} while (SELECT(sd + 1ull, &set, nullptr, nullptr, &timeout) == 1);
	}

	/**
	 * @brief		Receive a single packet from the specified socket.
	 * @param sd	Socket to use.
	 * @returns		packet::Packet
	 */
	inline packet::Packet recv_packet(const SOCKET& sd)
	{
		int psize{ 0 };
		ssize_t ret{ recv(sd, (char*)&psize, sizeof(int), 0) };

		// lambda to check if the return code is valid and packet size is valid
		const auto& validate{ [&psize, &ret, &sd]() {
			switch (ret) {
			case (sizeof(int)): // success
				break;
			case 0: // nothing received
				throw socket_exception("net::recv_packet()", "Connection Lost!", LAST_SOCKET_ERROR_CODE(), getLastSocketErrorMessage());
			case SOCKET_ERROR: // error
				throw socket_exception("net::recv_packet()", "Connection closed by server.");
			default: // invalid size
				throw socket_exception("net::recv_packet()", "Received a corrupted packet!", LAST_SOCKET_ERROR_CODE(), getLastSocketErrorMessage());
			}
			if (psize < packet::PSIZE_MIN)
				std::cerr << "Received unexpectedly small packet size: " << psize << std::endl;
			else if (psize > packet::PSIZE_MAX) {
				std::cerr << "Received unexpectedly large packet size: " << psize << std::endl;
				flush(sd); // flush the remaining data
			}
		} };
		validate();

		packet::serialized_packet spacket{ psize, 0, 0, { 0x00 } }; ///< create a serialized packet to receive data

		for (int received{ 0 }, ret{ 0 }; received < psize; received += ret) {
			ret = recv(sd, (char*)&spacket + sizeof(int) + received, static_cast<size_t>(psize) - received, 0);
			validate();
		}

		return { spacket };
	}

	inline std::chrono::milliseconds wait_for_packet(const SOCKET& sd, std::chrono::milliseconds const& maxTime)
	{
		fd_set set;
		FD_ZERO(&set);
		FD_SET(sd, &set);

		const auto timeout{ make_timeout(Global.select_timeout) };
		const auto t0{ std::chrono::steady_clock::now() };
		for (auto elapsed{ t0 - std::chrono::steady_clock::now() }; elapsed < maxTime; elapsed = t0 - std::chrono::steady_clock::now()) {
			if (SELECT(sd + 1ull, &set, nullptr, nullptr, &timeout) == 1) {
				return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
			}
		}
		return maxTime;
	}
}