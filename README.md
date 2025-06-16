# Unreal MCP Server Plugin

**Version:** 0.1.0 (Beta)
**Created By:** Hi-Rez Studios

## Overview

The Unreal MCP Server is an Unreal Engine plugin designed to implement an MCP (Model Context Protocol) server. Its primary goal is to facilitate interaction between AI agents and the Unreal Engine environment by exposing engine functionalities.

This plugin allows external applications, particularly AI agents, to query and potentially manipulate aspects of an Unreal Engine project in real-time.

## Features

*   **TCP Server:** The plugin runs a TCP server, typically on port **30069** (this should be verified within the plugin's configuration or source if different).
*   **JSON-Based Commands:** Communication with the server is handled via JSON-formatted messages.
*   **Extensible Command Set:** Designed to support various commands for interacting with Unreal Engine. An initial example command is `get_blueprint_contents`, which retrieves the T3D representation of a Blueprint.
*   **Editor Integration:** The plugin module is configured to run in the Editor (`"Type": "Editor"`).

## Getting Started

### Prerequisites

*   Unreal Engine (version compatibility should be checked with your specific engine version).
*   A C++ toolchain compatible with your Unreal Engine version for compiling the plugin if you are building from source.

### Installation

1.  **Clone or Download:**
    *   Place the `UnrealMCPServer` plugin folder into your Unreal Engine project's `Plugins` directory. For example: `YourProject/Plugins/UnrealMCPServer`.
    *   If your project doesn't have a `Plugins` folder, create one at the root of your project directory.

2.  **Enable the Plugin:**
    *   Open your Unreal Engine project.
    *   Go to `Edit -> Plugins`.
    *   Search for "Unreal MCP Server" (it should appear under the "MCPServer" category or "Project -> Other").
    *   Ensure the "Enabled" checkbox is ticked.
    *   You may need to restart the Unreal Editor.

3.  **Compile (if necessary):
    *   If you added the plugin to a C++ project, Unreal Engine might prompt you to recompile the project. Allow it to do so.
    *   If you are using a Blueprint-only project, you might need to convert it to a C++ project first (File -> New C++ Class... -> None -> Create Class, then close the editor and re-open the .sln to build).

### Usage

1.  **Server Activation:** Once the plugin is enabled and the editor/project is running, the TCP server should automatically start listening on the configured port (e.g., 30069).
2.  **Client Connection:**
    *   Develop a client application (e.g., using the companion `hirez_mcp_client` Python library or any other TCP client).
    *   Connect the client to the Unreal Engine instance at `localhost:30069` (or the appropriate IP/port if configured differently).
3.  **Sending Commands:**
    *   Send JSON-formatted commands to the server.
    *   Example (conceptual) for `get_blueprint_contents`:
        ```json
        {
          "command": "get_blueprint_contents",
          "parameters": {
            "blueprint_path": "/Game/Path/To/Your/Blueprint.Blueprint"
          }
        }
        ```
4.  **Receiving Responses:** The server will respond with JSON-formatted data.

## Repository Structure

*   `Source/`: Contains the C++ source code for the plugin.
    *   `UnrealMCPServer/`: Main module for the MCP server logic.
*   `Config/`: Configuration files for the plugin.
*   `Binaries/`: Compiled binaries (platform-specific).
*   `Intermediate/`: Temporary files generated during compilation.
*   `UnrealMCPServer.uplugin`: The plugin descriptor file, containing metadata about the plugin.
*   `LICENSE`: Contains the licensing information for this plugin.
*   `README.md`: This file.

## Current Status

This plugin is currently in **Beta**. Features and APIs might change.

## Contributing

Details on contributing, reporting issues, or feature requests will be provided as the project matures. For now, please refer to the project's issue tracker if available.

## License

This project is licensed under the terms specified in the `LICENSE` file.
