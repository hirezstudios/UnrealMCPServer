﻿
#include "UMCP_Types.h"


FUMCP_JsonRpcId FUMCP_JsonRpcId::CreateNullId()
{
	return FUMCP_JsonRpcId(MakeShared<FJsonValueNull>());
}

FUMCP_JsonRpcId FUMCP_JsonRpcId::CreateFromJsonValue(const TSharedPtr<FJsonValue>& JsonValue)
{
	// If JsonValue is nullptr (e.g. field was not found in JsonObject), this correctly results in an 'absent' ID.
	// If JsonValue is a valid FJsonValueNull, it correctly results in a 'null' ID.
	return FUMCP_JsonRpcId(JsonValue);
}

// Type checking methods
bool FUMCP_JsonRpcId::IsString() const { return Value.IsValid() && Value->Type == EJson::String; }
bool FUMCP_JsonRpcId::IsNumber() const { return Value.IsValid() && Value->Type == EJson::Number; }
bool FUMCP_JsonRpcId::IsNull() const { return !Value.IsValid() || Value->Type == EJson::Null; }

TSharedPtr<FJsonValue> FUMCP_JsonRpcId::GetJsonValue() const 
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

FString FUMCP_JsonRpcId::ToString() const
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
bool FUMCP_JsonRpcRequest::ToJsonString(FString& OutJsonString) const
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

bool FUMCP_JsonRpcRequest::CreateFromJsonString(const FString& JsonString, FUMCP_JsonRpcRequest& OutRequest)
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
		OutRequest.id = FUMCP_JsonRpcId::CreateFromJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("id")));
	}
	else
	{
		OutRequest.id = FUMCP_JsonRpcId::CreateNullId();
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
bool FUMCP_JsonRpcError::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	// FJsonObjectConverter::UStructToJsonObject will only serialize UPROPERTY members.
	// We need to construct it manually or use a hybrid approach if we want to keep UPROPERTY for code/message.
	OutJsonObject->SetNumberField(TEXT("code"), code);
	OutJsonObject->SetStringField(TEXT("message"), message);
	if (data.IsValid()) // data is TSharedPtr<FJsonValue>
	{
		OutJsonObject->SetField(TEXT("data"), data);
	}
	return true; // Assume success for manual construction
}

bool FUMCP_JsonRpcError::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_JsonRpcError& OutErrorObject)
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

bool FUMCP_JsonRpcResponse::ToJsonString(FString& OutJsonString) const
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
		TSharedPtr<FJsonObject> ErrorJsonObject = MakeShared<FJsonObject>();
		// Assuming FJsonRpcErrorObject::ToJsonObject correctly serializes it to ErrorJsonObject
		if (error->ToJsonObject(ErrorJsonObject) && ErrorJsonObject.IsValid())
		{
			JsonObject->SetObjectField(TEXT("error"), ErrorJsonObject);
		}
		else
		{
			// Fallback if error content serialization fails (should be rare)
			TSharedPtr<FJsonObject> FallbackError = MakeShared<FJsonObject>();
			FallbackError->SetNumberField(TEXT("code"), static_cast<int32>(EUMCP_JsonRpcErrorCode::InternalError));
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

bool FUMCP_JsonRpcResponse::CreateFromJsonString(const FString& JsonString, FUMCP_JsonRpcResponse& OutResponse)
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
		OutResponse.id = FUMCP_JsonRpcId::CreateFromJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("id")));
	}
	else
	{
		OutResponse.id = FUMCP_JsonRpcId::CreateNullId();
	}

	if (RootJsonObject->HasTypedField<EJson::Object>(TEXT("error")))
	{
		TSharedPtr<FJsonObject> ErrorJsonObject = RootJsonObject->GetObjectField(TEXT("error"));
		OutResponse.error = MakeShared<FUMCP_JsonRpcError>();
		if (!FUMCP_JsonRpcError::CreateFromJsonObject(ErrorJsonObject, *OutResponse.error))
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

bool FUMCP_ServerInfo::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FUMCP_ServerInfo::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_ServerInfo& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FUMCP_ServerCapabilitiesTools::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FUMCP_ServerCapabilitiesTools::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_ServerCapabilitiesTools& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FUMCP_ServerCapabilitiesResources::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FUMCP_ServerCapabilitiesResources::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_ServerCapabilitiesResources& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FUMCP_ServerCapabilitiesPrompts::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FUMCP_ServerCapabilitiesPrompts::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_ServerCapabilitiesPrompts& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FUMCP_ServerCapabilities::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FUMCP_ServerCapabilities::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_ServerCapabilities& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FUMCP_InitializeParams::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FUMCP_InitializeParams::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_InitializeParams& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FUMCP_InitializeResult::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FUMCP_InitializeResult::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FUMCP_InitializeResult& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}
