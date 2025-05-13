#pragma once

struct FUMCP_UriTemplateMatch
{
	FString Uri;
	TMap<FString, TArray<FString>> Variables;
};

enum class EUMCP_UriTemplateComponentType
{
	Literal,
	VarList,
};

enum class EUMCP_UriTemplateComponentVarSpecType
{
	Normal,
	Exploded,
	Prefixed,
};

struct FUMCP_UriTemplateComponentVarSpec
{
	EUMCP_UriTemplateComponentVarSpecType Type = EUMCP_UriTemplateComponentVarSpecType::Normal;
	int32 MaxLength = 0;
	FString Val;
};

struct FUMCP_UriTemplateComponent
{
	FString Literal;
	FString::ElementType ExpressionOperator;
	TArray<FUMCP_UriTemplateComponentVarSpec> VarSpecs;
	EUMCP_UriTemplateComponentType Type;

	FString::ElementType GetPrefixChar() const;
	FString::ElementType GetSeparatorChar() const;

	static void FromLiteral(FString Literal, FUMCP_UriTemplateComponent& OutComp, FString& OutError);
	static void FromVarList(FString VarList, FUMCP_UriTemplateComponent& OutComp, FString& OutError);

	FString Expand(const TMap<FString, TArray<FString>>& Values) const;
};

struct FUMCP_UriTemplate
{
public:
	FUMCP_UriTemplate() = default;
	FUMCP_UriTemplate(FString InUriTemplateStr);
	
	bool IsValid() const { return Error.IsEmpty(); }
	const FString& ParseError() const { return Error; }
	const FString& GetUriTemplateStr() const { return UriTemplateStr; }

	bool FindMatch(const FString& Uri, FUMCP_UriTemplateMatch& OutMatch) const;
	FString Expand(const TMap<FString, TArray<FString>>& Values) const;

private:
	void TryParseTemplate();

	TArray<FUMCP_UriTemplateComponent> Components;
	FString UriTemplateStr{};
	FString Error{};
};
