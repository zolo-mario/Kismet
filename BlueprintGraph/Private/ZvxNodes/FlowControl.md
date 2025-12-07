![Flow Control](https://dev.epicgames.com/community/api/documentation/image/4935de70-80bb-46d6-83f1-7d7a99a909b2?resizing_type=fill&width=1920&height=335)

## Standard Flow Control Nodes

These nodes provide a variety of means to control the flow of execution.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/8b39e139-d354-4676-b283-f1ca239f539b/flowcontrolexpanded.png)

### Branch

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/f71f4d3e-8707-4d18-b398-fca822b66bdd/branch_example.png)

The **Branch** node serves as a simple way to create decision-based flow from a single true/false condition. Once executed, the Branch node looks at the incoming value of the attached Boolean, and outputs an execution pulse down the appropriate output.

In this simple example, the branch is checking the current state of a boolean variable. If true, it sets the color of a light to be red. If false, it sets it to blue.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/002d732c-d5e5-4d28-a8ac-93ee264b1273/branch_network.png)

Click for full image.

| Item | Description |
| --- | --- |
| Input Pins |  |
| **(Unlabeled)** | This execution input triggers the branch check. |
| **Condition** | Takes in a boolean value used to indicate which output pin will be triggered. |
| Output Pins |  |
| **True** | This outputs an execution pulse if the incoming condition was `true`. |
| **False** | This outputs an execution pulse if the incoming condition was `false`. |

### DoN

![DoN Example](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/2a2feed1-3e05-4442-96a0-d2ce61fa65b9/do_n.png)

The **DoN** node will fire off an execution pulse **N** times. After the limit has been reached, it will cease all outgoing execution until a pulse is sent into its **Reset** input.

For example, you could have a vehicle start twenty times, and then fail to start until a refueling event tied to the **Reset** input was activated.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/03c9917a-0dfe-4236-9e35-833da99c5f06/refuel_key_do_n.png)

Click for full image.

| Item | Description |
| --- | --- |
| Input Pins |  |
| **Enter** | This execution input triggers the DoN check. |
| **n** | This input sets the number of times the DoN node will trigger. |
| **Reset** | This execution input will reset the DoN node so that it can be triggered again. |
| Output Pins |  |
| **Exit** | This execution pin is only triggered if the DoN has not yet been triggered more than N times, or if its Reset input has been called. |

### DoOnce

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/7047d5ce-c36a-4bbf-8323-caa2f3283ffd/doonce_example.png)

The **DoOnce** node - as the name suggests - will fire off an execution pulse just once. From that point forward, it will cease all outgoing execution until a pulse is sent into its *Reset* input. This node is equivalent to a DoN node where **N = 1**.

For example, you could have the network for an opening door run through a DoOnce, and that door would open only one time. However, you could tie a trigger event into the Reset, which will cause the door to be accessible once the trigger has been activated.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/ff517c5e-913b-40d0-8618-9cb8e61c9f44/doonce_network.png)

Click for full image.

| Item | Description |
| --- | --- |
| Input Pins |  |
| **(Unlabeled)** | This execution input triggers the DoOnce check. |
| **Reset** | This execution input will reset the DoOnce node so that it can be triggered again. |
| Output Pins |  |
| **Completed** | This execution pin is only triggered if the DoOnce has not yet been triggered, or if its Reset input has been called. |

### FlipFlop

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/28be1dab-7592-4213-bff6-fdd63f67b5c9/flipflop_example.png)

The **FlipFlop** node takes in an execution output and toggles between two execution outputs. The first time it is called, output A executes. The second time, B. Then A, then B, and so on. The node also has a boolean output allowing you to track when Output A has been called.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/acb20e5a-300c-4d0c-aece-c0cc035d7fd1/flipflop_network.png)

Click for full image.

| Item | Description |
| --- | --- |
| Input Pins |  |
| **(Unlabeled)** | This execution input triggers the FlipFlop. |
| Output Pins |  |
| **A** | This output pin is called the first and every odd-numbered time thereafter that the FlipFlop is triggered. |
| **B** | This output pin is called the second and every even-numbered time thereafter that the FlipFlop is triggered. |
| **Is A** | Outputs a boolean value indicating whether Output A is being triggered or not. This, in effect, will toggle between `true` and `false` each time the FlipFlop node is triggered. |

### ForLoop

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/9c23427b-7d18-4b55-a4f7-4345be5e41c4/forloop_example.png)

The **ForLoop** node works like a standard code loop, firing off an execution pulse for each index between a start and end.

In this simple example, the loop is triggered when the player touches a simple level trigger. The loop iterates 10 times, each time calling a Print String, which logs a prefix message along with the current iteration.

Loop iterations will take place between frames, so large loops may incur a performance hit.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/96bbf6cc-2e85-443a-8670-a6e7e24742a1/forloop_network.png)

Click for full image.

| Item | Description |
| --- | --- |
| Input Pins |  |
| **(Unlabeled)** | This execution input launches the loop. |
| **First Index** | Takes in an Int representing the first index of the loop. |
| **Last Index** | Takes in an Int representing the last index of the loop. |
| Output Pins |  |
| **Loop Body** | This outputs an execution pulse on each iteration of the loop as it moves between the indices. |
| **Index** | This outputs the current index of the loop. |
| **Completed** | This is a standard execution output pin that fires when the for loop has completed. |

### ForLoopWithBreak

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/7098aa10-983e-400f-b7a3-7002b8658993/forloopwithbreak_example.png)

The **ForLoopWithBreak** node works in a very similar manner to the ForLoop node, except that it includes an input pin that allows the loop to be broken.

In this simple example, the loop is triggered when the player touches a simple level trigger. The loop iterates 1,000 times, each time hitting a Branch which checks to see if the loop has hit 500 iterations. If it has not, then a message with the current iteration count is broadcast to the screen. Once it exceeds 500, the Branch calls a Custom Event, which in turn breaks the loop. A Custom Event is used for visual clarity, to eliminate the need to have the wire wrap back around to the Break input pin.

Loop iterations will take place between frames, so large loops may incur a performance hit.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/afae4b3d-ce95-461b-8e28-ae7228a3a63a/forloopwithbreak_network.png)

Click for full image.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/156d4def-c0bf-4b5c-9a4a-b0551c4dff1c/brokenat500.png)

| Item | Description |
| --- | --- |
| Input Pins |  |
| **(Unlabeled)** | This execution input launches the loop. |
| **First Index** | Takes in an Int representing the first index of the loop. |
| **Last Index** | Takes in an Int representing the last index of the loop. |
| **Break** | This execution input breaks the loop. |
| Output Pins |  |
| **Loop Body** | This outputs an execution pulse on each iteration of the loop as it moves between the indices. |
| **Index** | This outputs the current index of the loop. |
| **Completed** | This is a standard execution output pin that fires when the for loop has completed. |

### Gate

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/d2eb88c7-9306-48c3-8a14-c49b576abce8/gate_example.png)

A **Gate** node is used as a way to open and close a stream of execution. Simply put, the Enter input takes in execution pulses, and the current state of the gate (open or closed) determines whether those pulses pass out of the Exit output or not.

In this simple example, a timeline with no tracks, with both auto-play and looping activated, updates the Enter input pin of a gate. In the level are two triggers. One trigger opens the gate, the other closes it. If the gate is open, then execution pulses leave the Exit pin and a Print String is called that logs a message to the screen. Once the player touches the Close trigger, the gate closes and the message stops displaying. If they then touch the Open trigger, the message starts to appear again.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/a0f13691-1e2b-49ab-948c-56f69ed4e89e/gate_network.png)

Click for full image.

| Item | Description |
| --- | --- |
| Input Pins |  |
| **Enter** | This execution input represents any execution that is to be controlled by the gate. |
| **Open** | This execution pin sets the state of the gate to *open*, allowing execution pulses to pass through to the Exit output pin. |
| **Close** | This execution pin sets the state of the gate to *closed*, stopping execution pulses from passing through to the Exit output pin. |
| **Toggle** | This execution pin reverses the current state of the gate. *Open* becomes *closed* and vice versa. |
| **Start Closed** | This boolean input determines the starting status of the gate. If set to *true* the gate begins in a closed state. |
| Output Pins |  |
| **Exit** | If the gate status is currently *open*, then any execution pulses that hit the Enter input pin will cause a pulse to leave the Exit output pin. If the gate is *closed*, the Exit pin is nonfunctional. |

### MultiGate

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/25befdf6-d1da-46c3-9728-8bb836911cdc/multigate_example.png)

The **MultiGate** node takes in a single data pulse and routs it to any number of potential outputs. This can take place sequentially, at random, and may or may not loop.

In this example, a simple looping and auto-playing timeline is outputting a pulse every half-second. That pulse hits the MultiGate and is routed accordingly, triggering one of a series of Print String nodes, which, when played in order, reveal a special message.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/3220c95f-f701-4258-b42b-9448a0ec1729/multigate_network.png)

Click for full image.

| Item | Description |
| --- | --- |
| Input Pins |  |
| **(Unlabeled)** | This is the primary execution input that takes in any pulses that need to be routed by this MultiGate. |
| **Reset** | This sets the current output index back to 0 by default, or to the currently set *Start Index*, if not -1. |
| **Is Random** | If set to *true*, then the outputs are chosen in random order. |
| **Loop** | If set to *true*, then the outputs will continuously repeat in a loop. If *false*, then the MultiGate ceases to function once all outputs have been used. |
| **Start Index** | Takes in an Int to represent the output index the MultiGate should use first. A value of -1 is the same as not specifying a start point. |
| Output Pins |  |
| **Out #** | Each output represents a possible output pin that the MultiGate may use to send out a routed execution pulse. |
| **Add pin** | Though not really an output pin, this button allows you to add as many outputs as you like. Outputs can be removed by right-clicking and choosing Remove Output Pin. |

### Sequence

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/d03a95bd-5909-40f4-ba76-4b712ba6e9f1/sequence_example.png)

The **Sequence** node allows for a single execution pulse to trigger a series of events in order. The node may have any number of outputs, all of which get called as soon as the Sequence node receives an input. They will always get called in order, but without any delay. To a typical user, the outputs will likely appear to have been triggered simultaneously.

In this example, the sequence node is called at the beginning of the level. It then fires off 5 Print String nodes in order. However, without a meaningful delay, it will seem as if the messages appear at almost the same time as one another.

![](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/ef8bea0c-f8c3-487a-9ac9-5e2b14066748/sequence_network.png)

Click for full image.

| Item | Description |
| --- | --- |
| Input Pins |  |
| **(Unlabeled)** | This is the primary execution input that takes in any pulses that need to be routed by this Sequence. |
| Output Pins |  |
| **Out #** | Each output represents a possible output pin that the Sequence may use to send out a routed execution pulse. |
| **Add pin** | Though not really an output pin, this button allows you to add as many outputs as you like. Outputs can be removed by right-clicking and choosing Remove Output Pin. |

### WhileLoop

![](https://dev.epicgames.com/documentation/en-us/unreal-engine/BP_WhileLoop-1.png "BP_WhileLoop-1.png")

A test condition and a body are all that make up a **WhileLoop**. Before executing the statement(s) in its body, the Blueprint evaluates WhileLoop's test condition to determine if it is true. After executing the statement(s) in its body, the Blueprint re-evaluates the test condition, and if the condition remains true, it keeps executing the statement(s) in the loop's body. Otherwise, if the test condition returns false, the Blueprint terminates the loop and exits the loop's body.

The following table describes the node's pins:

| Item | Description |
| --- | --- |
| Input Pins |  |
| **(Unlabeled)** | This is the primary execution input that takes in any pulses that will drive this WhileLoop. |
| **Condition** | This is the loop's test condition. |
| Output Pins |  |
| **Loop Body** | This outputs an execution pulse on each iteration of the loop as it moves between the indices. |
| **Completed** | This is a standard execution output pin that fires as soon as the loop ends. |

**Best Practice:** Consider these questions when using WhileLoop.

- What is the loop's terminating condition?
- Is the condition initialized before the loop's first test?
- Is the condition being updated in each loop cycle before testing the condition again?

Answering these three questions should help you avoid infinite loops, which can cause your game to become unresponsive (or crash).
