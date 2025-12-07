#include "K2Node_ForEachLoop.h"
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
#include "K2Node_GetArrayItem.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "KismetCompilerMisc.h"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "K2Node_ForEachLoop"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_ForEachLoop

class FKCHandler_ForEachLoop : public FNodeHandlingFunctor
{
public:
	FKCHandler_ForEachLoop(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_ForEachLoop* ForEachLoopNode = CastChecked<UK2Node_ForEachLoop>(Node);

		// Register standard pins first
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Register array element output pin
		UEdGraphPin* ArrayElementPin = ForEachLoopNode->GetArrayElementPin();
		FBPTerminal** ArrayElementTermPtr = Context.NetMap.Find(ArrayElementPin);
		if (!ArrayElementTermPtr)
		{
			FBPTerminal* ArrayElementTerm = Context.CreateLocalTerminal();
			ArrayElementTerm->Type = ArrayElementPin->PinType;
			ArrayElementTerm->Source = ForEachLoopNode;
			ArrayElementTerm->Name = Context.NetNameMap->MakeValidName(ArrayElementPin);
			ArrayElementTerm->bIsLiteral = false;
			Context.NetMap.Add(ArrayElementPin, ArrayElementTerm);
		}

		// Register array index output pin
		UEdGraphPin* ArrayIndexPin = ForEachLoopNode->GetArrayIndexPin();
		if (!ArrayIndexPin)
		{
			// Debug: Try to find the pin manually
			for (UEdGraphPin* Pin : ForEachLoopNode->Pins)
			{
				if (Pin && Pin->PinName == UK2Node_ForEachLoop::ArrayIndexPinName)
				{
					ArrayIndexPin = Pin;
					break;
				}
			}
		}
		
		if (ArrayIndexPin)
		{
			FBPTerminal** ArrayIndexTermPtr = Context.NetMap.Find(ArrayIndexPin);
			if (!ArrayIndexTermPtr)
			{
				FBPTerminal* ArrayIndexTerm = Context.CreateLocalTerminal();
				ArrayIndexTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Int;
				ArrayIndexTerm->Type.PinSubCategory = NAME_None;
				ArrayIndexTerm->Type.PinSubCategoryObject = nullptr;
				ArrayIndexTerm->Source = ForEachLoopNode;
				ArrayIndexTerm->Name = Context.NetNameMap->MakeValidName(ArrayIndexPin);
				ArrayIndexTerm->bIsLiteral = false;
				Context.NetMap.Add(ArrayIndexPin, ArrayIndexTerm);
			}
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_ForEachLoop* ForEachLoopNode = CastChecked<UK2Node_ForEachLoop>(Node);

		// Get the pins
		UEdGraphPin* ArrayPin = ForEachLoopNode->GetArrayPin();
		UEdGraphPin* LoopBodyPin = ForEachLoopNode->GetLoopBodyPin();
		UEdGraphPin* ArrayElementPin = ForEachLoopNode->GetArrayElementPin();
		UEdGraphPin* ArrayIndexPin = ForEachLoopNode->GetArrayIndexPin();
		UEdGraphPin* CompletedPin = ForEachLoopNode->GetCompletedPin();

		// Validate all required pins exist (ArrayIndexPin is optional)
		if (!ArrayPin || !LoopBodyPin || !ArrayElementPin || !CompletedPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForEachLoopMissingPins", "ForEachLoop node missing required pins").ToString(), ForEachLoopNode);
			return;
		}

		// Get the terms for pins
		UEdGraphPin* ArrayNet = FEdGraphUtilities::GetNetFromPin(ArrayPin);
		UEdGraphPin* ArrayElementNet = FEdGraphUtilities::GetNetFromPin(ArrayElementPin);
		UEdGraphPin* ArrayIndexNet = ArrayIndexPin ? FEdGraphUtilities::GetNetFromPin(ArrayIndexPin) : nullptr;

		FBPTerminal* ArrayTerm = Context.NetMap.FindRef(ArrayNet);
		FBPTerminal* ArrayElementTerm = Context.NetMap.FindRef(ArrayElementNet);
		FBPTerminal* ArrayIndexTerm = ArrayIndexNet ? Context.NetMap.FindRef(ArrayIndexNet) : nullptr;

		if (!ArrayTerm || !ArrayElementTerm)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ForEachLoopMissingTerms", "ForEachLoop node missing required terms").ToString(), ForEachLoopNode);
			return;
		}

		// Create the custom ZVX statement for ForEachLoop
		FBlueprintCompiledStatement& ForEachLoopStatement = Context.AppendZvxStatementForNode(Node);
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
		
		ForEachLoopStatement.ZvxTerminalMap.Add(TEXT("Array"), ArrayTerm);
		ForEachLoopStatement.ZvxTerminalMap.Add(TEXT("ArrayElement"), ArrayElementTerm);
		if (ArrayIndexTerm)
		{
			ForEachLoopStatement.ZvxTerminalMap.Add(TEXT("ArrayIndex"), ArrayIndexTerm);
		}
	}
};

// Pin names
const FName UK2Node_ForEachLoop::ExecPinName(TEXT("exec"));
const FName UK2Node_ForEachLoop::ArrayPinName(TEXT("Array"));
const FName UK2Node_ForEachLoop::LoopBodyPinName(TEXT("LoopBody"));
const FName UK2Node_ForEachLoop::ArrayElementPinName(TEXT("ArrayElement"));
const FName UK2Node_ForEachLoop::ArrayIndexPinName(TEXT("ArrayIndex"));
const FName UK2Node_ForEachLoop::CompletedPinName(TEXT("Completed"));

UK2Node_ForEachLoop::UK2Node_ForEachLoop(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_ForEachLoop::AllocateDefaultPins()
{
	// Execution input
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ExecPinName);
	
	// Array input (wildcard array type)
	UEdGraphPin* ArrayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, ArrayPinName);
	ArrayPin->PinType.ContainerType = EPinContainerType::Array;
	ArrayPin->PinType.bIsReference = false;
	
	// Loop body execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
	
	// Array element output (wildcard type, will match array element type)
	UEdGraphPin* ArrayElementPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, ArrayElementPinName);
	ArrayElementPin->PinType.ContainerType = EPinContainerType::None;
	ArrayElementPin->PinType.bIsReference = false;
	
	// Array index output (integer)
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, ArrayIndexPinName);
	
	// Completed execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, CompletedPinName);

	Super::AllocateDefaultPins();
}

FText UK2Node_ForEachLoop::GetTooltipText() const
{
	return LOCTEXT("ForEachLoopTooltip", "Executes a loop for each element in an array");
}

FText UK2Node_ForEachLoop::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("ForEachLoop", "For Each Loop");
}

FText UK2Node_ForEachLoop::GetMenuCategory() const
{
	return LOCTEXT("ForEachLoopCategory", "Zvx");
}

FSlateIcon UK2Node_ForEachLoop::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Macro.Loop_16x");
	return Icon;
}

void UK2Node_ForEachLoop::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UK2Node_ForEachLoop::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);
	
	if (Pin == GetArrayPin())
	{
		RefreshPinTypes();
	}
}

void UK2Node_ForEachLoop::PostReconstructNode()
{
	Super::PostReconstructNode();
	RefreshPinTypes();
}

FNodeHandlingFunctor* UK2Node_ForEachLoop::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_ForEachLoop(CompilerContext);
}

void UK2Node_ForEachLoop::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

void UK2Node_ForEachLoop::RefreshPinTypes()
{
	UEdGraphPin* ArrayPin = GetArrayPin();
	UEdGraphPin* ArrayElementPin = GetArrayElementPin();
	
	if (!ArrayPin || !ArrayElementPin)
	{
		return;
	}

	// If array pin is connected, update element pin type to match array element type
	if (ArrayPin->LinkedTo.Num() > 0)
	{
		UEdGraphPin* ConnectedPin = ArrayPin->LinkedTo[0];
		if (ConnectedPin && ConnectedPin->PinType.ContainerType == EPinContainerType::Array)
		{
			// Copy the array's element type to the element output pin
			ArrayElementPin->PinType = ConnectedPin->PinType;
			ArrayElementPin->PinType.ContainerType = EPinContainerType::None; // Element is not an array
			ArrayElementPin->PinType.bIsReference = false;
			
			// Also update the array pin type to match the connected type
			ArrayPin->PinType = ConnectedPin->PinType;
		}
	}
	else
	{
		// No connection, revert to wildcard
		ArrayPin->PinType.ResetToDefaults();
		ArrayPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		ArrayPin->PinType.ContainerType = EPinContainerType::Array;
		
		ArrayElementPin->PinType.ResetToDefaults();
		ArrayElementPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		ArrayElementPin->PinType.ContainerType = EPinContainerType::None;
	}
}

// Pin getter functions
UEdGraphPin* UK2Node_ForEachLoop::GetExecPin() const
{
	return FindPin(ExecPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachLoop::GetArrayPin() const
{
	return FindPin(ArrayPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachLoop::GetLoopBodyPin() const
{
	return FindPin(LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachLoop::GetArrayElementPin() const
{
	return FindPin(ArrayElementPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachLoop::GetArrayIndexPin() const
{
	UEdGraphPin* Pin = FindPin(ArrayIndexPinName, EGPD_Output);
	if (!Pin)
	{
		// Fallback: search manually
		for (UEdGraphPin* TestPin : Pins)
		{
			if (TestPin && TestPin->PinName == ArrayIndexPinName && TestPin->Direction == EGPD_Output)
			{
				return TestPin;
			}
		}
	}
	return Pin;
}

UEdGraphPin* UK2Node_ForEachLoop::GetCompletedPin() const
{
	return FindPin(CompletedPinName, EGPD_Output);
}

#undef LOCTEXT_NAMESPACE