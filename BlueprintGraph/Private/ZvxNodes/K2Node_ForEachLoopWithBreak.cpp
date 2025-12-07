#include "K2Node_ForEachLoopWithBreak.h"
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

#define LOCTEXT_NAMESPACE "K2Node_ForEachLoopWithBreak"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_ForEachLoopWithBreak

class FKCHandler_ForEachLoopWithBreak : public FNodeHandlingFunctor
{
public:
	FKCHandler_ForEachLoopWithBreak(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_ForEachLoopWithBreak* ForEachLoopNode = CastChecked<UK2Node_ForEachLoopWithBreak>(Node);

		// Register standard pins first
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Create a local variable for the current index
		UEdGraphPin* ArrayIndexPin = ForEachLoopNode->GetArrayIndexPin();
		if (ArrayIndexPin)
		{
			FBPTerminal** IndexTermPtr = Context.NetMap.Find(ArrayIndexPin);
			if (!IndexTermPtr)
			{
				FBPTerminal* IndexTerm = Context.CreateLocalTerminal();
				IndexTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Int;
				IndexTerm->Type.PinSubCategory = NAME_None;
				IndexTerm->Type.PinSubCategoryObject = nullptr;
				IndexTerm->Source = ForEachLoopNode;
				IndexTerm->Name = Context.NetNameMap->MakeValidName(ArrayIndexPin);
				IndexTerm->bIsLiteral = false;
				Context.NetMap.Add(ArrayIndexPin, IndexTerm);
			}
		}

		// Create a local variable for the current array element
		UEdGraphPin* ArrayElementPin = ForEachLoopNode->GetArrayElementPin();
		if (ArrayElementPin)
		{
			FBPTerminal** ElementTermPtr = Context.NetMap.Find(ArrayElementPin);
			if (!ElementTermPtr)
			{
				FBPTerminal* ElementTerm = Context.CreateLocalTerminal();
				ElementTerm->Type = ArrayElementPin->PinType;
				ElementTerm->Source = ForEachLoopNode;
				ElementTerm->Name = Context.NetNameMap->MakeValidName(ArrayElementPin);
				ElementTerm->bIsLiteral = false;
				Context.NetMap.Add(ArrayElementPin, ElementTerm);
			}
		}

		// Validate required connections
		UEdGraphPin* ArrayPin = ForEachLoopNode->GetArrayPin();
		UEdGraphPin* LoopBodyPin = ForEachLoopNode->GetLoopBodyPin();
		UEdGraphPin* CompletedPin = ForEachLoopNode->GetCompletedPin();
		UEdGraphPin* BreakPin = ForEachLoopNode->GetBreakPin();

		if (!ArrayPin || !LoopBodyPin || !CompletedPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForEachLoopWithBreakMissingPins", "ForEachLoopWithBreak node missing required pins").ToString(), ForEachLoopNode);
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_ForEachLoopWithBreak* ForEachLoopNode = CastChecked<UK2Node_ForEachLoopWithBreak>(Node);

		// Get all the pins
		UEdGraphPin* ExecPin = ForEachLoopNode->GetExecPin();
		UEdGraphPin* ArrayPin = ForEachLoopNode->GetArrayPin();
		UEdGraphPin* BreakPin = ForEachLoopNode->GetBreakPin();
		UEdGraphPin* LoopBodyPin = ForEachLoopNode->GetLoopBodyPin();
		UEdGraphPin* ArrayElementPin = ForEachLoopNode->GetArrayElementPin();
		UEdGraphPin* ArrayIndexPin = ForEachLoopNode->GetArrayIndexPin();
		UEdGraphPin* CompletedPin = ForEachLoopNode->GetCompletedPin();

		// Validate all required pins exist
		if (!ExecPin || !ArrayPin || !LoopBodyPin || !ArrayElementPin || !ArrayIndexPin || !CompletedPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForEachLoopWithBreakMissingPins", "ForEachLoopWithBreak node missing required pins").ToString(), ForEachLoopNode);
			return;
		}

		// Get the terms for pins
		UEdGraphPin* ArrayNet = FEdGraphUtilities::GetNetFromPin(ArrayPin);
		UEdGraphPin* ArrayElementNet = FEdGraphUtilities::GetNetFromPin(ArrayElementPin);
		UEdGraphPin* ArrayIndexNet = FEdGraphUtilities::GetNetFromPin(ArrayIndexPin);

		FBPTerminal* ArrayTerm = Context.NetMap.FindRef(ArrayNet);
		FBPTerminal* ArrayElementTerm = Context.NetMap.FindRef(ArrayElementNet);
		FBPTerminal* ArrayIndexTerm = Context.NetMap.FindRef(ArrayIndexNet);

		if (!ArrayTerm || !ArrayElementTerm || !ArrayIndexTerm)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForEachLoopWithBreakMissingTerms", "ForEachLoopWithBreak node missing required terms").ToString(), ForEachLoopNode);
			return;
		}

		// Create the custom ZVX statement for ForEachLoopWithBreak
		FBlueprintCompiledStatement& ForEachLoopStatement = Context.AppendZvxStatementForNode(Node);
		
		// Handle execution pins
		if (ExecPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& ExecStatement = GenerateSimpleThenGoto(Context, *ForEachLoopNode, ExecPin);
			ForEachLoopStatement.ZvxStatementMap.Add(TEXT("Exec"), &ExecStatement);
		}

		if (LoopBodyPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& LoopBodyStatement = GenerateSimpleThenGoto(Context, *ForEachLoopNode, LoopBodyPin);
			ForEachLoopStatement.ZvxStatementMap.Add(TEXT("LoopBody"), &LoopBodyStatement);
		}

		if (CompletedPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& CompletedStatement = GenerateSimpleThenGoto(Context, *ForEachLoopNode, CompletedPin);
			ForEachLoopStatement.ZvxStatementMap.Add(TEXT("Completed"), &CompletedStatement);
		}

		// Handle Break pin (optional)
		if (BreakPin && BreakPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& BreakStatement = GenerateSimpleThenGoto(Context, *ForEachLoopNode, BreakPin);
			ForEachLoopStatement.ZvxStatementMap.Add(TEXT("Break"), &BreakStatement);
		}

		// Add terminals to ZVX statement
		ForEachLoopStatement.ZvxTerminalMap.Add(TEXT("Array"), ArrayTerm);
		ForEachLoopStatement.ZvxTerminalMap.Add(TEXT("ArrayElement"), ArrayElementTerm);
		ForEachLoopStatement.ZvxTerminalMap.Add(TEXT("ArrayIndex"), ArrayIndexTerm);
	}
};

// Pin names
const FName UK2Node_ForEachLoopWithBreak::ExecPinName(TEXT(""));
const FName UK2Node_ForEachLoopWithBreak::ArrayPinName(TEXT("Array"));
const FName UK2Node_ForEachLoopWithBreak::BreakPinName(TEXT("Break"));
const FName UK2Node_ForEachLoopWithBreak::LoopBodyPinName(TEXT("Loop Body"));
const FName UK2Node_ForEachLoopWithBreak::ArrayElementPinName(TEXT("Array Element"));
const FName UK2Node_ForEachLoopWithBreak::ArrayIndexPinName(TEXT("Array Index"));
const FName UK2Node_ForEachLoopWithBreak::CompletedPinName(TEXT("Completed"));

UK2Node_ForEachLoopWithBreak::UK2Node_ForEachLoopWithBreak(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_ForEachLoopWithBreak::AllocateDefaultPins()
{
	// Execution input (unlabeled)
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ExecPinName);
	
	// Array input - wildcard that will be constrained by connections
	UEdGraphPin* ArrayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, ArrayPinName);
	ArrayPin->PinType.ContainerType = EPinContainerType::Array;
	
	// Break execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, BreakPinName);
	
	// Loop Body execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
	
	// Array Element output - wildcard that will match array element type
	UEdGraphPin* ArrayElementPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, ArrayElementPinName);
	ArrayElementPin->PinType.ContainerType = EPinContainerType::None;
	
	// Array Index output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, ArrayIndexPinName);
	
	// Completed execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, CompletedPinName);

	Super::AllocateDefaultPins();
}

FText UK2Node_ForEachLoopWithBreak::GetTooltipText() const
{
	return LOCTEXT("ForEachLoopWithBreakTooltip", "Outputs an execution pulse for each item in the array. Can be interrupted with a break signal. The item is accessible via the Array Element output, and the current index via Array Index.");
}

FText UK2Node_ForEachLoopWithBreak::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("ForEachLoopWithBreak", "For Each Loop with Break");
}

FText UK2Node_ForEachLoopWithBreak::GetMenuCategory() const
{
	return LOCTEXT("ForEachLoopWithBreakCategory", "Zvx");
}

FSlateIcon UK2Node_ForEachLoopWithBreak::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Macro.ForEach_16x");
	return Icon;
}

void UK2Node_ForEachLoopWithBreak::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UK2Node_ForEachLoopWithBreak::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	// If the array pin connection changed, update the element type
	if (Pin == GetArrayPin())
	{
		RefreshPinTypes();
	}
}

void UK2Node_ForEachLoopWithBreak::PostReconstructNode()
{
	Super::PostReconstructNode();
	RefreshPinTypes();
}

FNodeHandlingFunctor* UK2Node_ForEachLoopWithBreak::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_ForEachLoopWithBreak(CompilerContext);
}

void UK2Node_ForEachLoopWithBreak::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

// Pin getter functions
UEdGraphPin* UK2Node_ForEachLoopWithBreak::GetExecPin() const
{
	return FindPin(ExecPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachLoopWithBreak::GetArrayPin() const
{
	return FindPin(ArrayPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachLoopWithBreak::GetBreakPin() const
{
	return FindPin(BreakPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachLoopWithBreak::GetLoopBodyPin() const
{
	return FindPin(LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachLoopWithBreak::GetArrayElementPin() const
{
	return FindPin(ArrayElementPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachLoopWithBreak::GetArrayIndexPin() const
{
	return FindPin(ArrayIndexPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachLoopWithBreak::GetCompletedPin() const
{
	return FindPin(CompletedPinName, EGPD_Output);
}

void UK2Node_ForEachLoopWithBreak::RefreshPinTypes()
{
	UEdGraphPin* ArrayPin = GetArrayPin();
	UEdGraphPin* ArrayElementPin = GetArrayElementPin();
	
	if (!ArrayPin || !ArrayElementPin)
	{
		return;
	}

	// If array pin is connected, propagate the inner type to the element pin
	if (ArrayPin->LinkedTo.Num() > 0)
	{
		UEdGraphPin* LinkedPin = ArrayPin->LinkedTo[0];
		if (LinkedPin && LinkedPin->PinType.ContainerType == EPinContainerType::Array)
		{
			// Copy the array's inner type to the element pin
			ArrayElementPin->PinType = LinkedPin->PinType;
			ArrayElementPin->PinType.ContainerType = EPinContainerType::None;
		}
	}
	else
	{
		// Reset to wildcard if no connection
		ArrayElementPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		ArrayElementPin->PinType.PinSubCategory = NAME_None;
		ArrayElementPin->PinType.PinSubCategoryObject = nullptr;
		ArrayElementPin->PinType.ContainerType = EPinContainerType::None;
	}
}

#undef LOCTEXT_NAMESPACE