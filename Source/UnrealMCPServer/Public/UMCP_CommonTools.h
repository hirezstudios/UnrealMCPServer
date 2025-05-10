#pragma once

#include "UMCP_Types.h"

class FUMCP_CommonTools
{
public:
	void RegisterTools(class FUMCP_Server* Server);

private:
	bool ExportBlueprintToT3D(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
};
