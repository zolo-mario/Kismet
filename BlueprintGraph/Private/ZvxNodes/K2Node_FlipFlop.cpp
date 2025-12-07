#include "K2Node_FlipFlop.h"
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

#define LOCTEXT_NAMESPACE "K2Node_FlipFlop"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_FlipFlop

class FKCHandler_FlipFlop : public FNodeHandlingFunctor
{
public:
	FKCHandler_FlipFlop(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_FlipFlop* FlipFlopNode = CastChecked<UK2Node_FlipFlop>(Node);

		// Register standard pins first
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Create a boolean variable to track current state (true = A, false = B)
		FBPTerminal* StateTerm = Context.CreateLocalTerminal();
		StateTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		StateTerm->Type.PinSubCategory = NAME_None;
		StateTerm->Type.PinSubCategoryObject = nullptr;
		StateTerm->Source = FlipFlopNode;
		StateTerm->Name = Context.NetNameMap->MakeValidName(FlipFlopNode, TEXT("FlipFlopState"));
		StateTerm->bIsLiteral = false;
		Context.Literals.Add(StateTerm);

		// Register IsA output pin
		UEdGraphPin* IsAPin = FlipFlopNode->GetIsAPin();
		if (IsAPin)
		{
			FBPTerminal** IsATermPtr = Context.NetMap.Find(IsAPin);
			if (!IsATermPtr)
			{
				FBPTerminal* IsATerm = Context.CreateLocalTerminal();
				IsATerm->Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
				IsATerm->Type.PinSubCategory = NAME_None;
				IsATerm->Type.PinSubCategoryObject = nullptr;
				IsATerm->Source = FlipFlopNode;
				IsATerm->Name = Context.NetNameMap->MakeValidName(IsAPin);
				IsATerm->bIsLiteral = false;
				Context.NetMap.Add(IsAPin, IsATerm);
			}
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_FlipFlop* FlipFlopNode = CastChecked<UK2Node_FlipFlop>(Node);

		// Get the pins
		UEdGraphPin* EnterPin = FlipFlopNode->GetEnterPin();
		UEdGraphPin* APin = FlipFlopNode->GetAPin();
		UEdGraphPin* BPin = FlipFlopNode->GetBPin();
		UEdGraphPin* IsAPin = FlipFlopNode->GetIsAPin();

		// Validate all pins exist
		if (!EnterPin || !APin || !BPin || !IsAPin)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("FlipFlopMissingPins", "FlipFlop node missing required pins").ToString(), FlipFlopNode);
			return;
		}

		// Get the terms for pins
		UEdGraphPin* IsANet = FEdGraphUtilities::GetNetFromPin(IsAPin);
		FBPTerminal* IsATerm = Context.NetMap.FindRef(IsANet);

		if (!IsATerm)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("FlipFlopMissingTerms", "FlipFlop node missing required terms").ToString(), FlipFlopNode);
			return;
		}

		// Create the custom ZVX statement for FlipFlop
		FBlueprintCompiledStatement& FlipFlopStatement = Context.AppendZvxStatementForNode(Node);
		
		// Handle Enter pin execution
		if (EnterPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& EnterStatement = GenerateSimpleThenGoto(Context, *FlipFlopNode, EnterPin);
			FlipFlopStatement.ZvxStatementMap.Add(TEXT("Enter"), &EnterStatement);
		}

		// Handle A pin execution
		if (APin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& AStatement = GenerateSimpleThenGoto(Context, *FlipFlopNode, APin);
			FlipFlopStatement.ZvxStatementMap.Add(TEXT("A"), &AStatement);
		}

		// Handle B pin execution
		if (BPin->LinkedTo.Num() > 0)
		{
			FBlueprintCompiledStatement& BStatement = GenerateSimpleThenGoto(Context, *FlipFlopNode, BPin);
			FlipFlopStatement.ZvxStatementMap.Add(TEXT("B"), &BStatement);
		}
		
		FlipFlopStatement.ZvxTerminalMap.Add(TEXT("IsA"), IsATerm);
	}
};

// Pin names
const FName UK2Node_FlipFlop::EnterPinName(TEXT("Enter"));
const FName UK2Node_FlipFlop::APinName(TEXT("A"));
const FName UK2Node_FlipFlop::BPinName(TEXT("B"));
const FName UK2Node_FlipFlop::IsAPinName(TEXT("IsA"));

UK2Node_FlipFlop::UK2Node_FlipFlop(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_FlipFlop::AllocateDefaultPins()
{
	// Enter execution input (unlabeled in documentation)
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, EnterPinName);
	
	// A execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, APinName);
	
	// B execution output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, BPinName);
	
	// IsA boolean output
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, IsAPinName);

	Super::AllocateDefaultPins();
}

FText UK2Node_FlipFlop::GetTooltipText() const
{
	return LOCTEXT("FlipFlopTooltip", "Takes in an execution output and toggles between two execution outputs. The first time it is called, output A executes. The second time, B. Then A, then B, and so on.");
}

FText UK2Node_FlipFlop::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("FlipFlop", "Flip Flop");
}

FText UK2Node_FlipFlop::GetMenuCategory() const
{
	return LOCTEXT("FlipFlopCategory", "Zvx");
}

FSlateIcon UK2Node_FlipFlop::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Branch_16x");
	return Icon;
}

void UK2Node_FlipFlop::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FNodeHandlingFunctor* UK2Node_FlipFlop::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_FlipFlop(CompilerContext);
}

void UK2Node_FlipFlop::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

// Pin getter functions
UEdGraphPin* UK2Node_FlipFlop::GetEnterPin() const
{
	return FindPin(EnterPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_FlipFlop::GetAPin() const
{
	return FindPin(APinName, EGPD_Output);
}

UEdGraphPin* UK2Node_FlipFlop::GetBPin() const
{
	return FindPin(BPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_FlipFlop::GetIsAPin() const
{
	return FindPin(IsAPinName, EGPD_Output);
}

#undef LOCTEXT_NAMESPACE