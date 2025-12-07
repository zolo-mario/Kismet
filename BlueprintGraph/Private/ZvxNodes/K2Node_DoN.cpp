#include "K2Node_DoN.h"
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

#define LOCTEXT_NAMESPACE "K2Node_DoN"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_DoN

class FKCHandler_DoN : public FNodeHandlingFunctor
{
public:
	FKCHandler_DoN(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_DoN* DoNNode = CastChecked<UK2Node_DoN>(Node);

		// Register standard pins first
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Create a counter variable to track executions
		FBPTerminal* CounterTerm = Context.CreateLocalTerminal();
		CounterTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Int;
		CounterTerm->Type.PinSubCategory = NAME_None;
		CounterTerm->Type.PinSubCategoryObject = nullptr;
		CounterTerm->Source = DoNNode;
		CounterTerm->Name = Context.NetNameMap->MakeValidName(DoNNode, TEXT("DoNCounter"));
		CounterTerm->bIsLiteral = false;
		Context.Literals.Add(CounterTerm);
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_DoN* DoNNode = CastChecked<UK2Node_DoN>(Node);

		// Get the pins
		UEdGraphPin* EnterPin = DoNNode->GetEnterPin();
		UEdGraphPin* NPin = DoNNode->GetNPin();
		UEdGraphPin* ResetPin = DoNNode->GetResetPin();
		UEdGraphPin* ExitPin = DoNNode->GetExitPin();

		// Validate all pins exist
		if (!EnterPin || !NPin || !ExitPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("DoNMissingPins", "DoN node missing required pins").ToString(), DoNNode);
			return;
		}

		// Get the terms for pins
		UEdGraphPin* NNet = FEdGraphUtilities::GetNetFromPin(NPin);
		FBPTerminal* NTerm = Context.NetMap.FindRef(NNet);

		if (!NTerm)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("DoNMissingTerms", "DoN node missing required terms").ToString(), DoNNode);
			return;
		}

		// Create the custom ZVX statement for DoN
		FBlueprintCompiledStatement& DoNStatement = Context.AppendZvxStatementForNode(Node);
		
		// Handle Enter pin execution
		if (EnterPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& EnterStatement = GenerateSimpleThenGoto(Context, *DoNNode, EnterPin);
			DoNStatement.ZvxStatementMap.Add(TEXT("Enter"), &EnterStatement);
		}

		// Handle Reset pin execution
		if (ResetPin && ResetPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& ResetStatement = GenerateSimpleThenGoto(Context, *DoNNode, ResetPin);
			DoNStatement.ZvxStatementMap.Add(TEXT("Reset"), &ResetStatement);
		}

		// Handle Exit pin execution
		if (ExitPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& ExitStatement = GenerateSimpleThenGoto(Context, *DoNNode, ExitPin);
			DoNStatement.ZvxStatementMap.Add(TEXT("Exit"), &ExitStatement);
		}
		
		DoNStatement.ZvxTerminalMap.Add(TEXT("N"), NTerm);
	}
};

// Pin names
const FName UK2Node_DoN::EnterPinName(TEXT("Enter"));
const FName UK2Node_DoN::NPinName(TEXT("n"));
const FName UK2Node_DoN::ResetPinName(TEXT("Reset"));
const FName UK2Node_DoN::ExitPinName(TEXT("Exit"));

UK2Node_DoN::UK2Node_DoN(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_DoN::AllocateDefaultPins()
{
	// Enter execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, EnterPinName);
	
	// N input (integer) with default value
	UEdGraphPin* NPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, NPinName);
	NPin->DefaultValue = TEXT("1");
	
	// Reset execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ResetPinName);
	
	// Exit execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, ExitPinName);

	Super::AllocateDefaultPins();
}

FText UK2Node_DoN::GetTooltipText() const
{
	return LOCTEXT("DoNTooltip", "Fires off an execution pulse N times. After the limit has been reached, it will cease all outgoing execution until a pulse is sent into its Reset input.");
}

FText UK2Node_DoN::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("DoN", "Do N");
}

FText UK2Node_DoN::GetMenuCategory() const
{
	return LOCTEXT("DoNCategory", "Zvx");
}

FSlateIcon UK2Node_DoN::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Macro.Loop_16x");
	return Icon;
}

void UK2Node_DoN::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FNodeHandlingFunctor* UK2Node_DoN::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_DoN(CompilerContext);
}

void UK2Node_DoN::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

// Pin getter functions
UEdGraphPin* UK2Node_DoN::GetEnterPin() const
{
	return FindPin(EnterPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_DoN::GetNPin() const
{
	return FindPin(NPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_DoN::GetResetPin() const
{
	return FindPin(ResetPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_DoN::GetExitPin() const
{
	return FindPin(ExitPinName, EGPD_Output);
}

#undef LOCTEXT_NAMESPACE