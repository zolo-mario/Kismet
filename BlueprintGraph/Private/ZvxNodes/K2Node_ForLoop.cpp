#include "K2Node_ForLoop.h"
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

#define LOCTEXT_NAMESPACE "K2Node_ForLoop"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_ForLoop

class FKCHandler_ForLoop : public FNodeHandlingFunctor
{
public:
	FKCHandler_ForLoop(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_ForLoop* ForLoopNode = CastChecked<UK2Node_ForLoop>(Node);

		// Register standard pins first
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		UEdGraphPin* IndexPin = ForLoopNode->GetIndexPin();
		FBPTerminal** IndexTermPtr = Context.NetMap.Find(IndexPin);
		if (!IndexTermPtr)
		{
			FBPTerminal* IndexTerm = Context.CreateLocalTerminal();
			IndexTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Int;
			IndexTerm->Type.PinSubCategory = NAME_None;
			IndexTerm->Type.PinSubCategoryObject = nullptr;
			IndexTerm->Source = ForLoopNode;
			IndexTerm->Name = Context.NetNameMap->MakeValidName(IndexPin);
			IndexTerm->bIsLiteral = false;
			Context.NetMap.Add(IndexPin, IndexTerm);
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_ForLoop* ForLoopNode = CastChecked<UK2Node_ForLoop>(Node);

		// Get the pins
		UEdGraphPin* FirstIndexPin = ForLoopNode->GetFirstIndexPin();
		UEdGraphPin* LastIndexPin = ForLoopNode->GetLastIndexPin();
		UEdGraphPin* LoopBodyPin = ForLoopNode->GetLoopBodyPin();
		UEdGraphPin* IndexPin = ForLoopNode->GetIndexPin();
		UEdGraphPin* CompletedPin = ForLoopNode->GetCompletedPin();

		// Validate all pins exist
		if (!FirstIndexPin || !LastIndexPin || !LoopBodyPin || !IndexPin || !CompletedPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForLoopMissingPins", "ForLoop node missing required pins").ToString(), ForLoopNode);
			return;
		}

		// Validate execution pin types
		FEdGraphPinType ExpectedExecPinType;
		ExpectedExecPinType.PinCategory = UEdGraphSchema_K2::PC_Exec;

		if (!Context.ValidatePinType(LoopBodyPin, ExpectedExecPinType))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForLoopInvalidLoopBodyPin", "ForLoop LoopBody pin must be an execution pin").ToString(), ForLoopNode);
			return;
		}

		if (!Context.ValidatePinType(CompletedPin, ExpectedExecPinType))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForLoopInvalidCompletedPin", "ForLoop Completed pin must be an execution pin").ToString(), ForLoopNode);
			return;
		}

		// Get the terms for pins - use GetNetFromPin for connected pins
		UEdGraphPin* FirstIndexNet = FEdGraphUtilities::GetNetFromPin(FirstIndexPin);
		UEdGraphPin* LastIndexNet = FEdGraphUtilities::GetNetFromPin(LastIndexPin);
		UEdGraphPin* IndexNet = FEdGraphUtilities::GetNetFromPin(IndexPin);

		FBPTerminal* FirstIndexTerm = Context.NetMap.FindRef(FirstIndexNet);
		FBPTerminal* LastIndexTerm = Context.NetMap.FindRef(LastIndexNet);
		FBPTerminal* IndexTerm =Context.NetMap.FindRef(IndexNet);

		if (!FirstIndexTerm || !LastIndexTerm || !IndexTerm)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForLoopMissingTerms", "ForLoop node missing required terms").ToString(), ForLoopNode);
			return;
		}

		// Create the custom ZVX statement for ForLoop
		FBlueprintCompiledStatement& ForLoopStatement = Context.AppendZvxStatementForNode(Node);
		if (LoopBodyPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& LoopBodyStatement = GenerateSimpleThenGoto(Context, *ForLoopNode, LoopBodyPin);
			ForLoopStatement.ZvxStatementMap.Add(TEXT("LoopBody"), &LoopBodyStatement);
		}
		if (CompletedPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& CompletedStatement = GenerateSimpleThenGoto(Context, *ForLoopNode, CompletedPin);
			ForLoopStatement.ZvxStatementMap.Add(TEXT("Completed"), &CompletedStatement);
		}
		
		ForLoopStatement.ZvxTerminalMap.Add(TEXT("FirstIndex"), FirstIndexTerm);
		ForLoopStatement.ZvxTerminalMap.Add(TEXT("LastIndex"), LastIndexTerm);
		ForLoopStatement.ZvxTerminalMap.Add(TEXT("Index"), IndexTerm);
	}
};

// Pin names
const FName UK2Node_ForLoop::ExecPinName(TEXT("exec"));
const FName UK2Node_ForLoop::FirstIndexPinName(TEXT("FirstIndex"));
const FName UK2Node_ForLoop::LastIndexPinName(TEXT("LastIndex"));
const FName UK2Node_ForLoop::LoopBodyPinName(TEXT("LoopBody"));
const FName UK2Node_ForLoop::IndexPinName(TEXT("Index"));
const FName UK2Node_ForLoop::CompletedPinName(TEXT("Completed"));

UK2Node_ForLoop::UK2Node_ForLoop(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_ForLoop::AllocateDefaultPins()
{
	// Execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ExecPinName);
	
	// First index input (integer) with default value
	UEdGraphPin* FirstIndexPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FirstIndexPinName);
	FirstIndexPin->DefaultValue = TEXT("0");
	
	// Last index input (integer) with default value
	UEdGraphPin* LastIndexPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, LastIndexPinName);
	LastIndexPin->DefaultValue = TEXT("10");
	
	// Loop body execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
	
	// Current index output (integer)
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, IndexPinName);
	
	// Completed execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, CompletedPinName);

	Super::AllocateDefaultPins();
}

FText UK2Node_ForLoop::GetTooltipText() const
{
	return LOCTEXT("ForLoopTooltip", "Executes a loop from FirstIndex to LastIndex");
}

FText UK2Node_ForLoop::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("ForLoop", "For Loop");
}

FText UK2Node_ForLoop::GetMenuCategory() const
{
	return LOCTEXT("ForLoopCategory", "Zvx");
}

FSlateIcon UK2Node_ForLoop::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Macro.Loop_16x");
	return Icon;
}

void UK2Node_ForLoop::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FNodeHandlingFunctor* UK2Node_ForLoop::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_ForLoop(CompilerContext);
}

void UK2Node_ForLoop::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

// Pin getter functions
UEdGraphPin* UK2Node_ForLoop::GetExecPin() const
{
	return FindPin(ExecPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoop::GetFirstIndexPin() const
{
	return FindPin(FirstIndexPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoop::GetLastIndexPin() const
{
	return FindPin(LastIndexPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoop::GetLoopBodyPin() const
{
	return FindPin(LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForLoop::GetIndexPin() const
{
	return FindPin(IndexPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForLoop::GetCompletedPin() const
{
	return FindPin(CompletedPinName, EGPD_Output);
}

#undef LOCTEXT_NAMESPACE
