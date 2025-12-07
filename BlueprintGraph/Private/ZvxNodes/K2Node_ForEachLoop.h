#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "K2Node_ForEachLoop.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UEdGraph;

UCLASS(MinimalAPI)
class UK2Node_ForEachLoop : public UK2Node
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual bool IsNodePure() const override { return false; }
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	// End of UK2Node interface

	// Pin names
	static const FName ExecPinName;
	static const FName ArrayPinName;
	static const FName LoopBodyPinName;
	static const FName ArrayElementPinName;
	static const FName ArrayIndexPinName;
	static const FName CompletedPinName;

public:
	UEdGraphPin* GetExecPin() const;
	UEdGraphPin* GetArrayPin() const;
	UEdGraphPin* GetLoopBodyPin() const;
	UEdGraphPin* GetArrayElementPin() const;
	UEdGraphPin* GetArrayIndexPin() const;
	UEdGraphPin* GetCompletedPin() const;

private:
	// Update pin types based on array connection
	void RefreshPinTypes();
};