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
bool UMCP_ToJsonObject(const T& InStruct, TSharedPtr<FJsonObject>& OutJsonObject)
{
	if (!OutJsonObject.IsValid()) return false;
	return FJsonObjectConverter::UStructToJsonObject(InStruct.StaticStruct(), &InStruct, OutJsonObject.ToSharedRef());
}

template<typename T>
bool UMCP_CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, T& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

USTRUCT()
struct FUMCP_ServerInfo
{
    GENERATED_BODY()

    UPROPERTY()
    FString name; // e.g., "HiRezMCPUnreal"

    UPROPERTY()
    FString version; // e.g., "0.1.0"

    // Could add more fields like 'documentationUrl', 'icon', etc. as needed
    FUMCP_ServerInfo() : name(TEXT("HiRezMCPUnreal")) {}
};

USTRUCT()
struct FUMCP_ServerCapabilitiesTools
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred

    UPROPERTY()
    bool inputSchema = false; // Whether server supports 'inputSchema' in ToolDefinition

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
struct FUMCP_ListToolsParams
{
	GENERATED_BODY()

	UPROPERTY()
	FString Cursor;
};