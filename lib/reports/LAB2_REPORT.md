1.	Domain Analysis
a.	Purpose of the laboratory work
Familiarization with the fundamental concepts of operating systems for embedded electronic systems, and with their practical implementation for microcontroller applications. The laboratory work aims to develop an application that executes multiple tasks in a sequential bare-metal scheduler, using recurrence and offsets, provider/consumer synchronization, and deterministic timing based on a hardware timer interrupt. The implementation must clearly demonstrate scheduling methodology, inter-task data flow, and non-blocking task behavior, with complete software architecture documentation.

b.	Objectives of the laboratory work
-	Familiarize with task scheduling and execution principles in an embedded system, with focus on non-preemptive sequential execution (bare-metal).
-	Understand and apply recurrence and offset parameters for CPU utilization optimization and deterministic execution order.
-	Implement synchronization and communication mechanisms between tasks based on provider/consumer variables and event flags.
-	Analyze practical advantages and limitations of sequential cyclic scheduling for low-resource MCUs.
-	Document software architecture, timing diagrams, and hardware/software interfaces used in Lab2_1.

c.	Problem definition
Design and implement a microcontroller-based button duration monitor that uses a timer-interrupt-driven sequential scheduler to process three periodic tasks: button event detection, statistics and LED feedback, and periodic reporting through serial STDIO.
Functional specifications:
FR-1.0: Scheduler Interface
FR-1.1: System shall generate a 1 ms system tick using Timer1 CTC interrupt.
FR-1.2: Scheduler shall execute tasks in fixed priority order (Task1 > Task2 > Task3).
FR-1.3: Scheduler shall support per-task recurrence and offset values.
FR-1.4: Task execution shall be non-blocking during normal runtime.
FR-2.0: Button Monitoring (Task 1)
FR-2.1: Task 1 shall run every 20 ms and call ButtonDriver::Update().
FR-2.2: On confirmed release, system shall measure last press duration in ms.
FR-2.3: Press duration shall be classified SHORT (<500 ms) or LONG (>=500 ms).
FR-2.4: Task 1 shall publish event data through shared variables and print event using printf.
FR-3.0: Statistics and LED Feedback (Task 2)
FR-3.1: Task 2 shall run every 50 ms with 10 ms offset and consume Task 1 events.
FR-3.2: System shall maintain total, short, and long press counters plus accumulated duration.
FR-3.3: SHORT press shall trigger 5 LED blinks (10 toggles x 50 ms = 500 ms).
FR-3.4: LONG press shall trigger 10 LED blinks (20 toggles x 50 ms = 1000 ms).
FR-3.5: LED feedback shall be implemented as non-blocking state progression per task invocation.
FR-4.0: Periodic Reporting (Task 3)
FR-4.1: Task 3 shall run every 10000 ms with 100 ms offset.
FR-4.2: System shall print total, short, long, and average press duration via printf.
FR-4.3: Statistics shall be reset after each report window.
FR-5.0: User Feedback and Diagnostics
FR-5.1: System shall output startup status and diagnostics through serial STDIO.
FR-5.2: System shall perform startup button self-check and print PASS/FAIL.
FR-5.3: LED shall remain OFF when no blink sequence is active.

Non-functional specifications:
NFR-1.0: Performance
NFR-1.1: Tick period shall be 1 ms using Timer1 CTC configuration (16 MHz, prescaler 64, OCR1A=249).
NFR-1.2: Debounce sampling interval shall be 20 ms (5 samples target window ~100 ms).
NFR-1.3: LED feedback timing shall be 500 ms for SHORT and 1000 ms for LONG events.
NFR-1.4: Runtime task logic shall avoid blocking delays and busy-wait loops.
NFR-2.0: Reliability
NFR-2.1: System shall avoid duplicate press events caused by switch bounce.
NFR-2.2: Provider/consumer flow shall preserve event consistency between Task 1 and Task 2.
NFR-2.3: Statistics shall remain coherent across each 10-second reporting window.
NFR-2.4: System shall sustain repeated operation without hangs or scheduler loss.
NFR-3.0: Resource Efficiency
NFR-3.1: RAM usage shall remain within Arduino Uno limits (2KB total).
NFR-3.2: Flash usage shall remain within Arduino Uno limits (32KB total).
NFR-3.3: No dynamic memory allocation shall be used in critical scheduling paths.
NFR-3.4: CPU load shall remain low (estimated <1%).
NFR-4.0: Maintainability
NFR-4.1: Code shall preserve modular layers: SRV (Lab2), ECAL (drivers), MCAL (Arduino/AVR).
NFR-4.2: Driver interfaces shall remain reusable across labs.
NFR-4.3: Task configuration shall be centralized by constants (period/offset).
NFR-4.4: Non-obvious logic (timing, ISR, provider/consumer) shall be documented.
NFR-5.0: Scalability
NFR-5.1: Scheduler table shall be extensible for additional tasks.
NFR-5.2: Thresholds (short/long) and timing values shall be easily configurable.
NFR-5.3: GPIO pin assignments shall be configurable via constants.
NFR-5.4: Reporting format shall support extension with additional metrics.

System Constraints:
- Platform: Arduino Uno (ATmega328P, 16MHz, 2KB RAM, 32KB Flash)
- Input device: Push-button on D3 (INPUT_PULLUP, active-low)
- Output indicator: Green LED on D10 with 220Ω resistor
- Communication: Serial STDIO (TX/RX 0/1) at 9600 baud
- Development tools: PlatformIO IDE, VS Code, Wokwi simulator

d.	Description of the Technologies Used and the Context
The Lab2_1 implementation is structured as a finite periodic scheduling model (cyclic executive) with three sequential tasks dispatched every 1 ms tick. A hardware timer interrupt (Timer1 CTC) acts as time base, while all task functions execute in main context to keep serial I/O safe and deterministic. This architecture enables predictable, analyzable behavior without RTOS overhead.
User input is acquired through a debounced push-button driver. The driver is sampled every 20 ms by Task 1 and confirms state transitions after stable readings, mitigating mechanical bounce effects. On release, press duration is computed and classified into SHORT/LONG categories, then published as provider data for the next consumer task.
The system follows a non-blocking event-driven model. Task 2 consumes event data and advances an LED blink state machine one step per invocation (50 ms), ensuring feedback timing without delay-based blocking. Task 3 reports aggregated statistics every 10 seconds through printf, then resets counters for the next measurement interval.

e.	Presentation of Hardware and Software Components and Their Roles
The system is built around an Arduino Uno (ATmega328P, 16 MHz), which executes scheduler logic and task functions. Input is provided by a single push-button connected to D3 in active-low configuration with internal pull-up. Visual output is provided by a green LED connected to D10 through a 220 Ω current-limiting resistor. The circuit is assembled on breadboard with jumper wires and powered by USB.
Software development is performed in Visual Studio Code with PlatformIO. The Arduino framework provides MCAL APIs and AVR register access. The application reuses modular drivers: ButtonDriver for debounced input and duration capture, LedDriver for LED control, and SerialStdioDriver for printf redirection. Testing and validation are performed in both Wokwi simulation and physical Arduino hardware.

f.	System Architecture Explanation and Justification of the Solution
The system architecture follows a layered modular approach. The application/service layer (Lab2_main.cpp) contains scheduler setup, tick handling, and Task1/Task2/Task3 logic. The driver layer contains reusable ECAL components (button, LED, serial stdio). The hardware abstraction and register layer (Arduino + AVR) provides GPIO, timer, and interrupt control.
Using a sequential non-preemptive scheduler is justified by low complexity, deterministic timing, and low CPU load. Provider/consumer ordering is guaranteed by task table priority (Task1 before Task2), which ensures fresh button event data is available when statistics are updated. Timer1-based ticking ensures consistent temporal behavior independent of loop jitter.
The selection of printf through SerialStdioDriver is justified by structured diagnostics and periodic report formatting. It provides clear observability for validation (event detection, classification, counters, averages) and supports debugging without changing application state logic.

g.	Definition of Test Scenarios and Validation Criteria
For Lab2_1, test scenarios are grouped into: (1) Functional validation of the scheduler and all three tasks, (2) Non-functional timing validation (1 ms tick, task recurrence/offset behavior, 500/1000 ms LED feedback windows), (3) Behavioral validation of provider/consumer data flow between Task 1 and Task 2 and statistics reset by Task 3, and (4) Edge case validation (short presses near threshold, rapid repeated presses, no-press reporting interval).
Each scenario includes preconditions (system startup state), execution steps (button action and observation interval), expected results (serial output, LED pattern, counter values), and acceptance criteria (pass/fail). Validation methods include serial log inspection, timestamp comparison using millis(), event count consistency checks, and long-run stability checks.
Acceptance criteria include: correct SHORT/LONG classification against 500 ms threshold, exact blink sequence count (5 or 10), periodic report every ~10 s with coherent metrics, and successful reset of counters after each report. Reliability criteria include repeated operation without lockup and no false duplicate detections under normal usage.

h.	Technological Context and Industry Relevance
The implemented architecture is directly relevant to embedded cyclic scheduling used in:
- Automotive control loops (periodic sensing, processing, actuation)
- Industrial monitoring nodes (deterministic scan/report cycles)
- IoT edge devices (event detection + periodic telemetry)
- Safety signaling modules (non-blocking visual indication patterns)
The principles demonstrated in this laboratory work (timer-driven scheduling, task recurrence/offset, provider/consumer synchronization, and non-blocking output state machines) are foundational in production embedded systems, even when migrated later to RTOS-based implementations.

i.	Relevant Case Study Demonstrating Applicability
A practical analogue is an industrial operator panel module with one service button and one status LED. The module samples button state periodically, classifies interaction type (short/long press), updates internal counters, and emits periodic maintenance telemetry over serial/CAN/UART. Feedback is generated through deterministic non-blocking LED patterns to avoid interrupting the control loop.
Another relevant case is an IoT field node that captures user interaction events and reports summarized usage metrics every fixed interval. The same provider/consumer pattern appears: event acquisition task writes data, aggregation task computes statistics, report task transmits telemetry and clears the window.
These systems prioritize deterministic timing, observability, and low overhead—exactly the design goals demonstrated by Lab2_1.

2.	System Design
The modular implementation (Figure 1) follows separation between interface and implementation. Header files (.h) define task and driver interfaces, while source files (.cpp) implement scheduling, ISR tick generation, button event processing, LED control, and reporting logic.

Figure 1: Structure of the laboratory work
The architecture separates SRV scheduling logic from ECAL drivers and MCAL hardware abstraction (Figure 2). Task descriptors define recurrence, offset, and down-counter state. Timer1 generates a 1 ms interrupt that sets a scheduling flag. Main context executes tasks in deterministic order.

Figure 2: Architecture schema
From the user perspective (Figure 3), interaction is simple: press/release button, receive immediate event classification in serial output, and observe LED blink feedback proportional to press type.

Figure 3: User interaction
The flowchart (Figure 4) starts with scheduler tick handling, checks due tasks, updates button state, publishes event data when a release is detected, consumes data for statistics/blinking, and periodically prints aggregated metrics before reset.

Figure 4: Activity diagram
The state representation (Figure 5) for runtime behavior includes implicit states such as IDLE, PRESS_TRACKING, EVENT_PUBLISHED, BLINK_ACTIVE, and REPORT_WINDOW_RESET. Transitions are triggered by debounced button transitions, task recurrence counters, and report window expiration.

Figure 5: State diagram for the system
The sequence diagram (Figure 6) captures interaction among ButtonDriver, Task1 provider, Task2 consumer, LedDriver, and Task3 reporter. Task1 writes shared variables; Task2 reads/clears event flag and advances LED toggles; Task3 snapshots counters and resets them.

Figure 6: Sequence diagram of user action
The electrical implementation (Figure 7) uses Arduino Uno with a push-button on D3 (to GND, internal pull-up enabled), one green LED on D10 through 220 Ω resistor to GND, and serial USB connection for stdout diagnostics.

Figure 7: Modelated electronic schema
Behavioral decomposition (Figure 8) includes three periodic processes: Button Detection & Classification (20 ms), Statistics & Blink State Machine (50 ms), and Periodic Reporting (10000 ms). Their offsets (0 ms, 10 ms, 100 ms) reduce same-tick contention and preserve provider-before-consumer execution.

Figure 8: Behavior diagrams for each peripheral

3.	Results
The system was tested using PlatformIO and Wokwi simulator, then validated on Arduino Uno hardware. After startup, serial monitor displays scheduler and button self-check messages. During operation, each press release generates a Task 1 log line with measured duration and SHORT/LONG label. Task 2 generates visible LED blink sequences (5 for short, 10 for long). Task 3 prints periodic reports with total, short, long, and average duration, then resets counters for the next 10-second interval.
The testing requirements definition is shown in the following table (Table 1).

Table 1: Testing requirements
ID	Test category	Test description	Expected result
T1	Initialization	Power on system	Serial shows startup messages; scheduler starts; LED initialized correctly
T2	Tick Scheduler	Observe periodic execution	1 ms tick active; Task1/Task2/Task3 run with configured recurrence and offsets
T3	Short Press	Press and release <500ms	Task1 prints SHORT; Task2 triggers 5 blinks (500ms total)
T4	Long Press	Press and hold >=500ms	Task1 prints LONG; Task2 triggers 10 blinks (1000ms total)
T5	Provider/Consumer	Generate multiple presses	Task2 counters match Task1 published events; no stale event reuse
T6	Periodic Report	Wait 10s window	Task3 prints total/short/long/avg and resets statistics
T7	No-Press Window	No interaction for one report period	Report shows zeros, system remains stable
T8	LED Feedback	Alternate short and long presses	Blink count and duration correspond to event class
T9	Debounce Reliability	Rapid repeated button actions	No false duplicate events under normal physical operation
T10	Stability	Continuous repeated operations	System runs without hangs/crashes; reports remain coherent

Next images are the results of testing, including simulation captures (Figure 9-13) and hardware captures (Figure 14).

Figure 9: Startup serial output
Figure 10: Detected short press event
Figure 11: Detected long press event
Figure 12: Periodic report printout
Figure 13: LED blink sequence in simulation
Figure 14: Hardware setup and live test

4.	Conclusions
The implemented Lab2_1 application meets the requirements for a deterministic sequential embedded scheduler with provider/consumer synchronization. Timer1-driven 1 ms ticks ensure stable temporal behavior, while recurrence and offsets enable predictable multi-task execution with very low CPU load. The architecture cleanly separates SRV scheduling logic from reusable ECAL drivers, improving maintainability and extensibility.
The objectives of the laboratory work were achieved: task scheduling principles were implemented, inter-task communication was validated, and non-blocking feedback behavior was demonstrated through LED state progression instead of delay-based runtime blocking. The periodic reporting mechanism provides clear runtime observability and confirms coherent statistics over fixed windows.
Future improvements can include: migration of the same task set to FreeRTOS for comparative analysis (preemptive vs non-preemptive), introduction of bounded ring buffers for event queues, optional EEPROM persistence for long-term counters, and addition of a second input/output channel to evaluate scalability under increased task load.

During the preparation of this report, AI-based assistance tools (including Copilot and ChatGPT) were used for drafting and refinement of textual documentation. Technical details were cross-checked against the implemented source code and adapted to laboratory requirements.

Bibliography
[1] J. G. Ganssle, The Art of Designing Embedded Systems, 2nd ed. Burlington, MA, USA: Elsevier, 2007.
[2] M. Barr, Embedded C Programming and the Atmel AVR, 2nd ed. Oxford, U.K.: Newnes, 2006.
[3] R. L. Boylestad and L. Nashelsky, Electronic Devices and Circuit Theory, 11th ed. Upper Saddle River, NJ, USA: Pearson Education, 2013.
[4] “Arduino Uno Rev3 | Arduino Documentation.” [Online]. Available: https://docs.arduino.cc/hardware/uno-rev3/
[5] “Welcome to Wokwi! | Wokwi Docs.” [Online]. Available: https://docs.wokwi.com/
[6] GitHub repository. Available: https://github.com/danganhuh/ES_labs
