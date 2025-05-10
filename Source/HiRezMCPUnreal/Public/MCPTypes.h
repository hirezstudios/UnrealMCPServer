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

    UPROPERTY()
    FString id; // JSON-RPC ID can be string, number, or null. FString is a safe bet for MCP. Can be empty for notifications.

    FJsonRpcRequest() : jsonrpc(TEXT("2.0")) {}

    bool ToJsonString(FString& OutJsonString) const
    {
        TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
        JsonObject->SetStringField(TEXT("jsonrpc"), jsonrpc);
        JsonObject->SetStringField(TEXT("method"), method);
        if (params.IsValid())
        {
            JsonObject->SetObjectField(TEXT("params"), params);
        }
        JsonObject->SetStringField(TEXT("id"), id);
        return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&OutJsonString));
    }

    static bool CreateFromJsonString(const FString& JsonString, FJsonRpcRequest& OutRequest)
    {
        return FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &OutRequest, 0, 0);
    }
};

USTRUCT()
struct FJsonRpcErrorObject
{
    GENERATED_BODY()

    UPROPERTY()
    int32 code;

    UPROPERTY()
    FString message;

    TSharedPtr<FJsonObject> data; // Optional additional information

    FJsonRpcErrorObject() : code(0) {}
    FJsonRpcErrorObject(EMCPErrorCode InErrorCode, const FString& InMessage, TSharedPtr<FJsonObject> InData = nullptr)
        : code(static_cast<int32>(InErrorCode)), message(InMessage), data(InData) {}

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
    {
        OutJsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        if (OutJsonObject.IsValid() && data.IsValid())
        {
            OutJsonObject->SetObjectField(TEXT("data"), data);
        }
        return OutJsonObject.IsValid();
    }

    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FJsonRpcErrorObject& OutErrorObject)
    {
        if (!JsonObject.IsValid())
        {
            return false;
        }
        if (!FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutErrorObject, 0, 0))
        {
            return false;
        }
        if (JsonObject->HasTypedField<EJson::Object>(TEXT("data")))
        {
            OutErrorObject.data = JsonObject->GetObjectField(TEXT("data"));
        }
        return true;
    }
};

USTRUCT()
struct FJsonRpcResponse
{
    GENERATED_BODY()

    UPROPERTY()
    FString jsonrpc; // Should be "2.0"

    TSharedPtr<FJsonObject> result; // Using TSharedPtr<FJsonObject> for result
    TSharedPtr<FJsonRpcErrorObject> error;

    UPROPERTY()
    FString id; // Must be the same as the request it is responding to.

    FJsonRpcResponse() : jsonrpc(TEXT("2.0")) {}

    bool ToJsonString(FString& OutJsonString) const
    {
        TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
        JsonObject->SetStringField(TEXT("jsonrpc"), jsonrpc);
        JsonObject->SetStringField(TEXT("id"), id);

        if (result.IsValid())
        {
            JsonObject->SetObjectField(TEXT("result"), result);
        }
        else if (error.IsValid()) // Manually handle error field
        {
            TSharedPtr<FJsonObject> ErrorJsonObject;
            if (error->ToJsonObject(ErrorJsonObject) && ErrorJsonObject.IsValid())
            {
                JsonObject->SetObjectField(TEXT("error"), ErrorJsonObject);
            }
            else
            {
                // Fallback if error content serialization fails
                TSharedPtr<FJsonObject> FallbackError = MakeShared<FJsonObject>();
                FallbackError->SetNumberField(TEXT("code"), -32000); // Generic server error
                FallbackError->SetStringField(TEXT("message"), TEXT("Failed to serialize error object content within FJsonRpcResponse."));
                JsonObject->SetObjectField(TEXT("error"), FallbackError);
            }
        }
        // If neither result nor error is present, it's valid for some responses.
        return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&OutJsonString));
    }

    static bool CreateFromJsonString(const FString& JsonString, FJsonRpcResponse& OutResponse)
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            return false;
        }

        // Manually assign standard fields, FJsonObjectConverter::JsonObjectStringToUStruct won't work well due to non-UPROPERTY 'error'
        OutResponse.jsonrpc = JsonObject->GetStringField(TEXT("jsonrpc"));
        OutResponse.id = JsonObject->GetStringField(TEXT("id"));

        if (JsonObject->HasTypedField<EJson::Object>(TEXT("result")))
        {
            OutResponse.result = JsonObject->GetObjectField(TEXT("result"));
        }
        else if (JsonObject->HasTypedField<EJson::Object>(TEXT("error")))
        {
            TSharedPtr<FJsonObject> ErrorJsonObject = JsonObject->GetObjectField(TEXT("error"));
            OutResponse.error = MakeShared<FJsonRpcErrorObject>();
            if (!FJsonRpcErrorObject::CreateFromJsonObject(ErrorJsonObject, *OutResponse.error.Get()))
            {
                // Parsing the error object failed, maybe set a default error or clear it
                OutResponse.error.Reset(); // Or populate with a parse error indication
                // Optionally, we could consider this whole parsing a failure if the error object is malformed.
                // For now, we'll allow the response to be created but with a potentially missing/invalid error.
            }
        }
        return true;
    }
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

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
    {
        OutJsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        return OutJsonObject.IsValid();
    }

    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FServerInformation& OutStruct)
    {
        if (!JsonObject.IsValid()) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
    }
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

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
    {
        OutJsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        return OutJsonObject.IsValid();
    }

    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FToolServerCapabilities& OutStruct)
    {
        if (!JsonObject.IsValid()) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
    }
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

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
    {
        OutJsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        return OutJsonObject.IsValid();
    }

    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FResourceServerCapabilities& OutStruct)
    {
        if (!JsonObject.IsValid()) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
    }
};

USTRUCT()
struct FPromptServerCapabilities
{
    GENERATED_BODY()

    UPROPERTY()
    bool listChanged = false; // Deferred as SSE is deferred

    FPromptServerCapabilities() = default; // Added default constructor

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
    {
        OutJsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        return OutJsonObject.IsValid();
    }

    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FPromptServerCapabilities& OutStruct)
    {
        if (!JsonObject.IsValid()) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
    }
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

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
    {
        OutJsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        return OutJsonObject.IsValid();
    }

    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FServerCapabilities& OutStruct)
    {
        if (!JsonObject.IsValid()) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
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

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
    {
        OutJsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        return OutJsonObject.IsValid();
    }

    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FInitializeParams& OutStruct)
    {
        if (!JsonObject.IsValid()) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
    }
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
    FServerCapabilities serverCapabilities;

    FInitializeResult() = default; // Default constructor is fine, members will be default-initialized

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
    {
        OutJsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        return OutJsonObject.IsValid();
    }

    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FInitializeResult& OutStruct)
    {
        if (!JsonObject.IsValid()) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
    }
};
