easydabctl
Minimal emergency command-line controller for EasyDAB v2 modulators.

easydabctl is a small C++17 program using libcurl. It reads basic status and performs selected runtime control operations over the EasyDAB's built-in HTTP interface, without needing a browser, PHP, or a running web UI. It is intended for technicians who need to diagnose or recover an EasyDAB from an SSH session.
Project status: experimental / early release. The command encoding and binary-response parsing were derived from EasyDAB v2 web UI sources dated 2016 and compared with observed responses from a device reporting easydabv2.6144-28.11.2017. The CLI has been compiled on Linux, but live write operations have not been validated by this project. Check behavior with your hardware and firmware before relying on it in production.

Features
- Read RF frontend state, ETI socket state, ETI buffer fullness, broadcasting flag, external SPI SRAM detection, iFFT overflow, and FIR clipping.
- Read current DAC amplitude (register value, not RF power in watts).
- Read non-secret TCP/ETI configuration details: remote IP/port, mode, ZeroMQ handshake flag, and TCP_NODELAY flag.
- Manually enable/disable RF, control ETI client/server/socket, reconnect a configured ETI TCP client, set the runtime DAC amplitude, and restart the device.
- Optional JSON status output for shell scripting and monitoring.
- Prompt for the EasyDAB HTTP password securely from the terminal; support a named environment variable for unattended use.
- Require explicit --yes for all commands that change device state.
Intentionally not supported: flash erase/write, persistent configuration editing, RF frequency changes, and undocumented commands.
Requirements
- Linux (including Debian / Raspberry Pi OS on Raspberry Pi 3 or later)
- C++17 compiler, GNU Make
- libcurl development headers and library
- Network access to the EasyDAB's HTTP port (usually TCP/80)
On Debian / Raspberry Pi OS:
sudo apt update
sudo apt install g++ make libcurl4-openssl-dev
Build and install
git clone https://github.com/YOUR_GITHUB_USERNAME/easydabctl.git
cd easydabctl
make
sudo install -m 0755 easydabctl /usr/local/bin/easydabctl
Replace the example clone URL with the actual repository URL after publishing. Alternatively, extract the source release archive and run make in its project directory. Compile on the target Raspberry Pi; the supplied source archive does not require a prebuilt executable.

Check the command summary:
easydabctl --help
Quick start
The current default device address is 10.26.76.125 and the default HTTP username is admin. For other devices, provide --host and optionally --user:
# Read-only diagnostics
easydabctl --host 192.168.1.50 status
easydabctl --host 192.168.1.50 --raw status
easydabctl --host 192.168.1.50 --json status
easydabctl --host 192.168.1.50 amplitude
easydabctl --host 192.168.1.50 config
The program normally asks for the HTTP password using /dev/tty without echoing it. It does not accept a password directly as a CLI argument.
Runtime control
Warning: The following commands affect a real transmitter. An enabled RF frontend can drive an external PA. Verify the antenna system or correctly rated 50-ohm dummy load, PA interlocks, and local operating authorization first.
# Device-changing operations; --yes is mandatory
easydabctl --host 192.168.1.50 --yes rf off
easydabctl --host 192.168.1.50 --yes rf on
easydabctl --host 192.168.1.50 --yes eti off
easydabctl --host 192.168.1.50 --yes eti client
easydabctl --host 192.168.1.50 --yes eti server
easydabctl --host 192.168.1.50 --yes eti reconnect
easydabctl --host 192.168.1.50 --yes amplitude set 122
easydabctl --host 192.168.1.50 --yes restart
- eti client and eti server activate runtime socket modes; they do not change persistent flash configuration.
- eti reconnect checks that the saved configuration identifies TCP Client mode (0x04) before sending its runtime activation command. It is not an unconditional disconnect-and-reconnect sequence.
- amplitude set N accepts 1–255; value 00 is reserved by this protocol for reading amplitude. The change is runtime-only and does not imply an RF power level.
- restart may terminate the HTTP connection before a response can be received. A resulting transport error does not prove the restart failed.
- HTTP 200 indicates an HTTP response, not verified RF output, successful ZeroMQ reception, or completion of an underlying hardware action.
Command-line options
Option	Meaning
--host IPv4	EasyDAB IPv4 address (default 10.26.76.125)
--user NAME	HTTP Basic Auth username (default admin)
--password-env NAME	Read password from named environment variable instead of interactive prompt
--timeout SECONDS	Total HTTP timeout, 1–30 seconds (default 3)
--json	JSON output for supported read commands
--raw	Include raw hexadecimal bytes with supported human-readable read commands
--yes	Confirm device-changing commands
--help	Show usage


Example of an unattended read:
# Set the variable securely in your own process environment first.
easydabctl --host 192.168.1.50 --password-env EASYDAB_PASSWORD --json status
Environment variables may be visible to privileged processes. Avoid writing passwords into shell history, public scripts, or logs.
Basic protocol reference
Operation	HTTP endpoint	Notes
Status	GET /s_000000	Expected 14-byte response
DAC amplitude read	GET /s_000e00	Register value at byte offset 2
Configuration read	GET /b_070000	CLI prints only selected non-secret fields
RF on / off	GET /s_020000 / GET /s_025000	Runtime control
ETI off	GET /s_010000	Runtime control
ETI TCP client / server	GET /s_010400 / GET /s_010200	Runtime control
DAC amplitude set	GET /s_000eXX	XX is hexadecimal 01–FF
Restart	GET /r_000000	Device restart


This protocol uses GET even for state-changing actions. Do not use these endpoint paths as passive monitoring probes. Different EasyDAB firmware versions may behave differently.
Status decoding
The 14-byte status reply is interpreted using byte offsets 4 (RF), 9 (ETI mode), 10 (ETI administration), 11 (TCP status), 12 (buffer fill, scaled 0..255), and 13 (flags). Flag bits include iFFT overflow, FIR clipping, broadcasting (0x40), and external SPI SRAM detected (0x80). Unknown status combinations are reported as UNKNOWN rather than guessed.
Networking behavior
The tool uses HTTP/1.0, requests Connection: close, and sets a 2-second connection timeout. It does not automatically retry state-changing commands: a timeout can occur after a command has already reached the hardware.

HTTP Basic Authentication is transmitted over unencrypted HTTP. Use only on isolated, trusted networks or over a secure VPN. This CLI has no coordination lock shared with a PHP dashboard; avoid simultaneous changes from multiple clients.
Tests
make             # Compile
make test        # Run the included mock HTTP-server tests
The mock test suite needs local loopback sockets. In restricted execution sandboxes it may be blocked. Unit/mock tests and compilation are not substitutes for live hardware qualification. Start with read-only commands and verify your firmware's status fields before performing any action.
Troubleshooting
- Connection refused / timeouts: check the EasyDAB IP, TCP/80 reachability, and whether its embedded HTTP server is temporarily refusing connections.
- ETI is not connected: check saved TCP Client / ZeroMQ configuration separately from runtime socket activation. Verify the external ETI/ZeroMQ service independently.
- Wrong response size: do not assume the output is valid; it may be a login/error page or a different firmware format.
- Restart command times out: inspect the device again after it boots; do not blindly repeat the command.
Contributing
Bug reports, tested firmware/version details, protocol corrections, and pull requests are welcome. When filing a report, include the EasyDAB firmware version, the invoked command, and redacted status data. Never post configuration dumps containing credentials. Please do not test unverified RF actions on live transmitters.
License
A license has not yet been selected. Before publishing the project as reusable open-source software, add a LICENSE file with the license chosen by the project owner (for example MIT or GPL-3.0-or-later). Until then, the repository is publicly viewable if published, but reuse rights are not automatically granted.
