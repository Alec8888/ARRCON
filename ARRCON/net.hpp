/**
 * @file	net.hpp
 * @author	Tiiffi  ;  Heavily modified & updated for C++20 by radj307.
 * @brief	Contains all of the raw networking functions used by the rcon namespace.
 *\n		These functions were originally created by Tiiffi for mcrcon: (https://github.com/Tiiffi/mcrcon)
 *\n		This is the original license:
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
#include "packet.hpp"

#include <TermAPI.hpp>
#include <make_exception.hpp>

#include <optional>
#include <string>
#include <sys/socket.h>
#include <netdb.h>

#ifndef OS_WIN
#include <cstring>
#endif

 /**
  * @namespace	net
  * @brief		Contains functions used to interact with sockets.
  */
namespace net {
	/**
	 * @brief	Call WSAStartup & initialize the winsock dll.
	 *\n		Only necessary on Windows, as the Linux socket library
	 *			performs the startup & cleanup operations automatically.
	 */
	inline void init(void) noexcept(false)
	{
	#ifdef _WIN32
		WSADATA wsaData;
		int rc{ 0 };
		if (rc = WSAStartup(WINSOCK_VERSION, &wsaData); rc != 0 || LOBYTE(wsaData.wVersion) != LOBYTE(WINSOCK_VERSION) || HIBYTE(wsaData.wVersion) != HIBYTE(WINSOCK_VERSION))
			throw std::exception("WSAStartup failed with error code " + rc);
	#else

	#endif
	}

	/**
	 * @brief		Close the socket & call WSACleanup on windows.
	 * @param sd	The socket descriptor to close.
	 */
	inline void close_socket(const SOCKET& sd)
	{
	#ifdef _WIN32
		closesocket(sd);
		WSACleanup();
	#endif
	}

	/// @brief	Emergency stop handler, should be passed to the std::atexit() function to allow a controlled shutdown of the socket.
	inline void cleanup(void)
	{
		net::close_socket(Global.socket);
		std::cout << Global.palette.reset();
	}

	/**
	 * @brief	Retrieve the last reported socket error code.
	 * @returns	int
	 */
	inline int lastError()
	{
	#ifdef OS_WIN
		return WSAGetLastError();
	#else
		return -1;
	#endif
	}

	/**
	 * @brief		Connect the socket to the specified RCON server.
	 * @author		Tiiffi
	 * @param host	Target server IP address or hostname.
	 * @param port	Target server port.
	 * @returns		SOCKET
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

		int ret = getaddrinfo(host.c_str(), port.c_str(), &hints, &server_info);
		if (ret != 0)
			throw make_exception("Name resolution of \"", host, ':', port, "\" failed with error code ", lastError(), '!');

		// Go through the hosts and try to connect
		for (p = server_info; p != NULL; p = p->ai_next) {
			sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

			if (sd == -1)
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
			throw make_exception("Failed to connect to ", host, ':', port, "!");

		return sd;
	}

	/**
	 * @brief			Send a packet to the specified socket.
	 * @param sd		Socket to use.
	 * @param packet	Packet to send to the server.
	 * @returns			bool
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
	 * @brief		Flush all remaining packets.
	 * @param sd	Target Socket.
	 */
	inline void flush(const SOCKET& sd, const bool& do_check_first = true)
	{
		fd_set set{ 1u, sd };
		const auto timeout{ make_timeout(Global.select_timeout) };
		if (do_check_first && SELECT(sd + 1ull, &set, nullptr, nullptr, &timeout) != 1)
			return;
		do {
			if (recv(sd, std::unique_ptr<char>{}.get(), packet::PSIZE_MAX, 0) == 0)
				throw make_exception("Connection Lost! Last Error: ", lastError());
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

		if (auto ret{ recv(sd, (char*)&psize, sizeof(int), 0) }; ret == 0)
			throw make_exception("Connection Lost! Last Error: " + lastError());
		else if (ret != sizeof(int)) {
			std::cerr << term::warn << "Received a corrupted packet! Code " << ret << '\n';
			return{};
		}

		if (psize < packet::PSIZE_MIN) {
			std::cerr << term::warn << "Received unexpectedly small packet size: " << psize << std::endl;
		}
		else if (psize > packet::PSIZE_MAX) {
			std::cerr << term::warn << "Received unexpectedly large packet size: " << psize << std::endl;
			flush(sd);
		}

		packet::serialized_packet spacket{ psize, 0, 0, { 0x00 } }; ///< create a serialized packet to receive data

		for (int received{ 0 }, ret{ 0 }; received < psize; received += ret) {
			ret = recv(sd, (char*)&spacket + sizeof(int) + received, static_cast<size_t>(psize) - received, 0);
			if (ret == 0)
				throw make_exception("Connection Lost! Last Error: ", lastError());
		}

		return { spacket };
	}
}