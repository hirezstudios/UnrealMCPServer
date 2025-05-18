#include "UMCP_CommonResources.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UMCP_UriTemplate.h" // For FUMCP_UriTemplate
#include "UnrealMCPServerModule.h"
#include "Engine/Blueprint.h" // Required for UBlueprint
#include "Exporters/Exporter.h" // Required for UExporter

void FUMCP_CommonResources::Register(class FUMCP_Server* Server)
{
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Registering common MCP resources."));

	// Register the resource template for discovery AND functionality
	{
		FUMCP_ResourceTemplateDefinition T3DTemplateDefinition;
		T3DTemplateDefinition.name = TEXT("Blueprint T3D Exporter");
		T3DTemplateDefinition.description = TEXT("Exports the T3D representation of an Unreal Engine Blueprint asset specified by its path using the unreal+t3d://{filepath} URI scheme.");
		T3DTemplateDefinition.mimeType = TEXT("application/vnd.unreal.t3d");
		T3DTemplateDefinition.uriTemplate = TEXT("unreal+t3d://{filepath}");
		// Bind the actual handler for this templated resource
		T3DTemplateDefinition.ReadResource.BindRaw(this, &FUMCP_CommonResources::HandleT3DResourceRequest);

		if (Server->RegisterResourceTemplate(MoveTemp(T3DTemplateDefinition)))
		{
			UE_LOG(LogUnrealMCPServer, Log, TEXT("Registered T3D Blueprint Resource Template (unreal+t3d://{filepath}) for discovery and handling."));
		}
		else
		{
			UE_LOG(LogUnrealMCPServer, Error, TEXT("Failed to register T3D Blueprint Resource Template."));
		}
	}
}

bool FUMCP_CommonResources::HandleT3DResourceRequest(const FUMCP_UriTemplate& UriTemplate, const FUMCP_UriTemplateMatch& Match, TArray<FUMCP_ReadResourceResultContent>& OutContent)
{
	auto& Content = OutContent.AddDefaulted_GetRef();
	Content.uri = Match.Uri;
	
	const TArray<FString>* FilePathPtr = Match.Variables.Find(TEXT("filepath"));
	if (!FilePathPtr || FilePathPtr->IsEmpty() || (*FilePathPtr)[0].IsEmpty())
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("HandleT3DResourceRequest: 'filepath' not found in URI '%s' after matching template '%s'."), *Match.Uri, *UriTemplate.GetUriTemplateStr());
        Content.mimeType = TEXT("text/plain");
        Content.text = TEXT("Error: Missing 'filepath' parameter in URI.");
		return false; 
	}
	
	const FString& BlueprintPath = (*FilePathPtr)[0];
	UE_LOG(LogUnrealMCPServer, Log, TEXT("HandleT3DResourceRequest: Attempting to export Blueprint '%s' from URI '%s'."), *BlueprintPath, *Match.Uri);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to load Blueprint: %s"), *BlueprintPath);
        Content.mimeType = TEXT("text/plain");
        Content.text = FString::Printf(TEXT("Error: Failed to load Blueprint: %s"), *BlueprintPath);
		return false;
	}

	UExporter* Exporter = UExporter::FindExporter(Blueprint, TEXT("T3D"));
	if (!Exporter)
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("Failed to find T3D exporter for Blueprint: %s"), *BlueprintPath);
        Content.mimeType = TEXT("text/plain");
        Content.text = FString::Printf(TEXT("Error: Failed to find T3D exporter for Blueprint: %s"), *BlueprintPath);
		return false;
	}

	FStringOutputDevice OutputDevice;
	const uint32 ExportFlags = PPF_Copy | PPF_ExportsNotFullyQualified;
	Exporter->ExportText(nullptr, Blueprint, TEXT("T3D"), OutputDevice, GWarn, ExportFlags);

	if (OutputDevice.IsEmpty())
	{
		UE_LOG(LogUnrealMCPServer, Warning, TEXT("ExportText did not produce any output for Blueprint: %s. Using exporter: %s."), *BlueprintPath, *Exporter->GetClass()->GetName());
        Content.mimeType = TEXT("text/plain");
        Content.text = FString::Printf(TEXT("Error: ExportText did not produce any output for Blueprint: %s."), *BlueprintPath);
		return false;
	}

	Content.mimeType = TEXT("application/vnd.unreal.t3d");
	Content.text = OutputDevice;
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("Successfully exported Blueprint '%s' to T3D via URI '%s'. Output size: %d"), *BlueprintPath, *Match.Uri, Content.text.Len());
	return true;
}
