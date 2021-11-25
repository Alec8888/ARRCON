#pragma once
#include "packet.hpp"

#include <TermAPI.hpp>

#include <optional>
#include <string>
#include <winsock.h>
#include <sys/socket.h>

/**
 * @namespace	net
 * @brief		Contains functions used to interact with sockets.
 */
namespace net {
#ifdef _WIN32
	inline void init_WSA(void) noexcept(false)
	{
		WSADATA wsaData;
		if (const auto rc{ WSAStartup(WINSOCK_VERSION, &wsaData) }; rc != 0 || LOBYTE(wsaData.wVersion) != LOBYTE(WINSOCK_VERSION) || HIBYTE(wsaData.wVersion) != HIBYTE(WINSOCK_VERSION))
			throw std::exception("WSAStartup failed with error code " + rc);
	}
#endif

	/**
	 * @brief		Close the socket & call WSACleanup.
	 * @param sd	The socket descriptor to close.
	 */
	inline void close_socket(const SOCKET& sd)
	{
	#ifdef _WIN32
		closesocket(sd);
		WSACleanup();
	#else
		close(sd);
	#endif
	}
	inline SOCKET connect(const std::string& host, const std::string& port)
	{
		SOCKET sd;

		struct addrinfo* server_info, * p;

		struct addrinfo hints;
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

	#ifdef _WIN32
		init_WSA();
	#endif

		int ret = getaddrinfo(host.c_str(), port.c_str(), &hints, &server_info);
		if (ret != 0) {
			std::cerr << "Name resolution failed with error code " << ret << std::endl;
			exit(EXIT_FAILURE);
		}

		// Go through the hosts and try to connect
		for (p = server_info; p != NULL; p = p->ai_next) {
			sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

			if (sd == -1)
				continue;

			ret = connect(sd, p->ai_addr, static_cast<int>(p->ai_addrlen));
			if (ret == -1) {
				close_socket(sd);
				continue;
			}
			break; // connection successful, break from loop
		}

		freeaddrinfo(server_info);

		if (p == NULL) {
			std::cerr << "Connection failed with error code " << WSAGetLastError() << std::endl;
			exit(EXIT_FAILURE);
		}

		return sd;
}

	inline int send_packet(const SOCKET& sd, const packet::rc_packet& packet)
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

		return ret == -1 ? -1 : 1;
	}

	inline int flush(const SOCKET& sd, int size)
	{
		std::unique_ptr<char> tmp;
		//char tmp[size];
		int ret = recv(sd, tmp.get(), size, 0);

		if (ret == 0) {
			fprintf(stderr, "Connection lost.\n");
			g_connected = 0;
		}

		return ret;
	}

	inline std::optional<packet::rc_packet> recv_packet(const SOCKET& sd)
	{
		int psize;

		if (int ret{ recv(sd, (char*)&psize, sizeof(int), 0) }; ret == 0) {
			std::cerr << sys::term::error << "Connection Lost!" << std::endl;
		#ifdef _WIN32
			std::cerr << "Last socket error code: " << WSAGetLastError() << std::endl;
		#endif
			return std::nullopt;
		}
		else if (ret != sizeof(int)) {
			std::cerr << "Receive operation failed because the packet size was invalid! (" << ret << ')' << std::endl;
			g_connected = 0;
			return std::nullopt;
		}

		if (psize < packet::PSIZE_MIN) {
			std::cerr << sys::term::warn << "Received unexpectedly small packet size: " << psize << std::endl;
		}
		else if (psize > packet::PSIZE_MAX) {
			std::cerr << sys::term::warn << "Received unexpectedly large packet size: " << psize << std::endl;
			flush(sd, packet::PSIZE_MAX);
		}

		packet::serialized_packet spacket{ psize, 0, 0, {0x00} }; ///< create a serialized packet to receive data

		for (int received{ 0 }, ret{ 0 }; received < psize; received += ret) {
			ret = recv(sd, (char*)&spacket + sizeof(int) + received, psize - received, 0);
			if (ret == 0) { /* connection closed before completing receving */
				fprintf(stderr, "Connection lost.\n");
				g_connected = 0;
				return std::nullopt;
			}
		}

		return { spacket };
	}
}