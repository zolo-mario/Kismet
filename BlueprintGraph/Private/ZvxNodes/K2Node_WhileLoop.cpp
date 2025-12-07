#include "K2Node_WhileLoop.h"
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

#define LOCTEXT_NAMESPACE "K2Node_WhileLoop"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_WhileLoop

class FKCHandler_WhileLoop : public FNodeHandlingFunctor
{
public:
	FKCHandler_WhileLoop(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_WhileLoop* WhileLoopNode = CastChecked<UK2Node_WhileLoop>(Node);

		// Register standard pins first
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Handle Condition pin default value
		UEdGraphPin* ConditionPin = WhileLoopNode->GetConditionPin();
		if (ConditionPin && ConditionPin->LinkedTo.Num() == 0)
		{
			// Default to true if not connected
			ConditionPin->DefaultValue = TEXT("true");
		}

		// Validate required connections
		UEdGraphPin* LoopBodyPin = WhileLoopNode->GetLoopBodyPin();
		UEdGraphPin* CompletedPin = WhileLoopNode->GetCompletedPin();

		if (!LoopBodyPin || !CompletedPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("WhileLoopMissingPins", "WhileLoop node missing required output pins").ToString(), WhileLoopNode);
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_WhileLoop* WhileLoopNode = CastChecked<UK2Node_WhileLoop>(Node);

		// Get all the pins
		UEdGraphPin* ExecPin = WhileLoopNode->GetExecPin();
		UEdGraphPin* ConditionPin = WhileLoopNode->GetConditionPin();
		UEdGraphPin* LoopBodyPin = WhileLoopNode->GetLoopBodyPin();
		UEdGraphPin* CompletedPin = WhileLoopNode->GetCompletedPin();

		// Validate all required pins exist
		if (!ExecPin || !ConditionPin || !LoopBodyPin || !CompletedPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("WhileLoopMissingPins", "WhileLoop node missing required pins").ToString(), WhileLoopNode);
			return;
		}

		// Get the terms for pins
		UEdGraphPin* ConditionNet = FEdGraphUtilities::GetNetFromPin(ConditionPin);
		FBPTerminal* ConditionTerm = Context.NetMap.FindRef(ConditionNet);

		if (!ConditionTerm)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("WhileLoopMissingTerms", "WhileLoop node missing required terms").ToString(), WhileLoopNode);
			return;
		}

		// Create the custom ZVX statement for WhileLoop
		FBlueprintCompiledStatement& WhileLoopStatement = Context.AppendZvxStatementForNode(Node);
		
		// Handle execution pins
		if (ExecPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& ExecStatement = GenerateSimpleThenGoto(Context, *WhileLoopNode, ExecPin);
			WhileLoopStatement.ZvxStatementMap.Add(TEXT("Exec"), &ExecStatement);
		}

		if (LoopBodyPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& LoopBodyStatement = GenerateSimpleThenGoto(Context, *WhileLoopNode, LoopBodyPin);
			WhileLoopStatement.ZvxStatementMap.Add(TEXT("LoopBody"), &LoopBodyStatement);
		}

		if (CompletedPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& CompletedStatement = GenerateSimpleThenGoto(Context, *WhileLoopNode, CompletedPin);
			WhileLoopStatement.ZvxStatementMap.Add(TEXT("Completed"), &CompletedStatement);
		}

		// Add condition terminal to ZVX statement
		WhileLoopStatement.ZvxTerminalMap.Add(TEXT("Condition"), ConditionTerm);
	}
};

// Pin names
const FName UK2Node_WhileLoop::ExecPinName(TEXT(""));
const FName UK2Node_WhileLoop::ConditionPinName(TEXT("Condition"));
const FName UK2Node_WhileLoop::LoopBodyPinName(TEXT("Loop Body"));
const FName UK2Node_WhileLoop::CompletedPinName(TEXT("Completed"));

UK2Node_WhileLoop::UK2Node_WhileLoop(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_WhileLoop::AllocateDefaultPins()
{
	// Execution input (unlabeled)
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ExecPinName);
	
	// Condition boolean input
	UEdGraphPin* ConditionPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, ConditionPinName);
	ConditionPin->DefaultValue = TEXT("true");
	
	// Loop Body execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
	
	// Completed execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, CompletedPinName);

	Super::AllocateDefaultPins();
}

FText UK2Node_WhileLoop::GetTooltipText() const
{
	return LOCTEXT("WhileLoopTooltip", "Loop while the condition is true. Each iteration will fire the Loop Body. When the condition becomes false, fire the Completed output.");
}

FText UK2Node_WhileLoop::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("WhileLoop", "While Loop");
}

FText UK2Node_WhileLoop::GetMenuCategory() const
{
	return LOCTEXT("WhileLoopCategory", "Zvx");
}

FSlateIcon UK2Node_WhileLoop::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Macro.Loop_16x");
	return Icon;
}

void UK2Node_WhileLoop::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FNodeHandlingFunctor* UK2Node_WhileLoop::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_WhileLoop(CompilerContext);
}

void UK2Node_WhileLoop::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

// Pin getter functions
UEdGraphPin* UK2Node_WhileLoop::GetExecPin() const
{
	return FindPin(ExecPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_WhileLoop::GetConditionPin() const
{
	return FindPin(ConditionPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_WhileLoop::GetLoopBodyPin() const
{
	return FindPin(LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_WhileLoop::GetCompletedPin() const
{
	return FindPin(CompletedPinName, EGPD_Output);
}

#undef LOCTEXT_NAMESPACE