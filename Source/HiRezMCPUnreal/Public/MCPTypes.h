#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "MCPTypes.generated.h"

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

    UPROPERTY()
    TSharedPtr<FJsonObject> params; // Using TSharedPtr<FJsonObject> for params

    UPROPERTY()
    FString id; // JSON-RPC ID can be string, number, or null. FString is a safe bet for MCP. Can be empty for notifications.

    FJsonRpcRequest() : jsonrpc(TEXT("2.0")) {}
};

USTRUCT()
struct FJsonRpcResponse
{
    GENERATED_BODY()

    UPROPERTY()
    FString jsonrpc; // Should be "2.0"

    UPROPERTY()
    TSharedPtr<FJsonObject> result; // Using TSharedPtr<FJsonObject> for result

    UPROPERTY()
    TSharedPtr<FJsonRpcErrorObject> error;

    UPROPERTY()
    FString id; // Must be the same as the request it is responding to.

    FJsonRpcResponse() : jsonrpc(TEXT("2.0")) {}
};

USTRUCT()
struct FJsonRpcErrorObject
{
    GENERATED_BODY()

    UPROPERTY()
    int32 code;

    UPROPERTY()
    FString message;

    UPROPERTY()
    TSharedPtr<FJsonObject> data; // Optional additional information

    FJsonRpcErrorObject() : code(0) {}
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
};

USTRUCT()
struct FPromptServerCapabilities
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred
};

USTRUCT()
struct FServerCapabilities // Add more capability structs as needed (e.g., roots, sampling)
{
    GENERATED_BODY()

    UPROPERTY()
    TSharedPtr<FToolServerCapabilities> tools;

    UPROPERTY()
    TSharedPtr<FResourceServerCapabilities> resources;

    UPROPERTY()
    TSharedPtr<FPromptServerCapabilities> prompts;

    FServerCapabilities()
    {
        tools = MakeShared<FToolServerCapabilities>();
        resources = MakeShared<FResourceServerCapabilities>();
        prompts = MakeShared<FPromptServerCapabilities>();
    }
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
};

USTRUCT()
struct FInitializeResult
{
    GENERATED_BODY()

    UPROPERTY()
    FString protocolVersion; // Server's chosen protocol version

    UPROPERTY()
    TSharedPtr<FServerInformation> serverInfo;

    UPROPERTY()
    TSharedPtr<FServerCapabilities> serverCapabilities;

    FInitializeResult()
    {
        serverInfo = MakeShared<FServerInformation>();
        serverCapabilities = MakeShared<FServerCapabilities>();
    }
};
