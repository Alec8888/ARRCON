#include "mode.hpp"
#include "config.hpp"
#include "arg-handlers.hpp"

#include <fileio.hpp>

#include <signal.h>
#include <unistd.h>
#undef read		///< fileio.hpp compat
#undef write	///< fileio.hpp compat

/// @brief	Emergency stop handler, should be passed to the std::atexit() function to allow a controlled shutdown of the socket.
inline void safeExit(void)
{
	if (Global.socket != SOCKET_ERROR)
		net::close_socket(Global.socket);
	WSACleanup();
}

/**
 * @brief			Reads the target file and returns a vector of command strings for each valid line.
 * @param filename	Target Filename
 * @returns			std::vector<std::string>
 */
inline std::vector<std::string> read_script_file(std::string filename)
{
	if (!file::exists(filename)) // if the filename doesn't exist, try to resolve it from the PATH
		filename = env::PATH{}.resolve(filename, { "", ".ini", ".txt", ".bat", ".scr" });
	if (!file::exists(filename)) // if the resolved filename still doesn't exist, throw
		std::cerr << sys::term::warn << "Couldn't find file: \"" << filename << "\"\n";
	// read the file, parse it if the stream didn't fail
	else if (auto file{ file::read(filename) }; !file.fail()) {
		std::vector<std::string> commands;
		commands.reserve(file::count(file, '\n'));

		for (std::string line{}; str::getline(file, line, '\n'); )
			if (line = str::strip_line(line, "#;"); !line.empty())
				commands.emplace_back(line);

		commands.shrink_to_fit();
		return commands;
	}
	else throw std::exception(("IO Error Reading File: \""s + filename + "\""s).c_str());
	return{};
}

inline void sighandler(int sig)
{
	Global.connected = false;
	switch (sig) {
	case SIGINT:
		throw std::exception("Received SIGINT");
	case SIGTERM:
		throw std::exception("Received SIGTERM");
	case SIGABRT_COMPAT: [[fallthrough]];
	case SIGABRT:
		throw std::exception("Received SIGABRT");
	default:break;
	}
}

/**
 * @brief		Retrieves a list of all user-specified commands to be sent to the RCON server, in order.
 * @param args	All commandline arguments.
 * @returns		std::vector<std::string>
 */
inline std::vector<std::string> get_commands(const opt::ParamsAPI2& args)
{
	std::vector<std::string> vec{ args.typegetv_all<opt::Parameter>() }; // Arg<std::string> is implicitly convertable to std::string
	vec.reserve(vec.size() + Global.scriptfiles.size());
	for (auto& file : Global.scriptfiles) {// iterate through all user-specified files
		const auto sz{ Global.scriptfiles.size() };
		for (auto& command : read_script_file(file)) // add each command from the file
			vec.emplace_back(command);
		if (!Global.quiet && Global.scriptfiles.size() > sz)
			std::cout << sys::term::log << "Successfully read commands from \"" << file << "\"";
	}
	vec.shrink_to_fit();
	return vec;
}

int main(int argc, char** argv, char** envp)
{
	try {
		std::cout << sys::term::EnableANSI; // enable ANSI escape sequences on windows

		const opt::ParamsAPI2 args{ argc, argv, 'H', "host", 'P', "port", 'p', "password", 'd', "delay", 'f', "file" }; // parse arguments
		const auto& [prog_path, prog_name] { env::PATH{}.resolve_split(args.arg0().value()) };

		if (Global.ini_path = prog_path + '/' + std::filesystem::path(prog_name).replace_extension(".ini").generic_string(); file::exists(Global.ini_path))
			config::apply_config(Global.ini_path);

		const auto& [host, port, pass] { get_server_info(args) };
		handle_args(args, prog_name);

		// Register the cleanup function before connecting the socket
		std::atexit(&safeExit);

	#ifdef OS_WIN
		signal(SIGINT, &sighandler);
		signal(SIGTERM, &sighandler);
		signal(SIGABRT, &sighandler);
	#else
		struct sigaction sa { &sighandler, nullptr, {}, SA_RESTART, nullptr };
		sigaction(SIGINT, &sa, nullptr);
		sigaction(SIGTERM, &sa, nullptr);
		sigaction(SIGABRT, &sa, nullptr);
	#endif

		// Connect the socket
		Global.socket = net::connect(host, port);

		if (Global.connected = Global.socket != SOCKET_ERROR; !Global.connected)
			throw std::exception(("Failed to connect to "s + host + ":"s + port).c_str()); // connection failed

		// auth & commands
		if (rcon::authenticate(Global.socket, pass)) {
			if (mode::commandline(get_commands(args)) == 0ull || Global.force_interactive)
				mode::interactive(Global.socket, (Global.custom_prompt.empty() ? ("RCON@"s + host) : Global.custom_prompt));
			else std::cerr << sys::term::error << "No commands were executed." << std::endl;
		}
		else throw std::exception(("Authentication Failed! ("s + host + ":"s + port + ")"s).c_str());

		return EXIT_SUCCESS;
	} catch (const std::exception& ex) { ///< catch std::exception
		std::cerr << sys::term::error << ex.what() << std::endl;
	} catch (...) { ///< catch all other exceptions
		std::cerr << sys::term::error << "An unknown exception occurred!" << std::endl;
	}
	return EXIT_FAILURE;
}