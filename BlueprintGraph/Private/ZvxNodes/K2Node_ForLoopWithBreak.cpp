#include "K2Node_ForLoopWithBreak.h"
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

#define LOCTEXT_NAMESPACE "K2Node_ForLoopWithBreak"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_ForLoopWithBreak

class FKCHandler_ForLoopWithBreak : public FNodeHandlingFunctor
{
public:
	FKCHandler_ForLoopWithBreak(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_ForLoopWithBreak* ForLoopNode = CastChecked<UK2Node_ForLoopWithBreak>(Node);

		// Register standard pins first
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Create a local variable for the current index
		UEdGraphPin* IndexPin = ForLoopNode->GetIndexPin();
		if (IndexPin)
		{
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

		// Handle FirstIndex pin
		UEdGraphPin* FirstIndexPin = ForLoopNode->GetFirstIndexPin();
		if (FirstIndexPin && FirstIndexPin->LinkedTo.Num() == 0)
		{
			// Default to 0 if not connected
			FirstIndexPin->DefaultValue = TEXT("0");
		}

		// Handle LastIndex pin  
		UEdGraphPin* LastIndexPin = ForLoopNode->GetLastIndexPin();
		if (LastIndexPin && LastIndexPin->LinkedTo.Num() == 0)
		{
			// Default to 10 if not connected
			LastIndexPin->DefaultValue = TEXT("10");
		}

		// Validate required connections
		UEdGraphPin* LoopBodyPin = ForLoopNode->GetLoopBodyPin();
		UEdGraphPin* CompletedPin = ForLoopNode->GetCompletedPin();
		UEdGraphPin* BreakPin = ForLoopNode->GetBreakPin();

		if (!LoopBodyPin || !CompletedPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForLoopWithBreakMissingPins", "ForLoopWithBreak node missing required output pins").ToString(), ForLoopNode);
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_ForLoopWithBreak* ForLoopNode = CastChecked<UK2Node_ForLoopWithBreak>(Node);

		// Get all the pins
		UEdGraphPin* ExecPin = ForLoopNode->GetExecPin();
		UEdGraphPin* FirstIndexPin = ForLoopNode->GetFirstIndexPin();
		UEdGraphPin* LastIndexPin = ForLoopNode->GetLastIndexPin();
		UEdGraphPin* BreakPin = ForLoopNode->GetBreakPin();
		UEdGraphPin* LoopBodyPin = ForLoopNode->GetLoopBodyPin();
		UEdGraphPin* IndexPin = ForLoopNode->GetIndexPin();
		UEdGraphPin* CompletedPin = ForLoopNode->GetCompletedPin();

		// Validate all required pins exist
		if (!ExecPin || !FirstIndexPin || !LastIndexPin || !LoopBodyPin || !IndexPin || !CompletedPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForLoopWithBreakMissingPins", "ForLoopWithBreak node missing required pins").ToString(), ForLoopNode);
			return;
		}

		// Get the terms for pins
		UEdGraphPin* FirstIndexNet = FEdGraphUtilities::GetNetFromPin(FirstIndexPin);
		UEdGraphPin* LastIndexNet = FEdGraphUtilities::GetNetFromPin(LastIndexPin);
		UEdGraphPin* IndexNet = FEdGraphUtilities::GetNetFromPin(IndexPin);

		FBPTerminal* FirstIndexTerm = Context.NetMap.FindRef(FirstIndexNet);
		FBPTerminal* LastIndexTerm = Context.NetMap.FindRef(LastIndexNet);
		FBPTerminal* IndexTerm = Context.NetMap.FindRef(IndexNet);

		if (!FirstIndexTerm || !LastIndexTerm || !IndexTerm)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForLoopWithBreakMissingTerms", "ForLoopWithBreak node missing required terms").ToString(), ForLoopNode);
			return;
		}

		// Create the custom ZVX statement for ForLoopWithBreak
		FBlueprintCompiledStatement& ForLoopStatement = Context.AppendZvxStatementForNode(Node);
		
		// Handle execution pins
		if (ExecPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& ExecStatement = GenerateSimpleThenGoto(Context, *ForLoopNode, ExecPin);
			ForLoopStatement.ZvxStatementMap.Add(TEXT("Exec"), &ExecStatement);
		}

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

		// Handle Break pin (optional)
		if (BreakPin && BreakPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& BreakStatement = GenerateSimpleThenGoto(Context, *ForLoopNode, BreakPin);
			ForLoopStatement.ZvxStatementMap.Add(TEXT("Break"), &BreakStatement);
		}

		// Add terminals to ZVX statement
		ForLoopStatement.ZvxTerminalMap.Add(TEXT("FirstIndex"), FirstIndexTerm);
		ForLoopStatement.ZvxTerminalMap.Add(TEXT("LastIndex"), LastIndexTerm);
		ForLoopStatement.ZvxTerminalMap.Add(TEXT("Index"), IndexTerm);
	}
};

// Pin names
const FName UK2Node_ForLoopWithBreak::ExecPinName(TEXT(""));
const FName UK2Node_ForLoopWithBreak::FirstIndexPinName(TEXT("First Index"));
const FName UK2Node_ForLoopWithBreak::LastIndexPinName(TEXT("Last Index"));
const FName UK2Node_ForLoopWithBreak::BreakPinName(TEXT("Break"));
const FName UK2Node_ForLoopWithBreak::LoopBodyPinName(TEXT("Loop Body"));
const FName UK2Node_ForLoopWithBreak::IndexPinName(TEXT("Index"));
const FName UK2Node_ForLoopWithBreak::CompletedPinName(TEXT("Completed"));

UK2Node_ForLoopWithBreak::UK2Node_ForLoopWithBreak(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_ForLoopWithBreak::AllocateDefaultPins()
{
	// Execution input (unlabeled)
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ExecPinName);
	
	// First Index input
	UEdGraphPin* FirstIndexPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FirstIndexPinName);
	FirstIndexPin->DefaultValue = TEXT("0");
	
	// Last Index input
	UEdGraphPin* LastIndexPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, LastIndexPinName);
	LastIndexPin->DefaultValue = TEXT("10");
	
	// Break execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, BreakPinName);
	
	// Loop Body execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
	
	// Index output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, IndexPinName);
	
	// Completed execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, CompletedPinName);

	Super::AllocateDefaultPins();
}

FText UK2Node_ForLoopWithBreak::GetTooltipText() const
{
	return LOCTEXT("ForLoopWithBreakTooltip", "Outputs an execution pulse for each index between First Index and Last Index. Can be interrupted with a break signal.");
}

FText UK2Node_ForLoopWithBreak::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("ForLoopWithBreak", "For Loop with Break");
}

FText UK2Node_ForLoopWithBreak::GetMenuCategory() const
{
	return LOCTEXT("ForLoopWithBreakCategory", "Zvx");
}

FSlateIcon UK2Node_ForLoopWithBreak::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Macro.Loop_16x");
	return Icon;
}

void UK2Node_ForLoopWithBreak::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FNodeHandlingFunctor* UK2Node_ForLoopWithBreak::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_ForLoopWithBreak(CompilerContext);
}

void UK2Node_ForLoopWithBreak::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

// Pin getter functions
UEdGraphPin* UK2Node_ForLoopWithBreak::GetExecPin() const
{
	return FindPin(ExecPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoopWithBreak::GetFirstIndexPin() const
{
	return FindPin(FirstIndexPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoopWithBreak::GetLastIndexPin() const
{
	return FindPin(LastIndexPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoopWithBreak::GetBreakPin() const
{
	return FindPin(BreakPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoopWithBreak::GetLoopBodyPin() const
{
	return FindPin(LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForLoopWithBreak::GetIndexPin() const
{
	return FindPin(IndexPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForLoopWithBreak::GetCompletedPin() const
{
	return FindPin(CompletedPinName, EGPD_Output);
}

#undef LOCTEXT_NAMESPACE