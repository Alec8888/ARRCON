/**
 * @file	utils.hpp
 * @author	radj307
 * @brief	Contains various objects & functions used by ARRCON that I didn't want to include in <main.cpp>.
 */
#pragma once
#include "globals.h"
#include "copyright.h"
#include "config.hpp"			///< INI functions
#include "exceptions.hpp"
#include "net/objects/HostInfo.hpp"

#include <filei.hpp>
#include <fileutil.hpp>
#include <hasPendingDataSTDIN.h>
#include <str.hpp>
#include <opt3.hpp>
#include <term.hpp>
#include <envpath.hpp>
#include <INIRedux.hpp>

#include <iostream>
#include <vector>
#include <string>

#undef read
#undef write

 /**
  * @struct	Help
  * @brief	Functor that prints out the help display.
  */
struct Help {
private:
	const std::string _program_name;

public:
	Help(const std::filesystem::path& program_name) : _program_name{ program_name.generic_string() } {}
	friend std::ostream& operator<<(std::ostream& os, const Help& help)
	{
		return os << DEFAULT_PROGRAM_NAME << " v" << ARRCON_VERSION_EXTENDED << " (" << ARRCON_COPYRIGHT << ")\n"
			<< "  A robust & powerful commandline Remote-CONsole (RCON) client designed for use with the Source RCON Protocol.\n"
			<< "  It is also compatible with similar protocols such as the one used by Minecraft.\n"
			<< '\n'
			<< "  Report compatibility issues here: https://github.com/radj307/ARRCON/issues/new?template=support-request.md\n"
			<< '\n'
			<< "USAGE:" << '\n'
			<< "  " << help._program_name << " [OPTIONS] [COMMANDS]\n"
			<< '\n'
			<< "  Some arguments take additional inputs, labeled with <angle brackets>." << '\n'
			<< "  Arguments that contain spaces must be enclosed with single (\') or double(\") quotation marks." << '\n'
			<< '\n'
			<< "TARGET SPECIFIER OPTIONS:\n"
			<< "  -H, --host  <Host>          RCON Server IP/Hostname.  (Default: \"" << Global.DEFAULT_TARGET.hostname << "\")" << '\n'
			<< "  -P, --port  <Port>          RCON Server Port.         (Default: \"" << Global.DEFAULT_TARGET.port + "\")" << '\n'
			<< "  -p, --pass  <Pass>          RCON Server Password." << '\n'
			<< "  -S, --saved <Host>          Use a saved host's connection information, if it isn't overridden by arguments." << '\n'
			<< "      --save-host <H>         Create a new saved host named \"<H>\" using the current [Host/Port/Pass] value(s)." << '\n'
			<< "      --remove-host <H>       Remove an existing saved host named \"<H>\" from the list, then exit." << '\n'
			<< "  -l, --list-hosts            Show a list of all saved hosts, then exit." << '\n'
			<< '\n'
			<< "OPTIONS:\n"
			<< "  -h, --help                  Show the help display, then exit." << '\n'
			<< "  -v, --version               Print the current version number, then exit." << '\n'
			<< "  -q, --quiet                 Silent/Quiet mode; prevents or minimizes console output." << '\n'
			<< "  -i, --interactive           Starts an interactive command shell after sending any scripted commands." << '\n'
			<< "  -w, --wait <ms>             Wait for \"<ms>\" milliseconds between sending each command in mode [2]." << '\n'
			<< "  -n, --no-color              Disable colorized console output." << '\n'
			<< "  -Q, --no-prompt             Disables the prompt in interactive mode, and command echo in commandline mode." << '\n'
			<< "      --print-env             Prints all recognized environment variables, their values, and descriptions." << '\n'
			<< "      --write-ini             (Over)write the INI file with the default configuration values & exit." << '\n'
			<< "      --update-ini            Writes the current configuration values to the INI file, and adds missing keys." << '\n'
			<< "  -f, --file <file>           Load the specified file and run each line as a command." << '\n'
			;
	}
};

/**
 * @brief			Resolve the target server's connection information with the User's inputs.
 * @param args		Commandline argument container. The following options are checked:
 *\n				[-S|--saved]
 *\n				[-H|--host]
 *\n				[-P|--port]
 *\n				[-p|--pass]
 * #param saved		The user's list of saved connection targets, if one exists.
 * @returns			HostInfo
 *\n				This contains the resolved connection information of the target server.
 */
inline net::HostInfo resolveTargetInfo(const opt3::ArgManager& args, const net::HostList& saved = {})
{
	// Argument:  [-S|--saved]
	if (const auto savedArg{ args.getv_any<opt3::Flag, opt3::Option>('S', "saved") }; savedArg.has_value()) {
		if (const auto it{ saved.find(savedArg.value()) }; it != saved.end()) {
			return std::move(net::HostInfo{ it->second, Global.DEFAULT_TARGET }.moveWithOverrides(
				args.getv_any<opt3::Flag, opt3::Option>('H', "host"),
				args.getv_any<opt3::Flag, opt3::Option>('P', "port"),
				args.getv_any<opt3::Flag, opt3::Option>('p', "pass")
			));
		}
		else throw make_exception("There is no saved target named ", Global.palette.set_or(Color::YELLOW, '\"'), savedArg.value(), Global.palette.reset_or('\"'), " in the hosts file!");
	}
	else return{
		args.getv_any<opt3::Flag, opt3::Option>('H', "host").value_or(Global.DEFAULT_TARGET.hostname),
		args.getv_any<opt3::Flag, opt3::Option>('P', "port").value_or(Global.DEFAULT_TARGET.port),
		args.getv_any<opt3::Flag, opt3::Option>('p', "pass").value_or(Global.DEFAULT_TARGET.password)
	};
}

/**
 * @brief			Reads the target file and returns a vector of command strings for each valid line.
 * @param filename	Target Filename
 * @param pathvar	The value of the PATH environment variable as a PATH utility object.
 * @returns			std::vector<std::string>
 *\n				Each index contains a command in the file.
 */
inline std::vector<std::string> read_script_file(std::string filename, const env::PATH& pathvar)
{
	if (!file::exists(filename)) // if the filename doesn't exist, try to resolve it from the PATH
		filename = pathvar.resolve(filename, { ".txt" }).generic_string();
	if (!file::exists(filename)) // if the resolved filename still doesn't exist, print warning
		std::cerr << term::get_warn() << "Couldn't find file: \"" << filename << "\"\n";
	// read the file, parse it if the stream didn't fail
	else if (auto fss{ file::read(filename) }; !fss.fail()) {
		std::vector<std::string> commands;
		commands.reserve(file::count(fss, '\n') + 1ull);

		for (std::string lnbuf{}; std::getline(fss, lnbuf, '\n'); )
			if (lnbuf = str::strip_line(lnbuf, "#;"); !lnbuf.empty())
				commands.emplace_back(lnbuf);

		commands.shrink_to_fit();
		return commands;
	}
	return{};
}
/**
 * @brief			Retrieves a list of all user-specified commands to be sent to the RCON server, in order.
 * @param args		All commandline arguments.
 * @param pathvar	The value of the PATH environment variable as a PATH utility object.
 * @returns			std::vector<std::string>
 */
inline std::vector<std::string> get_commands(const opt3::ArgManager& args, const env::PATH& pathvar)
{
	std::vector<std::string> commands{ args.getv_all<opt3::Parameter>() }; // Arg<std::string> is implicitly convertable to std::string

	// Check for piped data on STDIN
	if (hasPendingDataSTDIN()) {
		for (std::string ln{}; str::getline(std::cin, ln, '\n'); ) {
			ln = str::strip_line(ln); // remove preceeding & trailing whitespace
			if (!ln.empty())
				commands.emplace_back(ln); // read all available lines from STDIN into the commands list
		}
	}

	// read all user-specified files
	for (const auto& file : Global.scriptfiles) {
		if (const auto script_commands{ read_script_file(file, pathvar) }; !script_commands.empty()) {
			if (!Global.quiet) // feedback
				std::cout << term::get_log(!Global.no_color) << "Successfully read commands from \"" << file << "\"\n";

			commands.reserve(commands.size() + script_commands.size());

			for (const auto& command : script_commands)
				commands.emplace_back(command);
		}
		else std::cerr << Global.palette.get_warn() << "Failed to read any commands from \"" << file << "\"\n";
	}
	commands.shrink_to_fit();
	return commands;
}

#pragma region ArgumentHandlers
inline void handle_hostfile_arguments(const opt3::ArgManager& args, net::HostList& hosts, const std::filesystem::path& hostfile_path)
{
	bool do_exit{ false };
	// remove-host
	if (const auto remove_hosts{ args.getv_all<opt3::Option>("remove-host") }; !remove_hosts.empty()) {
		do_exit = true;
		std::stringstream message_buffer; // save the messages in a buffer to prevent misleading messages in the event of a file writing error
		for (const auto& name : remove_hosts) {
			if (hosts.erase(name))
				message_buffer << Global.palette.get_msg() << "Removed " << Global.palette(Color::YELLOW, '\"') << name << Global.palette('\"') << '\n';
			else
				message_buffer << term::get_error(!Global.no_color) << "Hostname \"" << Global.palette(Color::YELLOW, '\"') << name << Global.palette('\"') << " doesn't exist!" << '\n';
		}

		// if the hosts file is empty, delete it
		if (hosts.empty()) {
			if (Global.autoDeleteHostlist) {
				if (std::filesystem::remove(hostfile_path))
					std::cout << message_buffer.rdbuf() << Global.palette.get_msg() << "Deleted the hostfile as there are no remaining entries." << std::endl;
				else
					throw permission_exception("handle_hostfile_arguments()", hostfile_path, "Failed to delete empty Hostfile!");
			}
			std::exit(EXIT_SUCCESS); // host list is empty, ignore do_list_hosts as nothing will happen
		} // otherwise, save the modified hosts file.
		else {
			if (config::save_hostfile(hosts, hostfile_path)) // print a success message or throw failure exception
				std::cout << message_buffer.rdbuf() << Global.palette.get_msg() << "Successfully saved modified hostfile " << hostfile_path << std::endl;
			else
				throw permission_exception("handle_hostfile_arguments()", hostfile_path, "Failed to write modified Hostfile to disk!");
		}
	}
	// save-host
	if (const auto save_host{ args.getv<opt3::Option>("save-host") }; save_host.has_value()) {
		do_exit = true;
		std::stringstream message_buffer; // save the messages in a buffer to prevent misleading messages in the event of a file writing error

		const file::INI::SectionContent target_info{ Global.target };
		const auto& [existing, added] {
			hosts.insert(std::make_pair(save_host.value(), target_info))
		};

		if (added)
			message_buffer << Global.palette.get_msg() << "Added host: " << Global.palette(Color::YELLOW, '\"') << save_host.value() << Global.palette.reset_or('\"') << " " << Global.target.hostname << ':' << Global.target.port << '\n';
		else if ([](const file::INI::SectionContent& left, const file::INI::SectionContent& right) -> bool {
			if (const auto left_host{ left.find("sHost") }, right_host{ right.find("sHost") }; left_host == left.end() || right_host == right.end() || left_host->second != right_host->second) {
				return false;
			}
			if (const auto left_port{ left.find("sPort") }, right_port{ right.find("sPort") }; left_port == left.end() || right_port == right.end() || left_port->second != right_port->second) {
				return false;
			}
			if (const auto left_pass{ left.find("sPass") }, right_pass{ right.find("sPass") }; left_pass == left.end() || right_pass == right.end() || left_pass->second != right_pass->second) {
				return false;
			}
			return true;
			}(existing->second, target_info))
			throw make_exception("Host ", Global.palette(Color::YELLOW, '\"'), save_host.value(), Global.palette('\"'), " is already set to ", Global.target.hostname, ':', Global.target.port, '\n');
		else
			message_buffer << Global.palette.get_msg() << "Updated " << Global.palette(Color::YELLOW, '\"') << save_host.value() << Global.palette('\"') << ": " << Global.target.hostname << ':' << Global.target.port << '\n';


		if (config::save_hostfile(hosts, hostfile_path)) // print a success message or throw failure exception
			std::cout << message_buffer.rdbuf() << Global.palette.get_msg() << "Successfully saved modified hostlist to " << hostfile_path << std::endl;
		else
			throw permission_exception("handle_hostfile_arguments()", hostfile_path, "Failed to write modified Hostfile to disk!");
	}
	// list all hosts
	if (args.check_any<opt3::Flag, opt3::Option>('l', "list-hosts")) {
		do_exit = true;
		if (hosts.empty()) {
			std::cerr << "There are no saved hosts in the list." << std::endl;
			std::exit(EXIT_SUCCESS);
		}

		const auto indentation_max{ [&hosts]() {
			if (Global.quiet)
				return 0ull; // don't process the list if this won't be used
			size_t longest{ 0ull };
			for (const auto& [name, _] : hosts)
				if (const auto sz{ name.size() }; sz > longest)
					longest = sz;
			return longest + 2ull;
		}() };

		for (const auto& [name, info] : hosts) {
			const net::HostInfo& hostinfo{ info };
			if (!Global.quiet) {
				std::cout
					<< Global.palette(Color::YELLOW, '\"') << name << Global.palette('\"') << '\n'
					<< "    Host:  " << hostinfo.hostname << '\n'
					<< "    Port:  " << hostinfo.port << '\n';
			}
			else {
				std::cout << Global.palette(Color::YELLOW, '\"') << name << Global.palette('\"')
					<< shared::indent(indentation_max, name.size()) << "( " << hostinfo.hostname << ':' << hostinfo.port << " )\n";
			}
		}
	}
	if (do_exit)
		std::exit(EXIT_SUCCESS);
}
#pragma endregion ArgumentHandlers
