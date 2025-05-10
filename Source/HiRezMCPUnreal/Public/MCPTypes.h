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

    // Constructors from specific types
    FJsonRpcId(const FString& InString) : Value(MakeShared<FJsonValueString>(InString)) {}
    FJsonRpcId(const TCHAR* InString) : Value(MakeShared<FJsonValueString>(FString(InString))) {}
    FJsonRpcId(int32 InNumber) : Value(MakeShared<FJsonValueNumber>(static_cast<double>(InNumber))) {}

    // Constructor for an explicit JSON null ID
    static FJsonRpcId CreateNullId()
    {
        return FJsonRpcId(MakeShared<FJsonValueNull>());
    }
	
    // Create from a generic FJsonValue (e.g., when parsing from a FJsonObject)
    static FJsonRpcId CreateFromJsonValue(const TSharedPtr<FJsonValue>& JsonValue)
    {
        // If JsonValue is nullptr (e.g. field was not found in JsonObject), this correctly results in an 'absent' ID.
        // If JsonValue is a valid FJsonValueNull, it correctly results in a 'null' ID.
        return FJsonRpcId(JsonValue);
    }

    // Type checking methods
    bool IsString() const { return Value.IsValid() && Value->Type == EJson::String; }
    bool IsNumber() const { return Value.IsValid() && Value->Type == EJson::Number; }
    bool IsNull() const { return !Value.IsValid() || Value->Type == EJson::Null; }

    // Accessor for the underlying FJsonValue, needed for serialization
    TSharedPtr<FJsonValue> GetJsonValue() const 
    {
    	if (!Value.IsValid())
    	{
    		return MakeShared<FJsonValueNull>();
    	}
        // If Value is nullptr (absent ID), this will return nullptr.
        // If the request had no ID, the response should also have no ID field.
        // If the request ID was explicitly null, this returns an FJsonValueNull, and response ID will be null.
        return Value; 
    }

    // Utility for logging or debugging
    FString ToString() const
    {
        if (!Value.IsValid()) return TEXT("[null]");
        switch (Value->Type)
        {
            case EJson::String: return Value->AsString();
            case EJson::Number: return FString::Printf(TEXT("%g"), Value->AsNumber());
            case EJson::Null:   return TEXT("[null]");
            default: // Should not happen for a valid ID (boolean, array, object)
                return TEXT("[invalid_id_type]"); 
        }
    }
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

    // ID is now FJsonRpcId to encapsulate its type (string, number, null, or absent)
    FJsonRpcId id;

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
        
        // Use id.GetJsonValue() which can be nullptr if ID is absent
        TSharedPtr<FJsonValue> IdValue = id.GetJsonValue();
        if (IdValue.IsValid()) 
        {
            JsonObject->SetField(TEXT("id"), IdValue);
        }
        // If IdValue is nullptr (ID is absent), the 'id' field is correctly omitted from the JSON.

        return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&OutJsonString));
    }

    static bool CreateFromJsonString(const FString& JsonString, FJsonRpcRequest& OutRequest)
    {
        TSharedPtr<FJsonObject> RootJsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

        if (!FJsonSerializer::Deserialize(Reader, RootJsonObject) || !RootJsonObject.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("FJsonRpcRequest::CreateFromJsonString: Failed to deserialize JsonString. String: %s"), *JsonString);
            return false;
        }

        if (!RootJsonObject->TryGetStringField(TEXT("jsonrpc"), OutRequest.jsonrpc) ||
            !RootJsonObject->TryGetStringField(TEXT("method"), OutRequest.method))
        {
            UE_LOG(LogTemp, Error, TEXT("FJsonRpcRequest::CreateFromJsonString: Missing 'jsonrpc' or 'method'. String: %s"), *JsonString);
            return false;
        }

        // Get the "id" field as an FJsonValue if it exists, then create FJsonRpcId from it.
        // If "id" field is not present in JSON, GetField returns nullptr, 
        // and CreateFromJsonValue correctly creates an 'absent' FJsonRpcId.
        if (RootJsonObject->HasField(TEXT("id")))
        {
            OutRequest.id = FJsonRpcId::CreateFromJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("id")));
        }
        else
        {
            OutRequest.id = FJsonRpcId::CreateNullId();
        }
        
        if (RootJsonObject->HasTypedField<EJson::Object>(TEXT("params")))
        {
            OutRequest.params = RootJsonObject->GetObjectField(TEXT("params"));
        }
        else
        {
            // Params are optional, so if not present or not an object, OutRequest.params remains nullptr.
            // This is valid for methods that don't require params. If 'initialize' is called without params,
            // the subsequent check 'RpcRequest.params.IsValid()' will correctly fail for it.
            OutRequest.params = nullptr; 
        }

        return true;
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

    // data can be any JSON value, using FJsonValue to represent it.
    TSharedPtr<FJsonValue> data; 

    FJsonRpcErrorObject() : code(0) {}
    FJsonRpcErrorObject(EMCPErrorCode InErrorCode, const FString& InMessage, TSharedPtr<FJsonValue> InData = nullptr)
        : code(static_cast<int32>(InErrorCode)), message(InMessage), data(InData) {}

    bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
    {
        // FJsonObjectConverter::UStructToJsonObject will only serialize UPROPERTY members.
        // We need to construct it manually or use a hybrid approach if we want to keep UPROPERTY for code/message.
        OutJsonObject = MakeShared<FJsonObject>();
        OutJsonObject->SetNumberField(TEXT("code"), code);
        OutJsonObject->SetStringField(TEXT("message"), message);
        if (data.IsValid()) // data is TSharedPtr<FJsonValue>
        {
            OutJsonObject->SetField(TEXT("data"), data);
        }
        return true; // Assume success for manual construction
    }

    static bool CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FJsonRpcErrorObject& OutErrorObject)
    {
        if (!JsonObject.IsValid()) return false;

        // Manually parse UPROPERTY fields
        if (!JsonObject->TryGetNumberField(TEXT("code"), OutErrorObject.code)) return false;
        if (!JsonObject->TryGetStringField(TEXT("message"), OutErrorObject.message)) return false;

        if (JsonObject->HasField(TEXT("data")))
        {
            OutErrorObject.data = JsonObject->GetField<EJson::None>(TEXT("data")); // Get data as FJsonValue
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

    // ID is now FJsonRpcId to encapsulate its type (string, number, null, or absent)
    FJsonRpcId id;

    // Result can be any valid JSON value (object, array, string, number, boolean, null)
    TSharedPtr<FJsonValue> result; 
    
    TSharedPtr<FJsonRpcErrorObject> error; // Error object if an error occurred

    FJsonRpcResponse() : jsonrpc(TEXT("2.0")) {}

    bool ToJsonString(FString& OutJsonString) const
    {
        TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
        JsonObject->SetStringField(TEXT("jsonrpc"), jsonrpc);

        // Use id.GetJsonValue() which can be nullptr if ID is absent
        TSharedPtr<FJsonValue> IdValue = id.GetJsonValue();
        if (IdValue.IsValid()) 
        {
            JsonObject->SetField(TEXT("id"), IdValue);
        }
        // If IdValue is nullptr (ID is absent), the 'id' field is correctly omitted from the JSON.

        if (error.IsValid()) // Check if error is valid and conceptually "set"
        {
            TSharedPtr<FJsonObject> ErrorJsonObject;
            // Assuming FJsonRpcErrorObject::ToJsonObject correctly serializes it to ErrorJsonObject
            if (error->ToJsonObject(ErrorJsonObject) && ErrorJsonObject.IsValid())
            {
                JsonObject->SetObjectField(TEXT("error"), ErrorJsonObject);
            }
            else
            {
                // Fallback if error content serialization fails (should be rare)
                TSharedPtr<FJsonObject> FallbackError = MakeShared<FJsonObject>();
                FallbackError->SetNumberField(TEXT("code"), static_cast<int32>(EMCPErrorCode::InternalError));
                FallbackError->SetStringField(TEXT("message"), TEXT("Internal server error during error serialization."));
                JsonObject->SetObjectField(TEXT("error"), FallbackError);
            }
        }
        else if (result.IsValid()) // Only include result if there is no error
        {
            // Set the result field using the FJsonValue, allowing any valid JSON type
            JsonObject->SetField(TEXT("result"), result);
        }
        // If neither error nor result is present (valid for some successful notifications, 
        // or if a method successfully does nothing and returns no data and no error),
        // then neither field is added, which is fine. For responses to requests with an ID,
        // either 'result' or 'error' MUST be present.

        return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&OutJsonString));
    }

    // Optional: CreateFromJsonString for FJsonRpcResponse (less common for server to parse its own responses)
    static bool CreateFromJsonString(const FString& JsonString, FJsonRpcResponse& OutResponse)
    {
        TSharedPtr<FJsonObject> RootJsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

        if (!FJsonSerializer::Deserialize(Reader, RootJsonObject) || !RootJsonObject.IsValid())
        {
            // UE_LOG for error if LogTemp or similar is accessible here
            return false;
        }

        if (!RootJsonObject->TryGetStringField(TEXT("jsonrpc"), OutResponse.jsonrpc))
        {
            return false; 
        }

        // Get the "id" field as an FJsonValue if it exists, then create FJsonRpcId from it.
        // If "id" field is not present in JSON, GetField returns nullptr, 
        // and CreateFromJsonValue correctly creates an 'absent' FJsonRpcId.
        if (RootJsonObject->HasField(TEXT("id")))
        {
            OutResponse.id = FJsonRpcId::CreateFromJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("id")));
        }
        else
        {
            OutResponse.id = FJsonRpcId::CreateNullId();
        }

        if (RootJsonObject->HasTypedField<EJson::Object>(TEXT("error")))
        {
            TSharedPtr<FJsonObject> ErrorJsonObject = RootJsonObject->GetObjectField(TEXT("error"));
            OutResponse.error = MakeShared<FJsonRpcErrorObject>();
            if (!FJsonRpcErrorObject::CreateFromJsonObject(ErrorJsonObject, *OutResponse.error))
            {
                // Failed to parse the error object content
                OutResponse.error = nullptr; // Or handle error parsing failure
            }
        }
        else if (RootJsonObject->HasField(TEXT("result"))) // Result can be any type
        {
            OutResponse.result = RootJsonObject->GetField<EJson::None>(TEXT("result"));
        }
        // If neither 'result' nor 'error' is present, it's an issue for non-notification responses.
        // This basic parser doesn't validate that rule.
        
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
    FServerCapabilities capabilities;

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
