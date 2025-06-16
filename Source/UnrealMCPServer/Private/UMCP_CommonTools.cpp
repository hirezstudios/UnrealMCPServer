#include "UMCP_CommonTools.h"
#include "UMCP_Server.h"
#include "UMCP_Types.h"
#include "UnrealMCPServerModule.h"
#include "Exporters/Exporter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "BlueprintGraph/Classes/K2Node.h"


namespace
{
	TSharedPtr<FJsonObject> FromJsonStr(const FString& Str)
	{
		TSharedPtr<FJsonObject> RootJsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Str);

		if (!FJsonSerializer::Deserialize(Reader, RootJsonObject) || !RootJsonObject.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("FJsonRpcRequest::CreateFromJsonString: Failed to deserialize JsonString. String: %s"), *Str);
			return nullptr;
		}

		return RootJsonObject;
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
	
	{
		FUMCP_ToolDefinition Tool;
		Tool.name = TEXT("search_blueprints");
		Tool.description = TEXT("Search for Blueprint assets based on various criteria including name patterns, parent classes, and package paths.");
		Tool.DoToolCall.BindRaw(this, &FUMCP_CommonTools::SearchBlueprints);
		Tool.inputSchema = FromJsonStr(TEXT(R"({
			"type": "object",
			"properties": {
				"searchType": {
					"type": "string",
					"enum": ["name", "parent_class", "all"],
					"description": "Type of search to perform: 'name' for name pattern matching, 'parent_class' for finding Blueprint subclasses, 'all' for comprehensive search"
				},
				"searchTerm": {
					"type": "string",
					"description": "Search term (Blueprint name pattern, parent class name, etc.)."
				},
				"packagePath": {
					"type": "string",
					"description": "Optional package path to limit search scope (e.g., '/Game/Blueprints'). If not specified, searches entire project."
				},
				"recursive": {
					"type": "boolean",
					"description": "Whether to search recursively in subfolders. Defaults to true."
				}
			},
			"required": ["searchType", "searchTerm"]
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

	FString BlueprintPath = arguments->GetStringField(TEXT("BlueprintPath"));
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

bool FUMCP_CommonTools::SearchBlueprints(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
{
	auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
	Content.type = TEXT("text");

	// Parse input parameters
	FString SearchType = arguments->GetStringField(TEXT("searchType"));
	FString SearchTerm = arguments->GetStringField(TEXT("searchTerm"));
	FString PackagePath = arguments->GetStringField(TEXT("packagePath"));
	bool bRecursive = arguments->GetBoolField(TEXT("recursive"));

	// Validate required parameters
	if (SearchType.IsEmpty() || SearchTerm.IsEmpty())
	{
		Content.text = TEXT("Missing required parameters: searchType and searchTerm are required.");
		return false;
	}

	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchBlueprints: Type=%s, Term=%s, Path=%s, Recursive=%s"), 
		*SearchType, *SearchTerm, *PackagePath, bRecursive ? TEXT("true") : TEXT("false"));

	// Get Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Prepare search filter
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	
	// Add package path filter if specified
	if (!PackagePath.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PackagePath));
		Filter.bRecursivePaths = bRecursive;
	}

	// Perform asset search
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchBlueprints: Found %d Blueprint assets before filtering"), AssetDataList.Num());

	// Build results JSON
	TSharedPtr<FJsonObject> ResultsJson = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	int32 TotalMatches = 0;

	for (const FAssetData& AssetData : AssetDataList)
	{
		bool bMatches = false;
		TArray<TSharedPtr<FJsonValue>> MatchesArray;

		// Apply search type filters
		if (SearchType == TEXT("name") || SearchType == TEXT("all"))
		{
			if (AssetData.AssetName.ToString().Contains(SearchTerm))
			{
				bMatches = true;
				
				// Add match detail
				TSharedPtr<FJsonObject> MatchJson = MakeShareable(new FJsonObject);
				MatchJson->SetStringField(TEXT("type"), TEXT("asset_name"));
				MatchJson->SetStringField(TEXT("location"), TEXT("Blueprint Asset"));
				MatchJson->SetStringField(TEXT("context"), FString::Printf(TEXT("Blueprint name '%s' contains '%s'"), 
					*AssetData.AssetName.ToString(), *SearchTerm));
				MatchesArray.Add(MakeShareable(new FJsonValueObject(MatchJson)));
			}
		}

		if (SearchType == TEXT("parent_class") || SearchType == TEXT("all"))
		{
			// Get parent class information from asset tags
			FString ParentClassPath;
			if (AssetData.GetTagValue(TEXT("ParentClass"), ParentClassPath))
			{
				if (ParentClassPath.Contains(SearchTerm))
				{
					bMatches = true;
					
					// Add match detail
					TSharedPtr<FJsonObject> MatchJson = MakeShareable(new FJsonObject);
					MatchJson->SetStringField(TEXT("type"), TEXT("parent_class"));
					MatchJson->SetStringField(TEXT("location"), TEXT("Blueprint Asset"));
					MatchJson->SetStringField(TEXT("context"), FString::Printf(TEXT("Parent class '%s' contains '%s'"), 
						*ParentClassPath, *SearchTerm));
					MatchesArray.Add(MakeShareable(new FJsonValueObject(MatchJson)));
				}
			}
		}

		// If this Blueprint matches, add it to results
		if (bMatches)
		{
			TotalMatches++;
			
			TSharedPtr<FJsonObject> BlueprintResult = MakeShareable(new FJsonObject);
			BlueprintResult->SetStringField(TEXT("assetPath"), AssetData.GetSoftObjectPath().ToString());
			BlueprintResult->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
			BlueprintResult->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
			
			// Get parent class for display
			FString ParentClassPath;
			AssetData.GetTagValue(TEXT("ParentClass"), ParentClassPath);
			BlueprintResult->SetStringField(TEXT("parentClass"), ParentClassPath);
			
			BlueprintResult->SetArrayField(TEXT("matches"), MatchesArray);
			
			ResultsArray.Add(MakeShareable(new FJsonValueObject(BlueprintResult)));
		}
	}

	// Build final result JSON
	ResultsJson->SetArrayField(TEXT("results"), ResultsArray);
	ResultsJson->SetNumberField(TEXT("totalResults"), TotalMatches);
	
	TSharedPtr<FJsonObject> SearchCriteriaJson = MakeShareable(new FJsonObject);
	SearchCriteriaJson->SetStringField(TEXT("searchType"), SearchType);
	SearchCriteriaJson->SetStringField(TEXT("searchTerm"), SearchTerm);
	if (!PackagePath.IsEmpty())
	{
		SearchCriteriaJson->SetStringField(TEXT("packagePath"), PackagePath);
	}
	SearchCriteriaJson->SetBoolField(TEXT("recursive"), bRecursive);
	ResultsJson->SetObjectField(TEXT("searchCriteria"), SearchCriteriaJson);

	// Convert to JSON string
	FString ResultJsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJsonString);
	FJsonSerializer::Serialize(ResultsJson.ToSharedRef(), Writer);

	Content.text = ResultJsonString;
	
	UE_LOG(LogUnrealMCPServer, Log, TEXT("SearchBlueprints: Completed search, found %d matches"), TotalMatches);
	
	return true;
}
