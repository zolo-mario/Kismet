#include "K2Node_DoOnce.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "EditorCategoryUtils.h"
#include "Styling/AppStyle.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetCompilerMisc.h"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "K2Node_DoOnce"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_DoOnce

class FKCHandler_DoOnce : public FNodeHandlingFunctor
{
public:
	FKCHandler_DoOnce(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_DoOnce* DoOnceNode = CastChecked<UK2Node_DoOnce>(Node);

		// Register standard pins first
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Create a boolean variable to track if it has been executed
		FBPTerminal* ExecutedTerm = Context.CreateLocalTerminal();
		ExecutedTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		ExecutedTerm->Type.PinSubCategory = NAME_None;
		ExecutedTerm->Type.PinSubCategoryObject = nullptr;
		ExecutedTerm->Source = DoOnceNode;
		ExecutedTerm->Name = Context.NetNameMap->MakeValidName(DoOnceNode, TEXT("DoOnceExecuted"));
		ExecutedTerm->bIsLiteral = false;
		Context.Literals.Add(ExecutedTerm);
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_DoOnce* DoOnceNode = CastChecked<UK2Node_DoOnce>(Node);

		// Get the pins
		UEdGraphPin* EnterPin = DoOnceNode->GetEnterPin();
		UEdGraphPin* ResetPin = DoOnceNode->GetResetPin();
		UEdGraphPin* CompletedPin = DoOnceNode->GetCompletedPin();

		// Validate all pins exist
		if (!EnterPin || !CompletedPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("DoOnceMissingPins", "DoOnce node missing required pins").ToString(), DoOnceNode);
			return;
		}

		// Create the custom ZVX statement for DoOnce
		FBlueprintCompiledStatement& DoOnceStatement = Context.AppendZvxStatementForNode(Node);
		
		// Handle Enter pin execution
		if (EnterPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& EnterStatement = GenerateSimpleThenGoto(Context, *DoOnceNode, EnterPin);
			DoOnceStatement.ZvxStatementMap.Add(TEXT("Enter"), &EnterStatement);
		}

		// Handle Reset pin execution
		if (ResetPin && ResetPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& ResetStatement = GenerateSimpleThenGoto(Context, *DoOnceNode, ResetPin);
			DoOnceStatement.ZvxStatementMap.Add(TEXT("Reset"), &ResetStatement);
		}

		// Handle Completed pin execution
		if (CompletedPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& CompletedStatement = GenerateSimpleThenGoto(Context, *DoOnceNode, CompletedPin);
			DoOnceStatement.ZvxStatementMap.Add(TEXT("Completed"), &CompletedStatement);
		}
	}
};

// Pin names
const FName UK2Node_DoOnce::EnterPinName(TEXT("Enter"));
const FName UK2Node_DoOnce::ResetPinName(TEXT("Reset"));
const FName UK2Node_DoOnce::CompletedPinName(TEXT("Completed"));

UK2Node_DoOnce::UK2Node_DoOnce(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_DoOnce::AllocateDefaultPins()
{
	// Enter execution input (unlabeled in documentation)
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, EnterPinName);
	
	// Reset execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ResetPinName);
	
	// Completed execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, CompletedPinName);

	Super::AllocateDefaultPins();
}

FText UK2Node_DoOnce::GetTooltipText() const
{
	return LOCTEXT("DoOnceTooltip", "Fires off an execution pulse just once. From that point forward, it will cease all outgoing execution until a pulse is sent into its Reset input.");
}

FText UK2Node_DoOnce::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("DoOnce", "Do Once");
}

FText UK2Node_DoOnce::GetMenuCategory() const
{
	return LOCTEXT("DoOnceCategory", "Zvx");
}

FSlateIcon UK2Node_DoOnce::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Macro.Loop_16x");
	return Icon;
}

void UK2Node_DoOnce::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FNodeHandlingFunctor* UK2Node_DoOnce::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_DoOnce(CompilerContext);
}

void UK2Node_DoOnce::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

// Pin getter functions
UEdGraphPin* UK2Node_DoOnce::GetEnterPin() const
{
	return FindPin(EnterPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_DoOnce::GetResetPin() const
{
	return FindPin(ResetPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_DoOnce::GetCompletedPin() const
{
	return FindPin(CompletedPinName, EGPD_Output);
}

#undef LOCTEXT_NAMESPACE