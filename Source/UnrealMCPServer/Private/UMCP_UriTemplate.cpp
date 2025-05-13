#include "UMCP_UriTemplate.h"

#include "FindInBlueprintManager.h"


FString::ElementType FUMCP_UriTemplateComponent::GetPrefixChar() const
{
	switch (ExpressionOperator)
	{
	case '=': // Reserved
	case ',': // Reserved
	case '!': // Reserved
	case '@': // Reserved
	case '|': // Reserved
		return 0;
	case '.': // Level 3 - Name lables or extensions prefixed by "."
	case '/': // Level 3 - Path segments prefixed by "/"
	case ';': // Level 3 - Path parameter name or name=value pairs prefixed by ";"
	case '?': // Level 3 - Query component beginning with "?" and consisting of name=value pairs separated by "&"
	case '&': // Level 3 - continuation of query-style &name=value pairs within a literal query component
	case '#': // Level 2 - fragment characters prefixed by "#"
		return ExpressionOperator;
	case '+': // Level 2 - Reserved character strings
	default:
		return 0;
	}
}

FString::ElementType FUMCP_UriTemplateComponent::GetSeparatorChar() const
{
	switch (ExpressionOperator)
	{
	case '=': // Reserved
	case ',': // Reserved
	case '!': // Reserved
	case '@': // Reserved
	case '|': // Reserved
	case '.': // Level 3 - Name lables or extensions prefixed by "."
	case '/': // Level 3 - Path segments prefixed by "/"
	case ';': // Level 3 - Path parameter name or name=value pairs prefixed by ";"
		return ExpressionOperator;
	case '?': // Level 3 - Query component beginning with "?" and consisting of name=value pairs separated by "&"
	case '&': // Level 3 - continuation of query-style &name=value pairs within a literal query component
		return '&';
	case '#': // Level 2 - fragment characters prefixed by "#"
	case '+': // Level 2 - Reserved character strings
	default:
		return ',';
	}
}

void FUMCP_UriTemplateComponent::FromLiteral(FString Literal, FUMCP_UriTemplateComponent& OutComp, FString& OutError)
{
	OutComp.Literal = MoveTemp(Literal);
	OutComp.Type = EUMCP_UriTemplateComponentType::Literal;
}

void FUMCP_UriTemplateComponent::FromVarList(FString VarList, FUMCP_UriTemplateComponent& OutComp, FString& OutError)
{
	if (VarList.IsEmpty())
	{
		OutError = TEXT("VarList must not be empty");
		return;
	}
	switch (VarList[0])
	{
	case '=': // Reserved
	case ',': // Reserved
	case '!': // Reserved
	case '@': // Reserved
	case '|': // Reserved
	case '.': // Level 3 - Name lables or extensions prefixed by "."
	case '/': // Level 3 - Path segments prefixed by "/"
	case ';': // Level 3 - Path parameter name or name=value pairs prefixed by ";"
	case '?': // Level 3 - Query component beginning with "?" and consisting of name=value pairs separated by "&"
	case '&': // Level 3 - continuation of query-style &name=value pairs within a literal query component
	case '+': // Level 2 - Reserved character strings
	case '#': // Level 2 - fragment characters prefixed by "#"
		OutComp.ExpressionOperator = VarList[0];
		VarList = VarList.RightChop(1);
		break;
	default:
		OutComp.ExpressionOperator = 0;
		break;
	}
	TArray<FString> VarSpecs;
	VarList.ParseIntoArray(VarSpecs, TEXT(","));
	if (VarSpecs.IsEmpty())
	{
		OutError = TEXT("VarList found no specs");
		return;
	}

	for (const FString& VarSpecStr : VarSpecs)
	{
		auto& VarSpec = OutComp.VarSpecs.Emplace_GetRef();
		if (VarSpecStr.Len() >= 2 && VarSpecStr[VarSpecStr.Len() - 1] == TEXT('*'))
		{
			VarSpec.Type = EUMCP_UriTemplateComponentVarSpecType::Exploded;
			VarSpec.Val = VarSpecStr.LeftChop(1);
			continue;
		}

		FString VarSpecPrefix, VarSpecMaxLength;
		if (VarSpecStr.Split(TEXT(":"), &VarSpecPrefix, &VarSpecMaxLength, ESearchCase::CaseSensitive))
		{
			VarSpec.Type = EUMCP_UriTemplateComponentVarSpecType::Prefixed;
			LexFromString(VarSpec.MaxLength, *VarSpecMaxLength);
			VarSpec.Val = VarSpecPrefix;
			continue;
		}

		VarSpec.Val = VarSpecStr;
	}
	
	OutComp.Type = EUMCP_UriTemplateComponentType::VarList;
}


FString FUMCP_UriTemplateComponent::Expand(const TMap<FString, TArray<FString>>& Values) const
{
	if (Type == EUMCP_UriTemplateComponentType::Literal)
	{
		return Literal;
	}

	// TODO
	return FString();
}

FUMCP_UriTemplate::FUMCP_UriTemplate(FString InUriTemplateStr)
{
	UriTemplateStr = MoveTemp(InUriTemplateStr);
	TryParseTemplate();
}

void FUMCP_UriTemplate::TryParseTemplate()
{
	FString Temp;
	bool bParsingVarlist = false;
	for (FString::ElementType c : UriTemplateStr)
	{
		if (bParsingVarlist)
		{
			if (c == TEXT('}'))
			{
				FUMCP_UriTemplateComponent::FromVarList(MoveTemp(Temp), Components.Emplace_GetRef(), Error);
				if (!Error.IsEmpty())
				{
					return;
				}
				Temp = FString();
				bParsingVarlist = false;
				continue;
			}
		}
		else
		{
			if (c == TEXT('{'))
			{
				if (!Temp.IsEmpty())
				{
					FUMCP_UriTemplateComponent::FromLiteral(MoveTemp(Temp), Components.Emplace_GetRef(), Error);
					if (!Error.IsEmpty())
					{
						return;
					}
				}
				Temp = FString();
				bParsingVarlist = true;
			}
		}
		Temp.AppendChar(c);
	}

	if (!Temp.IsEmpty())
	{
		FUMCP_UriTemplateComponent::FromLiteral(MoveTemp(Temp), Components.Emplace_GetRef(), Error);
	}
}

bool FUMCP_UriTemplate::FindMatch(const FString& Uri, FUMCP_UriTemplateMatch& OutMatch) const
{
	FStringView UriRemaining = Uri;
	for (auto Itr = Components.CreateConstIterator(); Itr; ++Itr)
	{
		if (UriRemaining.IsEmpty())
		{
			return false;
		}
		
		if (Itr->Type == EUMCP_UriTemplateComponentType::Literal)
		{
			if (!UriRemaining.StartsWith(Itr->Literal, ESearchCase::CaseSensitive))
			{
				return false;
			}
			UriRemaining = UriRemaining.RightChop(Itr->Literal.Len());
			continue;
		}

		if (Itr->Type != EUMCP_UriTemplateComponentType::VarList)
		{
			return false; // something horribly wrong has happened
		}

		const auto RequiredPrefix = Itr->GetPrefixChar();
		if (RequiredPrefix != 0 && UriRemaining[0] != RequiredPrefix)
		{
			return false;
		}

		FStringView ExpressionToMatch = UriRemaining;
		auto Current = Itr;
		if (auto Next = Itr + 1)
		{
			switch (Next->Type)
			{
			case EUMCP_UriTemplateComponentType::Literal:
				{
					auto End = UriRemaining.Find(Next->Literal);
					if (End == INDEX_NONE)
					{
						return false;
					}
					UriRemaining = UriRemaining.RightChop(End + Next->Literal.Len());
					ExpressionToMatch = ExpressionToMatch.LeftChop(End);
					Itr++; // We have successfully parsed the *next* component, so iterate ahead - the loop will do the next one
				}
				break;
			case EUMCP_UriTemplateComponentType::VarList:
				auto PrefixChar = Next->GetPrefixChar();
				if (PrefixChar == 0)
				{
					return false; // I'm not sure we could properly parse this at all
				}
				
				int32 NextStartPos;
				if (!UriRemaining.FindChar(PrefixChar, NextStartPos))
				{
					return false;
				}
				UriRemaining = UriRemaining.RightChop(NextStartPos);
				ExpressionToMatch = ExpressionToMatch.LeftChop(NextStartPos);
				break;
			}
		}

		const auto Separator = Current->GetSeparatorChar();
		TArray<FString> Vars;
		while (!ExpressionToMatch.IsEmpty())
		{
			int32 ExpressionEnd;
			if (ExpressionToMatch.FindChar(Separator, ExpressionEnd))
			{
				Vars.Add(FString(ExpressionToMatch.LeftChop(ExpressionEnd)));
			}
			else
			{
				Vars.Add(FString(ExpressionToMatch));
			}
		}

		// TODO parse out vars based on the Expression Operator

	}
	
	return true;
}

FString FUMCP_UriTemplate::Expand(const TMap<FString, TArray<FString>>& Values) const
{
	FString Result;
	for (const auto& Component : Components)
	{
		Result += Component.Expand(Values);
	}
	return Result;
}
