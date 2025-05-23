#include "UMCP_CommonTools.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UnrealMCPServerModule.h"
#include "Exporters/Exporter.h"


namespace
{
	TSharedPtr<FJsonObject> FromJsonStr(const FString& Str)
	{
		TSharedPtr<FJsonObject> RootJsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Str);

		if (!FJsonSerializer::Deserialize(Reader, RootJsonObject) || !RootJsonObject.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("FJsonRpcRequest::CreateFromJsonString: Failed to deserialize JsonString. String: %s"), *Str);
			return RootJsonObject;
		}

		return nullptr;
	}
}


void FUMCP_CommonTools::Register(class FUMCP_Server* Server)
{
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("export_blueprint_to_t3d");
		Tool.description = TEXT("Export a blueprint's contents to T3D format.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::ExportBlueprintToT3D);
		Tool.inputSchema = FromJsonStr(TEXT(R"({
			"type": "object",
			"properties": {
				"BlueprintPath": {
					"name": "BlueprintPath",
					"description": "The path to the blueprint to export",
					"type": "string"
				}
			},
			"required": ["BlueprintPath"]
		})"));
		Server->RegisterTool(MoveTemp(Tool));
	}
	
	// {
	//   	FUMCP_ToolDefinition Tool;
	//   	Tool.name = TEXT("some_cool_tool_name_here");
	//   	Tool.description = TEXT("Simple description of what the tool does");
	//   	Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::FunctionToExecuteTheTool);
	//   	Tool.inputSchema = FromJsonStr(TEXT(R"({
	//   		"type": "object",
	//   		"properties": {
	//   			"FirstParameterThatTheToolAccepts": {
	//   				"name": "FirstParameterThatTheToolAccepts",
	//   				"description": "description of what the parameter is used for with the tool",
	//   				"type": "string"
	//   			}
	//   		},
	//   		"required": ["FirstParameterThatTheToolAccepts"]
	//   	})"));
	//   	Server->RegisterTool(MoveTemp(Tool));
	// }
}

bool FUMCP_CommonTools::ExportBlueprintToT3D(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	FString BlueprintPath = arguments->GetStringField("BlueprintPath");
	if (BlueprintPath.IsEmpty())
	{
		Content.text = TEXT("Missing BlueprintPath parameter.");
		return false;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		Content.text = FString::Printf(TEXT("Failed to load Blueprint: %s"), *BlueprintPath);
		return false;
	}

	UExporter* Exporter = UExporter::FindExporter(Blueprint, TEXT("T3D"));
	if (!Exporter)
	{
		Content.text = FString::Printf(TEXT("Failed to find T3D exporter for Blueprint: %s"), *BlueprintPath);
		return false;
	}

	FStringOutputDevice OutputDevice;
	const uint32 ExportFlags = PPF_Copy | PPF_ExportsNotFullyQualified;
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Attempting to export Blueprint '%s' to T3D format using exporter: %s"), *BlueprintPath, *Exporter->GetClass()->GetName());
	Exporter->ExportText(nullptr, Blueprint, TEXT("T3D"), OutputDevice, GWarn, ExportFlags);
	if (OutputDevice.IsEmpty())
	{
		Content.text = FString::Printf(TEXT("ExportText did not produce any output for Blueprint: %s. Using exporter: %s."), *BlueprintPath, *Exporter->GetClass()->GetName());
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("%s"), *Content.text);
		return false;
	}

	Content.text = OutputDevice;
	return true;
}
