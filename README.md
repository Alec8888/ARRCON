# ARRCON - Another __RCON__ Client
Remote console client that is compatible with any game using the Source Rcon Protocol, based on [mcrcon](https://github.com/Tiiffi/mcrcon) by Tiiffi.  

### Features
  - Works for any game using the [Source RCON Protocol](https://developer.valvesoftware.com/wiki/Source_RCON_Protocol).
  - Supports Minecraft Bukkit terminal colors using the section sign `§`.
  - Can be used as a one-off from the commandline, or in an interactive shell.
  - Supports saving a server's hostname/IP, Port, and Password in a config file similar to OpenSSH.
  - Handles large packets without issue.
  - Handles multi-packet responses, and allows a configurable delay to account for network latency.
  - Cross-Platform:
    - Windows
    - Linux
    - _Should be compatible with macOS, see [here](https://github.com/radj307/ARRCON/wiki/Building-from-Source)_


# Installation
No installation is required, simply download the executable and put it somewhere on your [path](https://github.com/radj307/ARRCON/wiki/Adding-To-Path).  

Alternatively, see [Build From Source](https://github.com/radj307/ARRCON/wiki/Building-from-Source) to build it yourself.


# Usage
Open a terminal in the location where you placed the executable, and run `arrcon --help` for usage instructions.  
Additional documentation is available below, but it may be out of date.  

## Modes
The operation mode is selected based on context, but can be influenced by some options.  
There are 2 modes:
- ___Interactive___
  - Used by default when there are no command arguments.
  - Opens an interactive console session. You can send commands and view the responses.
- ___Commandline___
  - Executes a list of commands in order with a configurable delay between sending each packet.
  - You can also use the `-f <filepath>` or `--file <filepath>` options to specify a scriptfile, which is executed line-by-line after any commands passed on the commandline.

## Options
Options can be specified anywhere on the commandline, and must use a dash `-` delimiter.  
| Commandline Option / Flag   | Description                                              |
|-----------------------------|:---                                                      |
|`-H <Address>`               | Specify the RCON server's IP address or hostname.
|`-P <Port>`                  | Specify the RCON server's port number.
|`-p <Password>`              | Specify the RCON server's authentication password.
|`-f <File>` `--file <file>`  | Load the specified file and run each line as a command. You can specify this argument multiple times to include multiple files.
|`-h` `--help`                | Shows the help display, including a brief description of the program & option documentation, then exits.
|`-v` `--version`             | Prints the program name & current version number, then exits.  If the quiet option is specified, only the version number is printed.
|`-i` `--interactive`         | Starts an interactive session after executing any commands specified on the commandline.
|`-d <ms>` `--delay <ms>`     | Set the delay (in milliseconds) between sending each command packet when using commandline mode.
|`-q` `--quiet`               | Prevents server response packets from being displayed, but does not silence errors or exception messages.
|`-n` `--no-color`            | Disables colorized terminal output.
| `--no-prompt`               | Hides the prompt (Default prompt is "RCON@HOST> ").
| `--write-ini`               | (Over)Writes the default `.ini` configuration file.
| `--save-host <name>`        | Save the current target to the `.hosts` config as `<name>`. You can recall it later with `ARRCON <name>`

For example, to connect to _MyHostname:27015_ using _myPassword_ as the password, send the _help_ command, then start an interactive session:  
`ARRCON -i -H MyHostname -P 27015 -p myPassword help`  


# Config Files
ARRCON will look for configuration files in the following locations, stopping at the first one that it finds:
1. An Environment Variable named `ARRCON_CONFIG_DIR`.  
  ___Note: The name is dependent on the uppercase name of the executable, keep this in mind if you want to rename it!___  
  For example, if you renamed the executable `rcon` or `rcon.exe`, then it would check for `RCON_CONFIG_DIR`
2. Depending on your operating system, the default directory is:  
  __Windows:__ The directory where `ARRCON.exe` is located.  
  __Linux:__ `/usr/local/etc`  

All config files use INI syntax, and must have the same *filename* as the executable, and either a `.ini` or `.hosts` extension.  
By default this is `ARRCON.ini` & `ARRCON.hosts` respectively.

## INI Configuration File
You can create a default INI file by using the `--write-ini` option.  

This is the default INI file, with comments to explain what each setting does.
```ini
[target]
sDefaultHost =                ; Defines the default hostname/IP, unless a hostname/IP was specified on the commandline.
sDefaultPort =                ; Defines the default port, unless a port was specified on the commandline.
sDefaultPass =                ; Defines the default password, unless a password was specified on the commandline.
bAllowNoArgs = false          ; If no arguments are specified and this is false, the help display is shown and the program will exit before connecting to the above target.

[appearance]
bDisablePrompt = false        ; When true, this is the equivalent of always specifying the --no-prompt option.
bDisableColors = false        ; When true, this is the equivalent of always specifying the -n/--no-color option.
sCustomPrompt =               ; Accepts a string to use in place of the default prompt, excluding the end, which is always "> ".
bEnableBukkitColors = false   ; Enables support for Bukkit's color syntax. This is only relevant for minecraft servers running Bukkit.

[timing]
iCommandDelay = 0             ; The delay in milliseconds between sending each command when using commandline/scriptfile mode.
iReceiveDelay = 10            ; The time in milliseconds to wait after receiving packets. Raise this if multi-packet responses aren't fully received. 
iSelectTimeout = 500          ; The max amount of time to wait for packets before timing out. Raise this if your network is slow.
```

## Host Configuration File
As of version 3.0.0, you can save any number of targets in a _.hosts_ file using a friendly name, similar to SSH's config file.  
This allows you to connect to a target with one word, rather than setting `-H`, `-P`, & `-p` every time.  
If a host/port/password is specified on the commandline anyway, that value will override the one from the hosts file.  

Also added in v3.0.0 is the `--save-host <name>` option, which appends the current target information to the hosts file as `<name>`.  

__Example:__  
`ARRCON -H 192.168.1.100 -P 27015 -p password --save-host myServer` would create this entry:
```ini
[myServer]
sHost = 192.168.1.100
sPort = 27015
sPass = password
```
You can then use the `-H`|`--host` option to recall the saved target.  
Using `ARRCON -H myServer` would now be equivalent to `ARRCON -H 192.168.1.100 -P 27015 -p password`
