
#include "MCPTypes.h"


FJsonRpcId FJsonRpcId::CreateNullId()
{
	return FJsonRpcId(MakeShared<FJsonValueNull>());
}

FJsonRpcId FJsonRpcId::CreateFromJsonValue(const TSharedPtr<FJsonValue>& JsonValue)
{
	// If JsonValue is nullptr (e.g. field was not found in JsonObject), this correctly results in an 'absent' ID.
	// If JsonValue is a valid FJsonValueNull, it correctly results in a 'null' ID.
	return FJsonRpcId(JsonValue);
}

// Type checking methods
bool FJsonRpcId::IsString() const { return Value.IsValid() && Value->Type == EJson::String; }
bool FJsonRpcId::IsNumber() const { return Value.IsValid() && Value->Type == EJson::Number; }
bool FJsonRpcId::IsNull() const { return !Value.IsValid() || Value->Type == EJson::Null; }

TSharedPtr<FJsonValue> FJsonRpcId::GetJsonValue() const 
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

FString FJsonRpcId::ToString() const
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
bool FJsonRpcRequest::ToJsonString(FString& OutJsonString) const
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

bool FJsonRpcRequest::CreateFromJsonString(const FString& JsonString, FJsonRpcRequest& OutRequest)
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
bool FJsonRpcErrorObject::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
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

bool FJsonRpcErrorObject::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FJsonRpcErrorObject& OutErrorObject)
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

bool FJsonRpcResponse::ToJsonString(FString& OutJsonString) const
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

bool FJsonRpcResponse::CreateFromJsonString(const FString& JsonString, FJsonRpcResponse& OutResponse)
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

bool FServerInformation::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FServerInformation::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FServerInformation& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FToolServerCapabilities::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FToolServerCapabilities::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FToolServerCapabilities& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FResourceServerCapabilities::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FResourceServerCapabilities::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FResourceServerCapabilities& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FPromptServerCapabilities::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FPromptServerCapabilities::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FPromptServerCapabilities& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FServerCapabilities::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FServerCapabilities::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FServerCapabilities& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FInitializeParams::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FInitializeParams::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FInitializeParams& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}

bool FInitializeResult::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	return FJsonObjectConverter::UStructToJsonObject(this->StaticStruct(), this, OutJsonObject.ToSharedRef());
}

bool FInitializeResult::CreateFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FInitializeResult& OutStruct)
{
	if (!JsonObject.IsValid()) return false;
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
}
