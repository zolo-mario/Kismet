#include "K2Node_Gate.h"
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

#define LOCTEXT_NAMESPACE "K2Node_Gate"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_Gate

class FKCHandler_Gate : public FNodeHandlingFunctor
{
public:
	FKCHandler_Gate(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_Gate* GateNode = CastChecked<UK2Node_Gate>(Node);

		// Register standard pins first
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Create a boolean variable to track gate state (true = open, false = closed)
		FBPTerminal* GateStateTerm = Context.CreateLocalTerminal();
		GateStateTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		GateStateTerm->Type.PinSubCategory = NAME_None;
		GateStateTerm->Type.PinSubCategoryObject = nullptr;
		GateStateTerm->Source = GateNode;
		GateStateTerm->Name = Context.NetNameMap->MakeValidName(GateNode, TEXT("GateState"));
		GateStateTerm->bIsLiteral = false;
		Context.Literals.Add(GateStateTerm);

		// Handle StartClosed pin default value
		UEdGraphPin* StartClosedPin = GateNode->GetStartClosedPin();
		if (StartClosedPin && StartClosedPin->LinkedTo.Num() == 0)
		{
			// Default to false (gate starts open)
			StartClosedPin->DefaultValue = TEXT("false");
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_Gate* GateNode = CastChecked<UK2Node_Gate>(Node);

		// Get the pins
		UEdGraphPin* EnterPin = GateNode->GetEnterPin();
		UEdGraphPin* OpenPin = GateNode->GetOpenPin();
		UEdGraphPin* ClosePin = GateNode->GetClosePin();
		UEdGraphPin* TogglePin = GateNode->GetTogglePin();
		UEdGraphPin* ExitPin = GateNode->GetExitPin();
		UEdGraphPin* StartClosedPin = GateNode->GetStartClosedPin();

		// Validate required pins exist
		if (!EnterPin || !ExitPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("GateMissingPins", "Gate node missing required pins").ToString(), GateNode);
			return;
		}

		// Get the terms for StartClosed pin
		UEdGraphPin* StartClosedNet = FEdGraphUtilities::GetNetFromPin(StartClosedPin);
		FBPTerminal* StartClosedTerm = Context.NetMap.FindRef(StartClosedNet);

		// Create the custom ZVX statement for Gate
		FBlueprintCompiledStatement& GateStatement = Context.AppendZvxStatementForNode(Node);
		
		// Handle Enter pin execution
		if (EnterPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& EnterStatement = GenerateSimpleThenGoto(Context, *GateNode, EnterPin);
			GateStatement.ZvxStatementMap.Add(TEXT("Enter"), &EnterStatement);
		}

		// Handle Open pin execution
		if (OpenPin && OpenPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& OpenStatement = GenerateSimpleThenGoto(Context, *GateNode, OpenPin);
			GateStatement.ZvxStatementMap.Add(TEXT("Open"), &OpenStatement);
		}

		// Handle Close pin execution
		if (ClosePin && ClosePin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& CloseStatement = GenerateSimpleThenGoto(Context, *GateNode, ClosePin);
			GateStatement.ZvxStatementMap.Add(TEXT("Close"), &CloseStatement);
		}

		// Handle Toggle pin execution
		if (TogglePin && TogglePin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& ToggleStatement = GenerateSimpleThenGoto(Context, *GateNode, TogglePin);
			GateStatement.ZvxStatementMap.Add(TEXT("Toggle"), &ToggleStatement);
		}

		// Handle Exit pin execution
		if (ExitPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& ExitStatement = GenerateSimpleThenGoto(Context, *GateNode, ExitPin);
			GateStatement.ZvxStatementMap.Add(TEXT("Exit"), &ExitStatement);
		}

		// Add StartClosed terminal to ZVX statement
		if (StartClosedTerm)
		{
			GateStatement.ZvxTerminalMap.Add(TEXT("StartClosed"), StartClosedTerm);
		}
	}
};

// Pin names
const FName UK2Node_Gate::EnterPinName(TEXT(""));
const FName UK2Node_Gate::OpenPinName(TEXT("Open"));
const FName UK2Node_Gate::ClosePinName(TEXT("Close"));
const FName UK2Node_Gate::TogglePinName(TEXT("Toggle"));
const FName UK2Node_Gate::ExitPinName(TEXT("Exit"));
const FName UK2Node_Gate::StartClosedPinName(TEXT("Start Closed"));

UK2Node_Gate::UK2Node_Gate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_Gate::AllocateDefaultPins()
{
	// Enter execution input (unlabeled)
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, EnterPinName);
	
	// Open execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, OpenPinName);
	
	// Close execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ClosePinName);
	
	// Toggle execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, TogglePinName);
	
	// Exit execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, ExitPinName);
	
	// Start Closed boolean input
	UEdGraphPin* StartClosedPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, StartClosedPinName);
	StartClosedPin->DefaultValue = TEXT("false");

	Super::AllocateDefaultPins();
}

FText UK2Node_Gate::GetTooltipText() const
{
	return LOCTEXT("GateTooltip", "The Gate acts as a valve for execution flow. When open, executions from the Enter input will pass through to the Exit output. When closed, they will not. Can be opened, closed, or toggled by other execution flows.");
}

FText UK2Node_Gate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Gate", "Gate");
}

FText UK2Node_Gate::GetMenuCategory() const
{
	return LOCTEXT("GateCategory", "Zvx");
}

FSlateIcon UK2Node_Gate::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Gate_16x");
	return Icon;
}

void UK2Node_Gate::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FNodeHandlingFunctor* UK2Node_Gate::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_Gate(CompilerContext);
}

void UK2Node_Gate::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

// Pin getter functions
UEdGraphPin* UK2Node_Gate::GetEnterPin() const
{
	return FindPin(EnterPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_Gate::GetOpenPin() const
{
	return FindPin(OpenPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_Gate::GetClosePin() const
{
	return FindPin(ClosePinName, EGPD_Input);
}

UEdGraphPin* UK2Node_Gate::GetTogglePin() const
{
	return FindPin(TogglePinName, EGPD_Input);
}

UEdGraphPin* UK2Node_Gate::GetExitPin() const
{
	return FindPin(ExitPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_Gate::GetStartClosedPin() const
{
	return FindPin(StartClosedPinName, EGPD_Input);
}

#undef LOCTEXT_NAMESPACE