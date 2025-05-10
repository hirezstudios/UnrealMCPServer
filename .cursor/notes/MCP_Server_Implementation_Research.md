Implementing a Model Context Protocol (MCP) Server: A Comprehensive Guide
I. Introduction to Model Context Protocol (MCP)
A. What is MCP?
The Model Context Protocol (MCP) is an open standard designed to streamline and standardize the interactions between AI models, particularly Large Language Models (LLMs), and external systems.1 Its fundamental purpose is to provide a structured, discoverable, and consistent mechanism through which AI applications can access and utilize external data, tools, and other capabilities.2 This allows AI systems to move beyond their intrinsic knowledge and engage with dynamic, real-world information and functionalities.
MCP can be conceptualized as a "plugin system" for AI agents or, more broadly, as a "USB-C port for AI applications".1 This analogy highlights its role in enabling diverse AI models to connect with a wide array of external services without requiring bespoke integrations for each pair. The protocol was originally developed by Anthropic and subsequently open-sourced to encourage wider adoption and community contribution.3
B. Core Architectural Components: Host, Client, Server
The MCP architecture is defined by three primary components that collaborate to facilitate communication 3:
1. Host: The Host is the LLM-powered application that initiates and manages connections to MCP Servers. Examples include AI-integrated Integrated Development Environments (IDEs) like Cursor, dedicated AI assistant applications such as Claude Desktop, or custom-built AI agents and workflows. The Host is ultimately responsible for how the information and capabilities obtained via MCP are utilized by the LLM and presented to the end-user.
2. Client: The Client is a lightweight software component embedded within the Host application. It acts as a dedicated connector and intermediary between the Host and a single MCP Server. Each Client establishes and maintains a one-to-one, stateful connection with its corresponding Server. Key responsibilities of the Client include negotiating protocol versions and capabilities with the Server, and orchestrating the flow of MCP messages (requests, responses, notifications) between the Host and the Server.
3. Server (Your Implementation Focus): The Server is an independent process or service that implements the MCP specification. Its role is to expose a defined set of capabilities—such as Tools, Resources, and Prompts—to connected Clients. This guide focuses on the implementation of such an MCP Server.
The typical interaction flow begins with the Host application, which, through one of its Clients, establishes a connection with an MCP Server. The Client and Server then perform an initialization handshake to agree on protocol versions and supported capabilities. Subsequently, the Client, acting on behalf of the Host or an LLM, sends requests to the Server to access its exposed capabilities. The Server processes these requests and sends back responses.
A critical aspect for developers implementing an MCP Server is to recognize that the Server's direct counterparty is the MCP Client, not the end-user or the LLM itself. The LLM's intentions or the user's commands are translated by the Host and its Client into formal MCP requests that the Server receives. This abstraction means the Server must strictly adhere to the MCP message formats and interaction patterns, as the Client will expect conformance.
C. Fundamental Principles
MCP is built upon several core principles:
* JSON-RPC 2.0: All communication between MCP Clients and Servers is conducted using messages formatted according to the JSON-RPC 2.0 specification.2 This provides a lightweight, well-defined structure for requests, responses, and notifications.
* Stateful Connections: MCP sessions are inherently stateful. A connection, once established through an initialization handshake, maintains its state and context throughout its duration.7
* Capability Negotiation: During the initialization phase, the Client and Server explicitly declare and negotiate the features and capabilities they support.7 This ensures that both parties are aware of what operations are permissible and understood.
The authoritative sources for MCP are the official specification documents and the associated JSON schemas. The version dated 2025-03-26 is a key reference for current implementations.7
D. Scope of This Guide
This document provides a comprehensive guide for implementing an MCP Server from the ground up. It focuses on enabling a server to expose the core MCP capabilities—Tools, Resources, and Prompts—primarily using the HTTP Server-Sent Events (SSE) transport mechanism. The principles and specifications discussed are intended to be language-agnostic, assuming the implementer has access to standard HTTP server libraries in their chosen programming language. This guide does not delve into client-side implementation details beyond what is necessary to understand server interaction points, nor does it cover advanced server-initiated capabilities like Sampling or Roots in depth.
II. Core Protocol Mechanics: JSON-RPC 2.0 and MCP Lifecycle
A. JSON-RPC 2.0 Overview for MCP
The Model Context Protocol relies exclusively on JSON-RPC 2.0 for structuring all messages exchanged between clients and servers.2 A thorough understanding and precise implementation of these message formats are paramount for a compliant MCP server.
* Request Object: A JSON-RPC Request object is sent from a client to a server (or vice-versa in some MCP scenarios) to invoke a method. It MUST contain the following members 9:
   * jsonrpc: A String specifying the version of the JSON-RPC protocol. MUST be exactly "2.0".
   * method: A String containing the name of the method to be invoked (e.g., "initialize", "tools/list").
   * params: A Structured value that holds the parameter values to be used during the invocation of the method. This MAY be an Array of values or an Object of member name/value pairs. MCP typically utilizes named parameters (Object). This member MAY be omitted if the method does not require parameters.
   * id: An identifier established by the Client that MUST contain a String, Number, or NULL value if included. If it is not included it is assumed to be a notification. The Server's response (if any) MUST include the same id value. This id SHOULD be unique for concurrent requests.
* Response Object (Success): When a method invocation is successful, the receiving party (typically the server) MUST reply with a JSON-RPC Response object. This object contains 9:
   * jsonrpc: A String specifying the version of the JSON-RPC protocol, "2.0".
   * result: This member is REQUIRED on success. Its value is determined by the method invoked on the Server.
   * id: This member is REQUIRED and MUST be the same as the id in the Request object.
* Response Object (Error): When a method invocation results in an error, the Response object MUST contain an error member instead of result 9:
   * jsonrpc: A String specifying the version of the JSON-RPC protocol, "2.0".
   * error: This member is REQUIRED on error. It is an Object containing:
      * code: A Number that indicates the error type that occurred. Standard JSON-RPC error codes are integers.
      * message: A String providing a short description of the error.
      * data: A Primitive or Structured value that contains additional information about the error. This MAY be omitted.
   * id: This member is REQUIRED and MUST be the same as the id in the Request object. If there was an error in detecting the id in the Request (e.g. Parse error/Invalid Request), it MUST be Null.
Standard JSON-RPC error codes relevant to MCP server implementation include 9:
   * -32700 ParseError: Invalid JSON was received by the server. An error occurred on the server while parsing the JSON text.
   * -32600 InvalidRequest: The JSON sent is not a valid Request object.
   * -32601 MethodNotFound: The method does not exist or is not available.
   * -32602 InvalidParams: Invalid method parameter(s).
   * -32603 InternalError: Internal JSON-RPC error.
* Notification Object: A Notification is a Request object without an id member. It signifies a one-way message where no response is expected or will be sent.9
   * jsonrpc: "2.0".
   * method: A String containing the name of the method to be invoked.
   * params: A Structured value (Array or Object) for parameters. MAY be omitted.
Strict adherence to these JSON-RPC 2.0 formats is not merely a recommendation but a requirement for interoperability. Any deviation in the structure of requests parsed or responses generated by the server will likely result in communication failures with MCP clients, as these clients are built to expect precise conformance.
B. MCP Connection Lifecycle
An MCP connection progresses through distinct phases, ensuring orderly communication and capability alignment.
1. Initialization Phase (Handshake)
This is the crucial first phase where the client and server establish a common understanding of protocol versions and supported features.8
* Client sends initialize Request:
The client initiates the handshake by sending an initialize request to the server.
   * method: MUST be "initialize".
   * params: An object containing:
   * protocolVersion: A string indicating the MCP version the client wishes to use (e.g., "2025-03-26").
   * clientInfo: An object with information about the client, such as name (String) and version (String).
   * capabilities: An object detailing the MCP capabilities the client supports and may offer to the server (e.g., sampling: {} if it can handle server-initiated LLM sampling requests, or roots: { "listChanged": true } if it can provide workspace roots and notify on changes).9
   * Example initialize request params structure:
JSON
{
 "protocolVersion": "2025-03-26",
 "clientInfo": { "name": "ExampleClient", "version": "1.0.0" },
 "capabilities": {
   "sampling": {},
   "roots": { "listChanged": true }
 }
}

      * Server responds to initialize Request:
Upon receiving the initialize request, the server MUST respond.
         * result: An object containing:
         * protocolVersion: If the server supports the client's requested protocolVersion, it MUST respond with the same version. If not, it SHOULD respond with the latest version it does support. The client MAY then choose to disconnect if the versions are incompatible.17
         * serverInfo: An object with information about the server, such as name (String) and version (String).
         * capabilities: An object detailing the MCP capabilities the server offers to the client (e.g., tools: { "listChanged": true } if it provides tools and can notify on list changes, resources: { "listChanged": true, "subscribe": true } if it supports resources, list changes, and content subscriptions).9
         * Example initialize response result structure:
JSON
{
 "protocolVersion": "2025-03-26",
 "serverInfo": { "name": "MyCustomMCPServer", "version": "0.1.0" },
 "capabilities": {
   "tools": { "listChanged": true },
   "resources": { "listChanged": true, "subscribe": true },
   "prompts": { "listChanged": true }
 }
}

            * Client sends notifications/initialized Notification:
If the initialization is successful (protocol versions are compatible and capabilities exchanged), the client MUST send an initialized notification to the server.
               * method: MUST be "notifications/initialized".
               * params: Typically an empty object {} or omitted. This notification signals the completion of the handshake, and normal protocol operations can commence.8
The initialize handshake serves as a critical gateway for all subsequent MCP interactions. The server MUST correctly parse the client's declared capabilities and accurately report its own. The set of capabilities agreed upon during this phase forms a contract for the session, dictating which MCP methods and notifications are valid. For instance, a server should only send a notifications/tools/list_changed notification if the client indicated support for such notifications implicitly or if the server declared tools: { "listChanged": true } and the client accepted the connection.
2. Operation Phase
Once initialization is complete, the client and server enter the operation phase. During this phase, they exchange JSON-RPC requests, responses, and notifications corresponding to the various MCP features (Tools, Resources, Prompts) and protocol utilities, strictly adhering to the capabilities negotiated during initialization.8
3. Shutdown Phase
MCP does not define specific protocol messages for shutdown. Termination of the connection is typically handled at the transport layer.17 For instance, with HTTP SSE, the client might close the HTTP connection. For stdio transport, the client would close the server process's standard input stream. The server implementation SHOULD be prepared to handle such disconnections gracefully, releasing any associated resources.
C. Capability Negotiation In-Depth
Capability negotiation is a cornerstone of MCP's flexibility, allowing clients and servers to adapt to different feature sets.7 Both parties declare their capabilities within the capabilities object of their respective initialize messages.
The keys in the capabilities object represent feature names (e.g., "tools", "resources", "prompts" for server-exposed capabilities; "sampling", "roots" for client-exposed capabilities that a server might use). The value associated with each key indicates the level of support:
               * An empty object {} typically signifies basic support for the capability.
               * An object with boolean flags (e.g., {"listChanged": true, "subscribe": true}) indicates support for specific sub-features of that capability.14 For example, resources: { "listChanged": true, "subscribe": true } means the server supports the resources feature, can notify clients if the list of resources changes, and allows clients to subscribe to individual resource content updates.
A server MUST NOT attempt to use a client-declared capability if the client did not advertise it, and conversely, a client MUST NOT attempt to use a server capability if the server did not advertise it or if the negotiated set does not include it. Therefore, the server implementation needs a mechanism to store and consult these negotiated capabilities for the duration of a client session. This internal state will guide the server's behavior, such as determining whether it is permissible to send a list_changed notification or to expect a client to understand resource subscription semantics.
III. HTTP SSE Transport Implementation
A. Overview of SSE for MCP
For MCP implementations over HTTP, Server-Sent Events (SSE) is a specified transport mechanism.1 SSE enables a server to push data to a client asynchronously over a single, long-lived HTTP connection. This is ideal for server-to-client messages like MCP responses and notifications.
Critically, in the context of MCP's bidirectional communication needs, client-to-server messages (MCP Requests and Notifications from the client) are typically sent via separate HTTP POST requests to a designated endpoint on the server.9
B. Server-Side Requirements
Implementing an MCP server using HTTP SSE involves handling two types of HTTP interactions:
1. SSE Endpoint (e.g., /sse or /messages)
The client initiates the SSE connection by making an HTTP GET request to a specific endpoint on the server (e.g., /sse as mentioned in 1, or /messages as seen in some examples 21). The exact path for this endpoint should be clearly defined and communicated as part of the server's access information.
Upon receiving this GET request, the server MUST:
               * Respond with an HTTP status code of 200 OK.
               * Include the following HTTP headers in its response 21:
               * Content-Type: text/event-stream (This header is essential for the client to recognize the response as an SSE stream).
               * Cache-Control: no-cache (Strongly recommended to prevent intermediate proxies from buffering the stream).
               * Connection: keep-alive (Strongly recommended to ensure the underlying TCP connection remains open for the duration of the SSE stream).
               * Keep this HTTP connection open. It is through this persistent connection that the server will send all MCP JSON-RPC Response and Notification objects destined for that particular client.
2. HTTP POST Endpoint (for Client-to-Server Messages)
To send MCP Request or Notification messages to the server, the client will make HTTP POST requests. The specification indicates that these POST requests are typically directed to the same path as the SSE endpoint (e.g., /sse or /messages).9
The server must listen for HTTP POST requests on this path. The body of each POST request will contain a single JSON-RPC Request or Notification object, formatted as a JSON string.
The server processes this incoming JSON-RPC message. If the message was a JSON-RPC Request (containing an id), the server will generate a corresponding JSON-RPC Response. This Response MUST then be sent back to the client over the established SSE stream associated with that client.
This architecture implies a crucial implementation detail: the server needs a mechanism to correlate incoming HTTP POST requests with the correct, active SSE stream of the originating client. When a client sends a POST request, the server must identify which SSE connection belongs to that client to route the JSON-RPC response appropriately. This often involves some form of session management or connection identification established when the SSE connection is first made, although the MCP specification itself does not prescribe the exact method for this correlation. It is an essential implementation consideration for the server developer.
3. Message Formatting for SSE Stream (Server-to-Client)
All MCP JSON-RPC Response and Notification objects sent by the server to the client over the SSE stream MUST be formatted according to the SSE specification.21 Each message is an "event" and should be structured as plain text, UTF-8 encoded:
id: <unique_event_id_optional>
event: <mcp_message_type_optional_e.g., 'message'>
data: <JSON_RPC_Response_or_Notification_string_single_line>
data: <if_JSON_string_had_newlines_it_is_split_across_multiple_data_lines>
               * id: (Optional) A unique ID for this event. Clients can use this with the Last-Event-ID header to resume connections.
               * event: (Optional) A string identifying the type of event. For MCP, this might be a generic type like "message", or it could potentially be the MCP method name for notifications. The client will parse the data field to get the full JSON-RPC message.
               * data: The payload of the event. This MUST be the JSON-stringified MCP Response or Notification object.
               * If the JSON string itself contains newline characters, it MUST be broken into multiple consecutive data: lines.
               * Each event is terminated by a blank line (i.e., two consecutive newline characters: \n\n).
C. Security Considerations for SSE
               * Origin Validation: Servers SHOULD validate the Origin HTTP header on incoming SSE connection requests to mitigate risks such as DNS rebinding attacks.21
               * TLS (HTTPS): All MCP communication over HTTP, including SSE streams and POST requests, MUST be secured using TLS (HTTPS) to protect the confidentiality and integrity of the data in transit.9
D. Table: HTTP SSE Transport Characteristics
To summarize the HTTP SSE transport mechanism for MCP:
Aspect
	Client Role/Action
	Server Role/Action
	Key HTTP Headers (Server Response)
	Connection Setup
	Sends HTTP GET request to server's SSE endpoint (e.g., /sse).
	Accepts GET request. Responds 200 OK. Keeps connection open for streaming. Manages mapping of this connection to the client identity.
	Content-Type: text/event-stream, Cache-Control: no-cache, Connection: keep-alive
	Client-to-Server Message
	Sends MCP Request/Notification as JSON body of an HTTP POST request to server's endpoint.
	Accepts POST request. Parses JSON-RPC message. Processes request.
	(Standard POST request headers from client)
	Server-to-Client Message
	Listens on the open SSE stream for events. Parses data field of events as JSON-RPC messages.
	Sends MCP Response/Notification as SSE events over the established stream. Formats messages with data: lines, terminated by \n\n.
	(N/A - part of SSE stream format)
	Connection Teardown
	Closes the HTTP connection.
	Detects connection closure. Cleans up any associated state or resources.
	(N/A)
	This table clarifies the distinct roles and actions for establishing and using the HTTP SSE transport, highlighting the dual nature of the server's HTTP handling (SSE streaming via GET, and request processing via POST).
IV. Implementing Server Capabilities
An MCP server exposes its functionalities through three primary capabilities: Tools, Resources, and Prompts. For each, the server must implement specific JSON-RPC methods for discovery and interaction, and define the structure of the data it exchanges. The official MCP JSON schema is the authoritative reference for all data structures.7
A. Tools
1. Purpose:
Tools are executable functions that the server exposes. An LLM, via the MCP Client, can request the invocation of these tools to perform specific actions, retrieve dynamic information, or interact with other systems.4
2. ToolDefinition Structure:
When a client requests a list of available tools, the server responds with an array of ToolDefinition objects. Each ToolDefinition MUST conform to the following structure 14:
               * name: A string that uniquely identifies the tool within the server. This name is used by clients to call the tool.
               * description: An optional string providing a human-readable explanation of what the tool does. This description is crucial for the LLM to understand the tool's purpose and when to use it. Including examples of usage within the description can significantly aid the LLM.
               * inputSchema: A object representing a valid JSON Schema that defines the expected input parameters for the tool.
               * The type of this schema MUST be "object".
               * It MUST contain a properties object, where each key is an input parameter name, and its value is a JSON Schema defining that parameter's type, format, description, etc.
               * It MAY contain a required array, listing the names of parameters that are mandatory for calling the tool. The quality and clarity of the inputSchema are vital for effective tool use by an LLM. As noted in practical implementations, a well-defined schema, though potentially "expensive" in terms of token count when presented to an LLM, is essential for the LLM to formulate correct arguments.25
               * annotations: An optional object containing hints about the tool's behavior, primarily for the client or user interface:
               * title: An optional string for a human-readable display title.
               * readOnlyHint: An optional boolean (default false). If true, indicates the tool does not modify its environment.
               * destructiveHint: An optional boolean (default true if readOnlyHint is false, otherwise false). If true, indicates the tool may perform destructive updates.
               * idempotentHint: An optional boolean (default false). If true, indicates repeated calls with the same arguments have no additional effect beyond the first call.
               * openWorldHint: An optional boolean (default true). If true, indicates the tool interacts with external, potentially unpredictable entities.
3. JSON-RPC Method: tools/list
               * Purpose: Allows the Client to discover the set of tools currently available from the Server.2
               * Request: The params for this method are typically empty or an empty object. Servers MAY implement pagination for large tool lists, in which case params could include pagination arguments (similar to prompts/list 19).
               * Response (result): An object containing a single key tools, whose value is an array of ToolDefinition objects as described above.
JSON
{
 "tools":
     },
     "annotations": { "readOnlyHint": true }
   }
   //... more ToolDefinition objects
 ]
}

4. JSON-RPC Method: tools/call
                  * Purpose: Enables the Client to request the execution of a specific tool by the Server.2
                  * Request (params): An object containing:
                  * name: A string specifying the name of the tool to be invoked. This MUST match one of the tool names returned by tools/list.
                  * arguments: An object whose members are the arguments for the tool. The structure and values of this object MUST conform to the inputSchema of the specified tool.
JSON
{
 "name": "get_weather",
 "arguments": {
   "location": "Paris, FR"
 }
}

                  * Response (result): An object containing:
                  * isError: A boolean indicating whether the tool execution resulted in an error. If true, the content array will typically contain details of the error.
                  * content: An array of content part objects representing the output of the tool. Each content part object has a type and associated data. Common types include:
                  * { "type": "text", "text": "string_result" }
                  * { "type": "image", "data": "base64_encoded_image_string", "mimeType": "image/png" } 1 The MCP schema may define other content types (e.g., "embedded" 24). If isError is true, content usually contains a single text object with the error message.
JSON
// Successful execution
{
 "isError": false,
 "content":
}
// Error during execution
{
 "isError": true,
 "content": [
   { "type": "text", "text": "Error: Could not retrieve weather for the specified location." }
 ]
}

5. JSON-RPC Notification: notifications/tools/list_changed
                  * Purpose: Allows the Server to inform the Client that the list of available tools (or their definitions) has changed.24
                  * Notification: A JSON-RPC Notification object with:
                  * method: "notifications/tools/list_changed"
                  * params: Typically empty or an empty object {}.
                  * Upon receiving this notification, the Client SHOULD call tools/list again to get the updated tool definitions.
                  * The Server MUST only send this notification if the tools: { "listChanged": true } capability was successfully negotiated during initialization.
When designing tools, it's important to consider their granularity and the clarity of their descriptions and schemas. Overly numerous or poorly documented tools can be difficult for an LLM to utilize effectively. Often, a smaller set of well-defined, more general tools is preferable to a large collection of highly specific ones.25 The server implementation bears responsibility for this design aspect, which directly impacts the usability of its exposed capabilities.
B. Resources
1. Purpose:
Resources allow the server to expose various forms of data and content that clients can read and use as contextual information for LLM interactions. This can include file contents, database records, API responses, live system data, images, logs, and more.4
2. Resource Object Structure and Identification:
                  * URI Identification: Each resource is uniquely identified by a Uniform Resource Identifier (URI). The URI format is [protocol]://[host]/[path]. The server implementation is responsible for defining its own URI schemes (e.g., file:///path/to/doc.txt, database://table/record_id, api://endpoint/data).26 This URI scheme must be robust and allow the server to locate and serve the correct resource when requested.
                  * Content Types: Resources can contain 26:
                  * Text: UTF-8 encoded textual data.
                  * Binary: Raw binary data, which MUST be base64 encoded when transmitted within MCP messages.26
                  * Metadata (in resources/list response): When listing resources, each item typically includes 26:
                  * uri: The string URI of the resource.
                  * name: A string providing a human-readable name for the resource.
                  * description: An optional string detailing the resource.
                  * mimeType: An optional string indicating the MIME type of the resource (e.g., text/plain, application/json, image/jpeg).
                  * Content Payload (in resources/read response): When resource content is read, it's typically represented as 26:
                  * uri: The string URI of the resource.
                  * mimeType: An optional string MIME type.
                  * text: A string containing the UTF-8 text content (if it's a text resource).
                  * blob: A string containing the base64 encoded binary data (if it's a binary resource).
                  * Resource Templates: For resources that are dynamically generated or parameterized, the server can expose a uriTemplate (conforming to RFC 6570) instead of a static uri in the resources/list response. The client can then expand this template with specific values to form a concrete resource URI.26
3. JSON-RPC Method: resources/list
                  * Purpose: Allows the Client to discover the list of available resources and resource templates from the Server.
                  * Request: params are typically empty or an empty object. Pagination MAY be supported.
                  * Response (result): An object containing a resources key, whose value is an array of objects. Each object can be a concrete resource definition (with uri, name, etc.) or a resource template definition (with uriTemplate, name, etc.).26
JSON
{
 "resources":
}

4. JSON-RPC Method: resources/read
                     * Purpose: Allows the Client to read the content of one or more specified resources.26
                     * Request (params): An object containing:
                     * uri: A string representing the URI of the resource to be read.
JSON
{
 "uri": "file:///etc/config.ini"
}

                     * Response (result): An object containing a contents key. Its value is an array, where each element represents the content of a requested resource.26
JSON
{
 "contents":\nsetting=value\n"
   }
 ]
}
// For a binary resource:
// {
//   "uri": "image:///logo.png",
//   "mimeType": "image/png",
//   "blob": "iVBORw0KGgoAAAANSUhEUgAAAAUA..." // base64 data
// }

5. JSON-RPC Method: resources/subscribe
                        * Purpose: Allows the Client to subscribe to real-time updates for a specific resource. If the resource's content changes, the server will notify the client.26
                        * Request (params): An object { "uri": "string_resource_uri_to_subscribe_to" }.
                        * Response: An empty success response (e.g., result: {}).
                        * The Server MUST only permit subscriptions if the resources: { "subscribe": true } capability was negotiated.
6. JSON-RPC Method: resources/unsubscribe
                        * Purpose: Allows the Client to cancel a previous subscription to resource updates.26
                        * Request (params): An object { "uri": "string_resource_uri_to_unsubscribe_from" }.
                        * Response: An empty success response.
7. JSON-RPC Notification: notifications/resources/list_changed
                        * Purpose: Sent by the Server to the Client when the list of available resources or resource templates has changed.26
                        * Notification: method: "notifications/resources/list_changed", params (typically empty or {}).
                        * The Client SHOULD then call resources/list to fetch the updated list.
                        * The Server MUST only send this if resources: { "listChanged": true } was negotiated.
8. JSON-RPC Notification: notifications/resources/updated
                        * Purpose: Sent by the Server to the Client when the content of a resource to which the Client is subscribed has changed.26
                        * Notification (params): An object { "uri": "string_uri_of_updated_resource" }.
                        * Upon receiving this, the Client SHOULD call resources/read with the specified URI to get the new content.
The server's management of its resource URI scheme is a critical implementation detail. The scheme must uniquely identify all resources the server intends to expose and must be parsable by the server's own logic to retrieve and serve the correct content when resources/read requests are received.
C. Prompts
1. Purpose:
Prompts enable servers to define reusable, often parameterized, message templates or workflows. These are typically initiated by the user (via the Client interface) to guide LLM interactions in a standardized way for common tasks.4
2. Prompt Definition Structure (in prompts/list response):
A prompt is defined by the following structure when listed 19:
                        * name: A string that uniquely identifies the prompt.
                        * description: An optional string providing a human-readable explanation of the prompt's purpose.
                        * arguments: An optional array of PromptArgument objects. Each PromptArgument object defines a parameter the prompt can accept:
                        * name: A string identifying the argument.
                        * description: An optional string describing the argument.
                        * required: An optional boolean indicating if the argument is mandatory.
3. JSON-RPC Method: prompts/list
                        * Purpose: Allows the Client to discover the set of predefined prompts available from the Server.19
                        * Request: params are typically empty or an empty object. Pagination MAY be supported.19
                        * Response (result): An object containing a prompts key, whose value is an array of PromptDefinition objects.
JSON
{
 "prompts":
   }
   //... more PromptDefinition objects
 ]
}

4. JSON-RPC Method: prompts/get
                           * Purpose: Allows the Client to retrieve the actual message sequence for a specific prompt, optionally providing values for its arguments.19
                           * Request (params): An object containing:
                           * name: A string specifying the name of the prompt to retrieve.
                           * arguments: An object where keys are argument names (matching those in the prompt's definition) and values are the user-provided or client-determined values for those arguments.
JSON
{
 "name": "summarize_text",
 "arguments": {
   "text_to_summarize": "The Model Context Protocol is an open standard...",
   "max_length": 100
 }
}

                           * Response (result): An object containing:
                           * description: An optional string, usually the description from the prompt definition.
                           * messages: An array of Message objects. This array constitutes the structured conversation or instruction set to be sent to the LLM. Each Message object has:
                           * role: A string, either "user" or "assistant", indicating the originator of the message content.
                           * content: An object defining the content of the message. This can be of various types:
                           * { "type": "text", "text": "string_content" }
                           * { "type": "resource", "resource": { "uri": "string_resource_uri", "text": "string_resource_text_if_applicable", "mimeType": "string_mime_type" } }.19 This allows prompts to dynamically embed content from resources managed by the server.
JSON
{
 "description": "Generates a concise summary of the provided text.",
 "messages":
}

The server's logic for prompts/get is not just simple string templating. It involves constructing a structured array of Message objects. If a prompt definition requires embedding resource content, the server's handler for prompts/get might need to internally fetch that resource data (similar to its resources/read logic) to populate the messages array correctly. This makes prompt serving a potentially dynamic process.
5. JSON-RPC Notification: notifications/prompts/list_changed
                           * Purpose: Sent by the Server to the Client when the list of available prompts has changed.28
                           * Notification: method: "notifications/prompts/list_changed", params (typically empty or {}).
                           * The Client SHOULD then call prompts/list to fetch the updated list.
                           * The Server MUST only send this if prompts: { "listChanged": true } was negotiated.
V. Implementing Protocol Utilities
MCP defines several utility methods and notifications to manage the connection and long-running operations.
A. Ping
                           * Purpose: Allows either the client or the server to verify that the connection is still active and the other party is responsive.11
                           * Request (Client-to-Server or Server-to-Client):
                           * method: "ping"
                           * params: MAY be omitted or an empty object {}.30
                           * id: A unique identifier for the ping request.
                           * Response:
                           * result: An empty object {}.30
                           * id: MUST match the id of the ping request.
                           * Behavior: The receiver of a ping request MUST respond promptly with an empty result. If the sender does not receive a response within a reasonable timeout period, it MAY consider the connection stale and take appropriate action, such as terminating the connection.30 Implementations SHOULD periodically issue pings, with a configurable frequency, but avoid excessive pinging.
B. Cancellation ($/cancelRequest)
                           * Purpose: Allows the sender of a previous request to attempt to cancel its execution if it is taking too long or is no longer needed.7 The method name $/cancelRequest is conventional for such utility notifications in language server-like protocols.
                           * Notification (Sender to Receiver): This is a JSON-RPC Notification (no id).
                           * method: "$/cancelRequest"
                           * params: An object containing:
                           * id: The string or number id of the original request that should be cancelled.7
                           * Behavior: Upon receiving a $/cancelRequest notification, the receiver (e.g., the server, if a client is canceling a tools/call) SHOULD make a best-effort attempt to stop processing the request identified by the params.id. The receiver SHOULD NOT send a regular response for the canceled request. If an error occurred before the cancellation was processed, an error response might still be sent. Cancellation is not guaranteed to be instantaneous or always successful. For a server to effectively handle cancellation, it needs to track its ongoing operations by their original request IDs. This is particularly relevant for tools that might involve long-running computations or external I/O.
C. Progress Reporting ($/progress)
                           * Purpose: Allows the party handling a long-running request (e.g., a server executing a complex tool) to send intermediate progress updates to the requester (e.g., the client).7 The method name $/progress is conventional.
                           * Notification (Handler to Requester): This is a JSON-RPC Notification.
                           * method: "$/progress"
                           * params: An object containing:
                           * token: A string or number that uniquely identifies the progress reporting session. This token is used to associate the progress updates with the original request.
                           * value: An object containing the actual progress information. The structure of this value object is specific to the operation reporting progress but often includes fields like percentage (Number), message (String), or kind (String, e.g., "begin", "report", "end").7
                           * Behavior: The token for progress reporting needs to be established. For a server reporting progress on a tools/call, the client might generate this token and pass it as part of the tools/call arguments if the tool's inputSchema defines such a parameter, or the server might return a token in its initial response to tools/call if progress will follow. The MCP specification 7 lists "Progress tracking" as an additional utility. The RequestHandlerExtra object, potentially available to server-side request handlers in some SDKs 9, might offer a progress callback mechanism, abstracting the token management. If a server exposes tools that can take significant time, implementing progress reporting can greatly enhance the user experience by providing feedback on the operation's status.
VI. Error Handling and Logging
A. Standard JSON-RPC Errors
Robust error handling is crucial for a stable MCP server. The server MUST use the standard JSON-RPC error codes for general protocol-level errors 9:
                           * -32700 ParseError: Indicates invalid JSON was received.
                           * -32600 InvalidRequest: The received JSON is not a valid JSON-RPC Request object.
                           * -32601 MethodNotFound: The requested method does not exist or is not available on the server.
                           * -32602 InvalidParams: The parameters provided for a method are invalid or missing.
                           * -32603 InternalError: An internal error occurred on the server while processing the request.
The error response object MUST include code (Integer), message (String), and MAY include data for additional error details.11
B. Capability-Specific Errors
In addition to standard JSON-RPC errors, the server SHOULD use appropriate codes for errors specific to its capabilities. For example:
                           * When handling a prompts/get request, if the requested prompt name is not found, the server SHOULD return an error with code -32602 (InvalidParams) and a descriptive message.19
                           * If an internal server error occurs while fetching or constructing a prompt's content, -32603 (InternalError) would be appropriate.19
The server SHOULD always provide clear and informative error messages to help the client (and potentially the developer debugging the client) understand the nature of the failure.
C. Logging
While the MCP specification mentions "Logging" as an additional utility 7, which might imply a protocol-level mechanism for the server to send log messages to the client (akin to window/logMessage in LSP), the primary focus for a server implementer should be on robust internal server-side logging.
The server SHOULD implement comprehensive logging for:
                           * Debugging: Recording detailed information about incoming requests, processing steps, and outgoing responses.
                           * Auditing: Keeping a record of significant operations, especially for tools that might have side effects.
                           * Monitoring: Tracking server health, performance metrics, and error rates.9
When logging, the server MUST take care to avoid logging sensitive information contained within requests or responses, unless explicitly required for security auditing and appropriately protected.9 Protocol events, message flow, and errors are key items to log.
VII. Data Models and JSON Schema
A. The Central Role of schema.json
The official Model Context Protocol schema.json file, which is typically generated from a schema.ts (TypeScript definition file), serves as the single source of truth for all MCP message structures, parameter types, and object definitions.7 Any MCP server implementation MUST ensure that its parsing of incoming JSON requests and its generation of outgoing JSON responses strictly conform to the definitions in this schema. Developers should always refer to the schema version that corresponds to the targeted MCP protocol version (e.g., the schema associated with protocol version "2025-03-26" 14).
B. Key Data Structures to Implement
Implementing an MCP server without a pre-existing SDK requires manually defining data structures (classes, structs, records, etc., depending on the chosen language) that correspond to the JSON objects defined in the MCP schema. This allows for type-safe handling, validation, and serialization/deserialization of MCP messages. Key structures include, but are not limited to:
                           * Initialization: InitializeParams, InitializeResult (which encompass ClientInfo, ServerInfo, ClientCapabilities, ServerCapabilities objects).
                           * Tools: ToolDefinition (including InputSchema and Annotations), ToolCallParams, ToolCallResult (and its ContentPart variants like TextContentPart, ImageContentPart).
                           * Resources: ResourceDefinition (for resources/list, covering both static URIs and uriTemplate), ResourceReadParams, ResourceReadResult (and its ResourceContentPart variants for text and blob data), ResourceSubscriptionParams.
                           * Prompts: PromptDefinition (including PromptArgument), PromptGetParams, PromptGetResult (and the Message structure with Role and ContentPart).
                           * RPC Primitives: Request, Response (success/error), Notification, ErrorObject.
                           * Utilities: CancelParams (for $/cancelRequest), ProgressParams (for $/progress).
The complexity of the official schema underscores the importance of careful data modeling in the server's implementation language. While SDKs often provide these structures automatically (e.g., the Rust MCP SDK generates Rust types from the schema 31), a manual implementation will require diligent translation of the JSON Schema definitions into corresponding native types or the use of robust generic JSON parsing libraries coupled with thorough validation against the schema.
C. Table: Core MCP Data Structures and JSON Schema Reference
MCP Object/Message Part
	Key Properties (Illustrative)
	JSON Schema Reference
	InitializeParams
	protocolVersion, clientInfo, capabilities (client's)
	Defined under initialize request in the official schema.json.
	InitializeResult
	protocolVersion, serverInfo, capabilities (server's)
	Defined under initialize response in the official schema.json.
	ToolDefinition
	name, description, inputSchema (JSON Schema), annotations
	Defined as an item in the tools array of the tools/list response, and within definitions in schema.json.
	ToolCallParams
	name, arguments (object matching inputSchema)
	Defined under tools/call request in schema.json.
	ToolCallResult
	isError, content (array of ContentPart like TextContentPart, ImageContentPart)
	Defined under tools/call response in schema.json.
	ResourceDefinition
	uri or uriTemplate, name, description, mimeType
	Defined as an item in the resources array of the resources/list response, and within definitions in schema.json.
	ResourceContentPart
	uri, mimeType, text (for text) or blob (for binary, base64)
	Defined as an item in the contents array of the resources/read response in schema.json.
	PromptDefinition
	name, description, arguments (array of PromptArgument)
	Defined as an item in the prompts array of the prompts/list response, and within definitions in schema.json.
	PromptMessage
	role (user or assistant), content (object with type like text or resource)
	Defined as an item in the messages array of the prompts/get response in schema.json.
	ErrorObject
	code (integer), message (string), data (optional)
	Standard JSON-RPC structure, referenced in all error responses in schema.json.
	Notification (generic)
	method, params
	Standard JSON-RPC structure. Specific notifications like notifications/tools/list_changed or $/cancelRequest follow this, with method-specific params defined in schema.json.
	This table serves as a high-level cross-reference, emphasizing that the detailed structure of each of these conceptual objects is formally defined within the official MCP JSON schema. Implementers MUST consult the schema directly for precise field names, types, and optionality.
VIII. Security Considerations
Implementing an MCP server requires careful attention to security, as it exposes capabilities and potentially handles data that can be sensitive. While the MCP Host/Client often acts as the primary gatekeeper for user consent, the server itself has significant responsibilities.
A. MCP Security Principles
The MCP specification outlines several key security principles that should guide implementation 7:
                           * User Consent and Control: The end-user MUST have explicit control over what data is shared and what actions are taken. While this is primarily enforced by the Host application (e.g., Cursor, Claude Desktop), the server should operate under the assumption that any request it receives has been appropriately authorized by the user through the client.
                           * Data Privacy: Any user data or contextual information passed to the server (e.g., as arguments to tools, or as content for prompts) MUST be handled with care. The server should not misuse, unnecessarily store, or transmit this data elsewhere without explicit consent or a clear policy.
                           * Tool Safety: Tools exposed by the server can represent arbitrary code execution from the perspective of the Host/Client. The server's descriptions of tool behavior are considered untrusted by the client unless the server itself is explicitly trusted by the Host/user. The Host is typically responsible for obtaining user consent before invoking any tool.
                           * LLM Sampling Controls: If the server architecture involves requesting LLM sampling through the client (an advanced feature not detailed for server implementation in this guide), user consent and control over prompts and completions are paramount.
B. Server-Side Responsibilities
To maintain a secure environment, the MCP server implementation MUST address the following:
                           * Input Validation: All incoming data, especially parameters for tools/call, resources/read, prompts/get, and any other method, MUST be rigorously validated against their defined schemas.9 This helps prevent injection attacks, malformed requests leading to crashes, and other vulnerabilities arising from unexpected input.
                           * Resource Protection: If the server exposes resources from a file system, it MUST implement strong path canonicalization and validation to prevent path traversal vulnerabilities (e.g., ensuring a client cannot request file:///../../../etc/passwd). If resources originate from databases or other backend systems, appropriate access controls and query sanitization are necessary.9
                           * Error Handling: Error messages returned to the client MUST NOT leak sensitive internal server information, such as stack traces, internal IP addresses, or database credentials.9 Errors should be informative but generic regarding internal state.
                           * Transport Security: All network communication for MCP, especially when using HTTP SSE, MUST be encrypted using TLS (HTTPS).9 This protects the confidentiality and integrity of MCP messages in transit.
                           * Authentication and Authorization: While the core MCP specification is transport-agnostic and does not mandate a specific authentication scheme for client-to-server communication, any production-level MCP server exposed over a network WILL require robust authentication and authorization mechanisms. This is to ensure that only legitimate and authorized clients can connect and invoke its capabilities. OAuth 2.0 has been discussed as a potential mechanism for MCP.2 Although this guide focuses on the protocol mechanics, implementers must plan for and integrate appropriate security measures for their specific deployment environment. Cursor, for example, allows providing environment variables to MCP servers for authentication with external services the MCP server itself might call.1
                           * Rate Limiting: To protect against denial-of-service attacks or accidental overuse, the server SHOULD implement rate limiting on incoming requests, particularly for resource-intensive operations or tool calls.
The server is a critical component in the overall security posture of an MCP-enabled system. Even if the Host/Client manages user consent, the server's diligence in validating inputs, protecting its resources, and securing its communication channels is essential for a trustworthy integration.
IX. Advanced Topics & Future Considerations (Brief Mention)
While this guide focuses on the foundational aspects of implementing an MCP server that exposes Tools, Resources, and Prompts, the Model Context Protocol encompasses more advanced capabilities and is an evolving standard.
                           * A. Server Initiating sampling/createMessage:
MCP allows a server to request LLM completions from the client, if the client has declared the sampling capability during initialization.4 This enables more complex, agentic behaviors where the server can leverage the client's LLM for sub-tasks. The server would send a sampling/createMessage request, and the client would handle the LLM interaction, returning the result to the server.
                           * B. Server Using Client-Exposed roots:
If a client supports and declares the roots capability, it can inform the server about relevant filesystem directories or other URI-based contexts (e.g., project workspaces).4 The server can then request this list of roots from the client (e.g., via a roots/list request, though the direction of this request needs clarification from full spec) to scope its operations or discover relevant resources within those boundaries.
                           * C. Structured Output from Tools:
There are ongoing discussions and proposals within the MCP community to allow tools to return more structured data (e.g., JSON with a specific schema) rather than just text or simple binary content. This might involve new fields in the tools/call response or metadata in the ToolDefinition to specify output schemas.37
                           * D. Pagination:
For methods that list multiple items (e.g., tools/list, resources/list, prompts/list), servers handling a large number of such items SHOULD consider implementing pagination to manage response sizes and client-side processing. The prompts/list specification already mentions support for pagination.19
X. Conclusion and Next Steps
Implementing a Model Context Protocol server from scratch is a significant undertaking that requires meticulous attention to the protocol's specifications, particularly its reliance on JSON-RPC 2.0, the HTTP SSE transport mechanism, and the detailed structures for capabilities like Tools, Resources, and Prompts. This guide has provided a comprehensive walkthrough of these core components, aiming to equip developers to build conformant servers in environments where pre-existing MCP frameworks or SDKs are unavailable.
Successful implementation hinges on:
                              1. Strict Adherence to JSON-RPC 2.0: All message parsing and generation must precisely follow the standard.
                              2. Correct Implementation of the Connection Lifecycle: The initialize handshake and capability negotiation are fundamental.
                              3. Robust HTTP SSE Handling: Managing the persistent SSE stream for server-to-client messages and handling separate HTTP POST requests for client-to-server messages, along with proper correlation, is key.
                              4. Accurate Data Modeling: Faithfully representing MCP data structures as defined in the official JSON schema is crucial for interoperability.
                              5. Thorough Security Practices: Input validation, transport security (TLS), and careful resource management are non-negotiable.
Next Steps for Implementation:
                              * Obtain the Official MCP JSON Schema: Secure the latest version of the schema.json corresponding to the target protocol version (e.g., "2025-03-26"). This will be your definitive guide for all data structures.
                              * Select HTTP Server Library: Choose an appropriate HTTP server library in your target programming language that allows fine-grained control over request handling, response generation, and managing long-lived connections for SSE.
                              * Implement Core Structures: Define language-specific classes or data structures for all MCP message types (Request, Response, Notification, Error) and key data objects (ToolDefinition, Resource, Prompt, etc.).
                              * Build the initialize Handler: This is the first critical piece of MCP logic. It must parse the client's initialize request, perform version and capability negotiation, and formulate the correct initialize response.
                              * Implement Capability Endpoints: Develop handlers for tools/list, tools/call, resources/list, resources/read, prompts/list, prompts/get, and their associated notifications if supported.
                              * Develop Transport Logic: Implement the SSE endpoint for streaming server-to-client messages and the POST handler for client-to-server messages. Ensure robust error handling at the transport layer.
                              * Testing: Thorough testing is essential.
                              * Utilize the MCP Inspector tool if available, which is designed for interacting with and debugging MCP servers.27
                              * If feasible, test against known MCP clients like Cursor or Claude Desktop (if they allow configuration with custom, locally-running MCP servers). Cursor, for instance, allows configuring local MCP servers via a mcp.json file.1
                              * Develop unit tests for individual message handlers and integration tests for full request-response flows.
By following the detailed specifications outlined in this guide and consistently referring to the official MCP documentation and schema, developers can successfully build custom MCP servers capable of extending AI agents and applications with rich, contextual capabilities.
Works cited
                              1. Model Context Protocol - Cursor, accessed May 9, 2025, https://docs.cursor.com/context/model-context-protocol
                              2. Model Context Protocol (MCP): A comprehensive introduction for ..., accessed May 9, 2025, https://stytch.com/blog/model-context-protocol-introduction/
                              3. A beginners Guide on Model Context Protocol (MCP) - OpenCV, accessed May 9, 2025, https://opencv.org/blog/model-context-protocol/
                              4. The Model Context Protocol (MCP) by Anthropic: Origins, functionality, and impact - Wandb, accessed May 9, 2025, https://wandb.ai/onlineinference/mcp/reports/The-Model-Context-Protocol-MCP-by-Anthropic-Origins-functionality-and-impact--VmlldzoxMTY5NDI4MQ
                              5. Model Context Protocol (MCP) - Anthropic API, accessed May 9, 2025, https://docs.anthropic.com/en/docs/agents-and-tools/mcp
                              6. Model Context Protocol (MCP): A Guide With Demo Project - DataCamp, accessed May 9, 2025, https://www.datacamp.com/tutorial/mcp-model-context-protocol
                              7. Specification - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/specification/2025-03-26
                              8. Understanding the Model Context Protocol (MCP): Architecture - Nebius, accessed May 9, 2025, https://nebius.com/blog/posts/understanding-model-context-protocol-mcp-architecture
                              9. Core architecture - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/docs/concepts/architecture
                              10. How to Use Model Context Protocol the Right Way - Boomi, accessed May 9, 2025, https://boomi.com/blog/model-context-protocol-how-to-use/
                              11. Building Standardized AI Tools with the Model Context Protocol ..., accessed May 9, 2025, https://www.innoq.com/en/articles/2025/03/model-context-protocol/
                              12. Model Context Protocol (MCP) :: Spring AI Reference, accessed May 9, 2025, https://docs.spring.io/spring-ai/reference/api/mcp/mcp-overview.html
                              13. modelcontextprotocol/modelcontextprotocol: Specification ... - GitHub, accessed May 9, 2025, https://github.com/modelcontextprotocol/modelcontextprotocol
                              14. modelcontextprotocol/schema/2025-03-26/schema.json at main - GitHub, accessed May 9, 2025, https://github.com/modelcontextprotocol/modelcontextprotocol/blob/main/schema/2025-03-26/schema.json
                              15. specification/docs/specification/2025-03-26/basic/_index.md at main - GitHub, accessed May 9, 2025, https://github.com/modelcontextprotocol/specification/blob/main/docs/specification/2025-03-26/basic/_index.md
                              16. Model Context Protocol (MCP) - Everything You Need to Know - Zencoder, accessed May 9, 2025, https://zencoder.ai/blog/model-context-protocol
                              17. Lifecycle - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/specification/draft/basic/lifecycle
                              18. Roots - Model Context Protocol Specification, accessed May 9, 2025, https://spec.modelcontextprotocol.io/specification/2025-03-26/client/roots/
                              19. Prompts - Specification - Model Context Protocol, accessed May 9, 2025, https://spec.modelcontextprotocol.io/specification/2025-03-26/server/prompts/
                              20. Extend your agent with Model Context Protocol (preview) - Microsoft Copilot Studio, accessed May 9, 2025, https://learn.microsoft.com/en-us/microsoft-copilot-studio/agent-extend-action-mcp
                              21. Transports - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/docs/concepts/transports
                              22. For Server Developers - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/quickstart/server
                              23. What is MCP? An overview of the Model Context Protocol - Speakeasy, accessed May 9, 2025, https://www.speakeasy.com/mcp/intro
                              24. Tools - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/docs/concepts/tools
                              25. One Month into MCP: Building an Interface Between LLMs and Unstructured, accessed May 9, 2025, https://unstructured.io/blog/one-month-into-mcp-building-an-interface-between-llms-and-unstructured
                              26. Resources - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/docs/concepts/resources
                              27. Model Context Protocol | Blog - Sonalake, accessed May 9, 2025, https://sonalake.com/latest/model-context-protocol/
                              28. Prompts - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/docs/concepts/prompts
                              29. What are MCP Prompts? - Speakeasy, accessed May 9, 2025, https://www.speakeasy.com/mcp/prompts
                              30. Ping - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/specification/2025-03-26/basic/utilities/ping
                              31. rust-mcp-schema - Lib.rs, accessed May 9, 2025, https://lib.rs/crates/rust-mcp-schema
                              32. Model Context Protocol (MCP) Schema for Rust - Crates.io, accessed May 9, 2025, https://crates.io/crates/rust-mcp-schema
                              33. model-context-protocol-resources/guides/mcp-server-development-guide.md at main - GitHub, accessed May 9, 2025, https://github.com/cyanheads/model-context-protocol-resources/blob/main/guides/mcp-server-development-guide.md
                              34. [RFC] Update the Authorization specification for MCP servers by localden · Pull Request #284 - GitHub, accessed May 9, 2025, https://github.com/modelcontextprotocol/modelcontextprotocol/pull/284?ref=blog.arcade.dev
                              35. Sampling - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/docs/concepts/sampling
                              36. Roots - Model Context Protocol, accessed May 9, 2025, https://modelcontextprotocol.io/docs/concepts/roots
                              37. [Proposal] Suggested Response Format #315 - GitHub, accessed May 9, 2025, https://github.com/modelcontextprotocol/modelcontextprotocol/discussions/315
                              38. Model Context Protocol - GitHub, accessed May 9, 2025, https://github.com/modelcontextprotocol
