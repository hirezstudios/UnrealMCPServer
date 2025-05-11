#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "JsonUtilities.h" // For FJsonObjectConverter
#include "Serialization/JsonSerializer.h" // For FJsonSerializer
#include "UMCP_Types.generated.h"

// Standard JSON-RPC 2.0 Error Codes & MCP Specific Codes
enum class EUMCP_JsonRpcErrorCode : int32
{
    // Standard JSON-RPC 2.0 Error Codes
    ParseError = -32700,
	ResourceNotFound = -32002,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ServerError = -32000, // Generic server error base

    // -32000 to -32099 are reserved for implementation-defined server-errors.
    // We can add more specific server errors in this range if needed.

    // MCP Specific Error Codes (can start from -32099 downwards or use another range)
    // Example: OperationNotSupported = -32001,
};

// Represents a JSON-RPC Request ID, which can be a string, number, or null.
// Also handles the concept of an "absent" ID for notifications that don't send one.
USTRUCT()
struct FUMCP_JsonRpcId
{
    GENERATED_BODY()

private:
    // Internal storage for the ID. Can be FJsonValueString, FJsonValueNumber, FJsonValueNull, or nullptr (for absent ID).
    TSharedPtr<FJsonValue> Value;

    // Private constructor for internal use by static factories or specific type constructors
    FUMCP_JsonRpcId(TSharedPtr<FJsonValue> InValue) : Value(InValue) {}

public:
    // Default constructor: Represents an absent ID (e.g. for notifications or when ID field is missing).
    FUMCP_JsonRpcId() : Value(nullptr) {}
    FUMCP_JsonRpcId(const FString& InString) : Value(MakeShared<FJsonValueString>(InString)) {}
    FUMCP_JsonRpcId(int32 InNumber) : Value(MakeShared<FJsonValueNumber>(static_cast<double>(InNumber))) {}

    static FUMCP_JsonRpcId CreateNullId();
    static FUMCP_JsonRpcId CreateFromJsonValue(const TSharedPtr<FJsonValue>& JsonValue);
    bool IsString() const;
    bool IsNumber() const;
    bool IsNull() const;

    TSharedPtr<FJsonValue> GetJsonValue() const;
    FString ToString() const;
};

// Forward declaration
struct FUMCP_JsonRpcError;

USTRUCT()
struct FUMCP_JsonRpcRequest
{
    GENERATED_BODY()

    UPROPERTY()
    FString jsonrpc; // Should be "2.0"

    UPROPERTY()
    FString method;

    TSharedPtr<FJsonObject> params; // Using TSharedPtr<FJsonObject> for params
    FUMCP_JsonRpcId id;

    FUMCP_JsonRpcRequest() : jsonrpc(TEXT("2.0")) {}
    bool ToJsonString(FString& OutJsonString) const;
    static bool CreateFromJsonString(const FString& JsonString, FUMCP_JsonRpcRequest& OutRequest);
};

USTRUCT()
struct FUMCP_JsonRpcError
{
    GENERATED_BODY()

    UPROPERTY()
    int32 code;

    UPROPERTY()
    FString message;

    // data can be any JSON value, using FJsonValue to represent it.
    TSharedPtr<FJsonValue> data; 

    FUMCP_JsonRpcError() : code(0) {}
    FUMCP_JsonRpcError(EUMCP_JsonRpcErrorCode InErrorCode, const FString& InMessage, TSharedPtr<FJsonValue> InData = nullptr)
        : code(static_cast<int32>(InErrorCode)), message(InMessage), data(InData) {}
	void SetError(EUMCP_JsonRpcErrorCode error) { code = static_cast<int32>(error); }

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_JsonRpcError& OutErrorObject);
};

USTRUCT()
struct FUMCP_JsonRpcResponse
{
    GENERATED_BODY()

    UPROPERTY()
    FString jsonrpc; // Should be "2.0"

    // ID is now FJsonRpcId to encapsulate its type (string, number, null, or absent)
    FUMCP_JsonRpcId id;

    // Result can be any valid JSON value (object, array, string, number, boolean, null)
    TSharedPtr<FJsonValue> result; 
    
    TSharedPtr<FUMCP_JsonRpcError> error; // Error object if an error occurred

    FUMCP_JsonRpcResponse() : jsonrpc(TEXT("2.0")) {}

    bool ToJsonString(FString& OutJsonString) const;
    static bool CreateFromJsonString(const FString& JsonString, FUMCP_JsonRpcResponse& OutResponse);
};

// MCP Specific Structures
template<typename T>
UNREALMCPSERVER_API bool UMCP_ToJsonObject(const T& InStruct, TSharedPtr<FJsonObject>& OutJsonObject)
{
	if (!OutJsonObject.IsValid()) return false;
	return FJsonObjectConverter::UStructToJsonObject(InStruct.StaticStruct(), &InStruct, OutJsonObject.ToSharedRef());
}

template<typename T>
UNREALMCPSERVER_API bool UMCP_CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, T& OutStruct, bool bAllowMissingObject = false)
{
	if (!JsonObject.IsValid()) return bAllowMissingObject;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

USTRUCT()
struct FUMCP_ServerInfo
{
    GENERATED_BODY()

    UPROPERTY()
    FString name;

    UPROPERTY()
    FString version;

    FUMCP_ServerInfo() : name(TEXT("UnrealMCPServer")) {}
};

USTRUCT()
struct FUMCP_ServerCapabilitiesTools
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred

    UPROPERTY()
    bool inputSchema = true; // Whether server supports 'inputSchema' in ToolDefinition

    UPROPERTY()
    bool outputSchema = false; // Whether server supports 'outputSchema' in ToolDefinition
};

USTRUCT()
struct FUMCP_ServerCapabilitiesResources
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred

    UPROPERTY()
    bool subscribe = false; // Deferred as SSE is deferred
};

USTRUCT()
struct FUMCP_ServerCapabilitiesPrompts
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred
};

USTRUCT()
struct FUMCP_ServerCapabilities // Add more capability structs as needed (e.g., roots, sampling)
{
    GENERATED_BODY()

	//UPROPERTY()
	//FUMCP_ServerCapabilitiesLogging logging;

    UPROPERTY()
    FUMCP_ServerCapabilitiesTools tools;

    UPROPERTY()
    FUMCP_ServerCapabilitiesResources resources;

    UPROPERTY()
    FUMCP_ServerCapabilitiesPrompts prompts;
};

USTRUCT()
struct FUMCP_InitializeParams
{
    GENERATED_BODY()

    UPROPERTY()
    FString protocolVersion; // Client's supported protocol version, e.g., "2025-03-26"

    // Client capabilities can be added here later if needed for negotiation
    // UPROPERTY()
    // TSharedPtr<FClientCapabilities> capabilities;
	
    // Client capabilities can be added here later if needed for negotiation
    // UPROPERTY()
    // TSharedPtr<FJsonObject> clientInfo;
};

USTRUCT()
struct FUMCP_InitializeResult
{
    GENERATED_BODY()

    UPROPERTY()
    FString protocolVersion; // Server's chosen protocol version

    UPROPERTY()
    FUMCP_ServerInfo serverInfo;

    UPROPERTY()
    FUMCP_ServerCapabilities capabilities;
};

USTRUCT()
struct FUMCP_CallToolParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	TSharedPtr<FJsonObject> arguments;
};

template<>
bool UMCP_CreateFromJsonObject<FUMCP_CallToolParams>(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_CallToolParams& OutStruct, bool bAllowMissingObject)
{
	if (!JsonObject.IsValid()) return bAllowMissingObject;
	OutStruct.name = JsonObject->GetStringField("name");
	OutStruct.arguments = JsonObject->GetObjectField("arguments");
	return true;
}

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_CallToolResultContent
{
	GENERATED_BODY()
	
	UPROPERTY()
	FString data; // used by `audio` and `image` types

	UPROPERTY()
	FString text; // used by `text` type

	UPROPERTY()
	FString mimetype; // used by `audio` and `image` types

	UPROPERTY()
	FString type;

	//TODO add embedded resource
};

USTRUCT()
struct FUMCP_CallToolResult
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FUMCP_CallToolResultContent> content;

	UPROPERTY()
	bool isError;
};

USTRUCT()
struct FUMCP_ListToolsParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString cursor;
};

UNREALMCPSERVER_API DECLARE_DELEGATE_RetVal_TwoParams(bool, FUMCP_ToolCall, TSharedPtr<FJsonObject> /* arguments */, TArray<FUMCP_CallToolResultContent>& /* OutContent */);

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ToolDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString description;

	TSharedPtr<FJsonObject> inputSchema;
	FUMCP_ToolCall DoToolCall;

	FUMCP_ToolDefinition(): name{}, description{}, inputSchema{ MakeShared<FJsonObject>() }, DoToolCall()
	{
		inputSchema->SetStringField(TEXT("type"), TEXT("object"));
	}
};

USTRUCT()
struct FUMCP_ListToolsResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString nextCursor;

	UPROPERTY()
	TArray<FUMCP_ToolDefinition> tools;
};

USTRUCT()
struct FUMCP_ReadResourceParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString uri;
};

USTRUCT()
struct FUMCP_ReadResourceResultContent
{
	GENERATED_BODY()

	UPROPERTY()
	FString uri;
	
	UPROPERTY()
	FString text; // Used by text resources
	
	UPROPERTY()
	FString blob; // Used by blob resources
	
	UPROPERTY()
	FString mimeType;
};

USTRUCT()
struct FUMCP_ReadResourceResult
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FUMCP_ReadResourceResultContent> contents;
};

USTRUCT()
struct FUMCP_ListResourcesParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString cursor;
};

UNREALMCPSERVER_API DECLARE_DELEGATE_RetVal_TwoParams(bool, FUMCP_ResourceRead, const FString& /* Uri */, TArray<FUMCP_ReadResourceResultContent>& /* OutContent */);

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ResourceDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString description;

	UPROPERTY()
	FString mimeType;

	UPROPERTY()
	FString uri;

	// Part of the spec, but we don't need it for templated resources
	UPROPERTY()
	int32 size;

	FUMCP_ResourceRead ReadResource;
};

USTRUCT()
struct UNREALMCPSERVER_API FUMCP_ListResourcesResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString nextCursor;

	UPROPERTY()
	TArray<FUMCP_ResourceDefinition> resources;
};

USTRUCT()
struct FUMCP_ListResourceTemplatesParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString cursor;
};

USTRUCT()
struct FUMCP_ResourceTemplateDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString description;

	UPROPERTY()
	FString mimeType;

	UPROPERTY()
	FString uriTemplate;
	
	FUMCP_ResourceRead ReadResource;
};

USTRUCT()
struct FUMCP_ListResourceTemplatesResult
{
	GENERATED_BODY()

	UPROPERTY()
	FString nextCursor;

	UPROPERTY()
	TArray<FUMCP_ResourceTemplateDefinition> resourceTemplates;
};
