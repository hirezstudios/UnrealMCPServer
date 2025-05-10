#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "JsonUtilities.h" // For FJsonObjectConverter
#include "Serialization/JsonSerializer.h" // For FJsonSerializer
#include "MCPTypes.generated.h"

// Standard JSON-RPC 2.0 Error Codes & MCP Specific Codes
enum class EMCPErrorCode : int32
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
struct FJsonRpcId
{
    GENERATED_BODY()

private:
    // Internal storage for the ID. Can be FJsonValueString, FJsonValueNumber, FJsonValueNull, or nullptr (for absent ID).
    TSharedPtr<FJsonValue> Value;

    // Private constructor for internal use by static factories or specific type constructors
    FJsonRpcId(TSharedPtr<FJsonValue> InValue) : Value(InValue) {}

public:
    // Default constructor: Represents an absent ID (e.g. for notifications or when ID field is missing).
    FJsonRpcId() : Value(nullptr) {}
    FJsonRpcId(const FString& InString) : Value(MakeShared<FJsonValueString>(InString)) {}
    FJsonRpcId(int32 InNumber) : Value(MakeShared<FJsonValueNumber>(static_cast<double>(InNumber))) {}

    static FJsonRpcId CreateNullId();
    static FJsonRpcId CreateFromJsonValue(const TSharedPtr<FJsonValue>& JsonValue);
    bool IsString() const;
    bool IsNumber() const;
    bool IsNull() const;

    TSharedPtr<FJsonValue> GetJsonValue() const;
    FString ToString() const;
};

// Forward declaration
struct FJsonRpcErrorObject;

USTRUCT()
struct FJsonRpcRequest
{
    GENERATED_BODY()

    UPROPERTY()
    FString jsonrpc; // Should be "2.0"

    UPROPERTY()
    FString method;

    TSharedPtr<FJsonObject> params; // Using TSharedPtr<FJsonObject> for params
    FJsonRpcId id;

    FJsonRpcRequest() : jsonrpc(TEXT("2.0")) {}
    bool ToJsonString(FString& OutJsonString) const;
    static bool CreateFromJsonString(const FString& JsonString, FJsonRpcRequest& OutRequest);
};

USTRUCT()
struct FJsonRpcErrorObject
{
    GENERATED_BODY()

    UPROPERTY()
    int32 code;

    UPROPERTY()
    FString message;

    // data can be any JSON value, using FJsonValue to represent it.
    TSharedPtr<FJsonValue> data; 

    FJsonRpcErrorObject() : code(0) {}
    FJsonRpcErrorObject(EMCPErrorCode InErrorCode, const FString& InMessage, TSharedPtr<FJsonValue> InData = nullptr)
        : code(static_cast<int32>(InErrorCode)), message(InMessage), data(InData) {}
	void SetError(EMCPErrorCode error) { code = static_cast<int32>(error); }

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FJsonRpcErrorObject& OutErrorObject);
};

USTRUCT()
struct FJsonRpcResponse
{
    GENERATED_BODY()

    UPROPERTY()
    FString jsonrpc; // Should be "2.0"

    // ID is now FJsonRpcId to encapsulate its type (string, number, null, or absent)
    FJsonRpcId id;

    // Result can be any valid JSON value (object, array, string, number, boolean, null)
    TSharedPtr<FJsonValue> result; 
    
    TSharedPtr<FJsonRpcErrorObject> error; // Error object if an error occurred

    FJsonRpcResponse() : jsonrpc(TEXT("2.0")) {}

    bool ToJsonString(FString& OutJsonString) const;
    static bool CreateFromJsonString(const FString& JsonString, FJsonRpcResponse& OutResponse);
};

// MCP Specific Structures

USTRUCT()
struct FServerInformation
{
    GENERATED_BODY()

    UPROPERTY()
    FString name; // e.g., "HiRezMCPUnreal"

    UPROPERTY()
    FString version; // e.g., "0.1.0"

    UPROPERTY()
    FString HirezMCPUnreal_version;

    UPROPERTY()
    FString unreal_engine_version;

    // Could add more fields like 'documentationUrl', 'icon', etc. as needed
    FServerInformation() : name(TEXT("HiRezMCPUnreal")) {}

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FServerInformation& OutStruct);
};

USTRUCT()
struct FToolServerCapabilities
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred

    UPROPERTY()
    bool inputSchema = false; // Whether server supports 'inputSchema' in ToolDefinition

    UPROPERTY()
    bool outputSchema = false; // Whether server supports 'outputSchema' in ToolDefinition

    FToolServerCapabilities() = default; // Added default constructor

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FToolServerCapabilities& OutStruct);
};

USTRUCT()
struct FResourceServerCapabilities
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred

    UPROPERTY()
    bool subscribe = false; // Deferred as SSE is deferred

    UPROPERTY()
    bool contentTypes = false; // If server provides 'contentTypes' in ResourceDefinition

    FResourceServerCapabilities() = default; // Added default constructor

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FResourceServerCapabilities& OutStruct);
};

USTRUCT()
struct FPromptServerCapabilities
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred

    FPromptServerCapabilities() = default; // Added default constructor

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FPromptServerCapabilities& OutStruct);
};

USTRUCT()
struct FServerCapabilities // Add more capability structs as needed (e.g., roots, sampling)
{
    GENERATED_BODY()

    UPROPERTY()
    FToolServerCapabilities tools;

    UPROPERTY()
    FResourceServerCapabilities resources;

    UPROPERTY()
    FPromptServerCapabilities prompts;

    FServerCapabilities() = default; // Default constructor is fine, members will be default-initialized

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FServerCapabilities& OutStruct);
};

USTRUCT()
struct FInitializeParams
{
    GENERATED_BODY()

    UPROPERTY()
    FString protocolVersion; // Client's supported protocol version, e.g., "2025-03-26"

    // Client capabilities can be added here later if needed for negotiation
    // UPROPERTY()
    // TSharedPtr<FClientCapabilities> capabilities;

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FInitializeParams& OutStruct);
};

USTRUCT()
struct FInitializeResult
{
    GENERATED_BODY()

    UPROPERTY()
    FString protocolVersion; // Server's chosen protocol version

    UPROPERTY()
    FServerInformation serverInfo;

    UPROPERTY()
    FServerCapabilities capabilities;

    FInitializeResult() = default; // Default constructor is fine, members will be default-initialized

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FInitializeResult& OutStruct);
};
