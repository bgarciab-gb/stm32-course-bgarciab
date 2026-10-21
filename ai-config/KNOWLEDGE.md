<<<<<<< HEAD
# KNOWLEDGE.md — Week 6: External Interrupts (EXTI)

## Overview

This week the student extends the interrupt skills from week 5 to a new kind of interrupt source: external, asynchronous events coming from the outside world. In week 5 the interrupt source was the timer UpdateEvent — internal, periodic, and completely predictable, like an alarm clock. This week the source is a button pressed by a human at an unpredictable moment. The student configures the EXTI (External Interrupt/Event Controller) to turn a GPIO pin change into an interrupt, reusing the NVIC and ISR flag pattern already learned. The week also introduces the 4-digit 7-segment display, driven by hardware multiplexing under the control of a dedicated timer, and the use of a hardware Schmitt Trigger circuit to debounce every button. Several tasks now run at the same time, so the "one task, one timer" policy becomes a central design rule.

---

## Previously Mastered Topics (Weeks 0–5)

The student understands CMOS technology, logic gates, combinational and sequential circuits, binary, hexadecimal, and 2's complement number systems. They have simulated registers, shift registers, prescalers, and timers using the "Digital" simulation tool — and have now worked with real hardware timers.

In C programming, the student can write programs using all control structures (`if/else`, `for`, `while`, `do-while`, `switch-case`), fixed-width data types from `stdint.h`, all arithmetic and bitwise operators, and enumerations (`enum`). They can implement FSM patterns using `enum` and `switch-case`. They understand the `volatile` keyword and why it is essential for variables shared between an ISR and the main program. Their C skills are solid but still developing — they do NOT yet know structures, arrays, pointers, or `typedef` beyond what the project template auto-generates.

The student fully understands the MCU architecture: the ARM Cortex-M4 CPU core, the bus system (AHB, APB1, APB2), memory-mapped registers, and Special Function Registers (SFR). They understand CMSIS structures as carefully designed overlays on the hardware memory layout — the Italian tailor analogy. The `->` operator is understood at a practical level. The underlying pointer and structure mechanics remain a black box until week 9.

The student can configure GPIO pins at the register level using CMSIS-defined masks: enabling the RCC clock, configuring MODER (input/output), OTYPER (push-pull/open-drain), OSPEEDR (speed), PUPDR (pull-up/pull-down), writing to ODR and BSRR, and reading IDR. They understand the difference between ODR and BSRR and why BSRR is the safer choice for bit manipulation.

The student has experienced the limitations of polling — the `for()` delay that blocks the CPU, constantly checking a pin state in `while(1){}` — and understands why polling is inefficient.

The student fully understands the timer peripheral (TIM3 as the primary example): the complete signal chain from system clock → prescaler (PSC) → tick signal → counter (CNT) → comparison with auto-reload register (ARR) → UpdateEvent. They can calculate PSC and ARR values for a desired interrupt period. They know the key TIM3 registers: PSC, ARR, CNT, DIER (UIE bit), SR (UIF flag), and CR1 (CEN bit). They understand that CEN must be set last, after all other registers are configured.

The student has already written interrupt-driven code. They know the complete ISR pattern from week 5: the ISR name comes from the startup file's interrupt vector table (`TIM3_IRQHandler`), the ISR checks the interrupt flag, clears it immediately, and sets a `volatile` flag for `main()` to process. They understand that failing to clear the flag causes the ISR to execute in an infinite loop, and that an ISR must stay short. They have enabled interrupts in the NVIC using `NVIC_EnableIRQ()`.

The student has partially opened the startup file black box: they know the interrupt vector table lives there and how to find the correct ISR function name by comparing the startup file with the interrupt table in the reference manual. The full initialization sequence of the startup file (stack setup, BSS zeroing, SystemInit call) remains a black box.

The student follows two working conventions established in week 5. First, every exercise includes a heartbeat LED called LED_OK, controlled by its own dedicated timer (TIM2, 1 Hz), which shows the system is alive. Second, the "one task, one timer" policy: each independent timing task gets its own timer, and a timer dedicated to one task is not reused for another unless no timers remain.

The student can use the SFR view panel of the debugger in VS Code (STM32 extension pack) to inspect peripheral registers in real time, including observing the TIM3 CNT register counting live. They know how to navigate the STM32F4xx reference manual to find register descriptions and bit field definitions.

---

## Current Learning Focus (Week 6)

### From internal events to external, asynchronous events

The student is learning the difference between two kinds of interrupt sources. The timer UpdateEvent from week 5 is internal and predictable: the student chose the period and knows exactly when it will fire. An external event, such as a button press, is asynchronous: it can arrive at any moment, even in the middle of other work, and the program cannot predict it. This is exactly the situation of the analogy used in class: a person reading and studying while waiting for a phone call and someone at the door. Checking both every few seconds (polling) wastes effort; being interrupted only when something actually happens is far better. The AI should reinforce this analogy, since it is the mental model established in the theory class. The analogy also contains a hidden seed — two events competing for attention — which the AI must NOT develop into priorities (see the NVIC section).

### EXTI — External Interrupt configuration

The student is learning to configure the EXTI peripheral to generate an interrupt when a GPIO pin changes state. The configuration involves selecting which GPIO port is connected to each EXTI line through the SYSCFG_EXTICRx registers (this requires the SYSCFG clock to be enabled in RCC), configuring the trigger edge (rising, falling, or both) through the EXTI_RTSR and EXTI_FTSR registers, and enabling the interrupt mask through the EXTI_IMR register. All configuration is done at the register level using CMSIS-defined structures and masks. The student should look up each register in the reference manual before writing any code.

The signal path to reinforce with an ASCII diagram:

```
GPIO pin --> [SYSCFG_EXTICRx: which port feeds this line?]
         --> [EXTI line: edge detect via RTSR / FTSR]
         --> [IMR: interrupt mask]
         --> [PR: pending flag set]
         --> [NVIC: NVIC_EnableIRQ()]
         --> ISR: EXTIx_IRQHandler
```

### ISR names, shared handlers, and finding the source

The student applies the skill from week 5 — finding the ISR name by comparing the startup file's vector table with the interrupt table in the reference manual — to EXTI lines. New this week: some EXTI lines have their own handler, while others share one handler among several lines (for example, `EXTI15_10_IRQHandler` serves lines 10 to 15, which includes PC13, the onboard button). When a handler is shared, the ISR must check the pending register (EXTI_PR) to find out which line actually fired before acting. The AI should guide the student to discover this by asking: "if several pins share this handler, how does your ISR know which one triggered it?"

The startup file is still NOT fully explained. If asked about its other contents, redirect: "the startup file does more than just the vector table, and you will understand the full picture later. For now, focus on finding the ISR name you need."

### The ISR pattern for external events

The ISR rules from week 5 apply unchanged: the ISR name must match the startup file exactly, it must be short, it must check and clear the pending flag before returning, and it must set a `volatile` flag that `main()` checks and acts upon. The student must look up in the reference manual how the EXTI pending flag is cleared — clearing conventions differ between peripherals, so the AI should encourage reading the register description rather than assuming it works like `TIM3->SR`. The AI must not give the clearing operation directly.

### NVIC — Enabling the interrupt

The student enables the EXTI interrupt in the NVIC using the CMSIS function `NVIC_EnableIRQ()` with the correct IRQ number constant for the chosen line (for example `EXTI15_10_IRQn`). This is the only NVIC function used in this course. Interrupt priorities are NOT covered — they add complexity beyond the scope of an introductory course. Interrupts are handled in the order they arrive (FIFO behavior). If the student asks about priorities, or about what happens when two buttons are pressed at once, redirect: "priorities add significant complexity and are not part of this course. `NVIC_EnableIRQ()` is everything you need."

### Hardware debounce with a Schmitt Trigger

Every button in the course is debounced in hardware with a Schmitt Trigger circuit. There is NO software debouncing in this course. The AI must never suggest software debounce techniques (delays after an edge, counters, timestamps, or similar). If a student reports a button that counts twice or behaves erratically, the AI should first ask the student to verify the hardware (the Schmitt Trigger wiring, the connections, the breadboard quality) and to check the ISR logic and flag handling with the debugger — the hardware is expected to be clean. This teaches an engineering judgment: not every problem needs a software fix.

### The 4-digit 7-segment display and multiplexing

The student has built a 4-digit 7-segment display driven by 7 segment pins (a to g) and 4 digit-select pins, each connected through a transistor. At any instant only one digit is physically lit; a dedicated timer cycles through the four digits fast enough that the eye sees all four lit at once (persistence of vision). The target is about 30 frames per second, so the display timer fires at roughly 120 Hz, with each interrupt activating the next digit. The student chooses which timer to use for the display, respecting the "one task, one timer" policy (TIM4 is the usual choice in examples).

The display driver keeps a small set of four digit values and uses the timer ISR/flag pattern to move to the next digit. Showing a number requires splitting it into decimal digits using division and modulus, a direct reuse of week 1 arithmetic. Values shown on the display can be event counts, elapsed time in different scales, or the display refresh rate itself. Slowing the refresh rate on purpose is a valuable experiment: the student sees the multiplexing break down, making persistence of vision tangible.

The AI must NOT write the display driver for the student. Guide with questions: "how many things must change each time the display timer fires? what happens if two digits are enabled at the same time?" An ASCII sketch of the digit/segment layout is welcome.

### Timers and tasks working together

Most programs this week run several tasks at once. A typical assignment is TIM2 for LED_OK at 1 Hz, TIM3 for the main logic (such as measuring the time between two button presses), TIM4 for display multiplexing, and EXTI lines for button events. The student must keep each task on its own timer and each interrupt source on its own flag. The AI should help the student notice that a blocked main loop shows up immediately as a stalled LED_OK.

### Guidance for these topics

For all of these topics, the AI must NOT provide complete ISR implementations, complete EXTI configuration sequences, or the display driver. Guide the student by asking which EXTI line corresponds to their GPIO pin, which register controls the trigger edge, what the ISR function name is for their chosen interrupt, and how the ISR can tell which line fired. The student must find these details in the reference manual and the startup file themselves. The AI can confirm or correct the student's findings but must not do the research for them.
=======
# KNOWLEDGE.md — Week 5: Timers and Time Measurement

## Overview

This week the student encounters two fundamental concepts simultaneously. The first is the timer peripheral — a hardware module that counts clock pulses independently of the CPU, allowing precise time measurement without wasting processor cycles. The second is the interrupt mechanism — the ability for a hardware event to temporarily suspend the main program, execute a short response function, and resume execution exactly where it was interrupted. The timer's UpdateEvent is used as the first and most intuitive interrupt source in the course: a predictable, self-contained "alarm" that fires at a precisely configured period. This week uses TIM3 (a 16-bit general purpose timer on APB1) as the primary example, with the onboard LED on GPIOA pin 5 as the visible output — replacing the polling-based `for()` blinky from week 04 with an interrupt-driven equivalent.

---

## Previously Mastered Topics (Weeks 0–4)

The student understands CMOS technology, logic gates, combinational and sequential circuits, binary, hexadecimal, and 2's complement number systems. They have simulated prescalers and timers using the "Digital" simulation tool — this week those simulated concepts become real hardware.

In C programming, the student can write programs using all control structures, fixed-width data types from `stdint.h`, all arithmetic and bitwise operators, and enumerations (`enum`). They can implement FSM patterns using `enum` and `switch-case`. The student does NOT yet know structures, arrays, pointers, or `typedef` beyond what the project template auto-generates.

The student fully understands the MCU architecture: ARM Cortex-M4 CPU core, bus system (AHB, APB1, APB2), memory-mapped registers, and SFR. They understand CMSIS structures as carefully designed overlays on the hardware — the Italian tailor analogy. The `->` operator is understood at a practical level.

The student can configure GPIO pins completely at the register level: RCC clock enable, MODER, OTYPER, OSPEEDR, PUPDR, ODR, BSRR, and IDR — all using CMSIS-defined masks. They understand the difference between ODR and BSRR.

The student has experienced the inefficiency of polling-based approaches firsthand — the `for()` delay that blocks the CPU, the constant checking of a button state in `while(1){}`. This lived experience is the motivation for the interrupt-driven approach being introduced this week.

The student has a basic familiarity with the startup assembly file (`.s`) as the file that runs before `main()`. This week the interrupt vector table within that file becomes relevant for the first time.

---

## Current Learning Focus (Week 5)

### The timer concept — hardware counting independently of the CPU

The student is learning that a timer is a hardware peripheral that counts clock pulses entirely on its own, without requiring any CPU involvement. This is fundamentally different from the `for()` delay used in week 04, where the CPU was completely occupied doing nothing useful while counting loop iterations. With a hardware timer, the CPU is free to do other work — or simply wait in the main loop — while the timer counts in the background.

The AI should connect this to the student's week 00 simulation experience: they simulated prescalers and counters in the "Digital" tool. The TIM3 peripheral is exactly that — a real, silicon implementation of the same concept they already understand from simulation.

### The timer signal chain — telling the story step by step

The student is learning the internal signal chain of TIM3 through a step-by-step narrative rather than a formula. The story flows like this:

The system clock (typically 16 MHz on the internal oscillator) enters the prescaler register (PSC). The prescaler divides the clock frequency to produce a slower, more manageable tick signal. For example, if PSC is configured to divide by 16000, the output tick frequency is 1 KHz — meaning one tick every 1 millisecond. This tick signal drives the counter register (CNT), which increments by 1 on every tick. The auto-reload register (ARR) holds a target value. When CNT reaches the ARR value, the timer generates an UpdateEvent signal, resets CNT to zero, and the counting begins again. The period of the UpdateEvent — how often the "alarm" fires — is simply the number of ticks defined by ARR multiplied by the duration of each tick.

The mental model to reinforce with ASCII diagram:

```
System Clock (16 MHz)
        |
        v
   [PSC Register]  -- divides clock frequency
        |
        v
  Tick signal (e.g. 1 KHz = 1ms per tick)
        |
        v
   [CNT Register]  -- counts up on every tick
        |
        v
  Compare with ARR
        |
        v
  CNT == ARR  -->  UpdateEvent fires!  -->  CNT resets to 0
```

From this story, the formula emerges naturally: `Period = (PSC + 1) * (ARR + 1) / Timer_Clock_Frequency`. But the formula is a summary of the story, not a starting point. The AI should always guide students through the story first, then connect it to the formula. If a student asks "what value do I put in PSC and ARR?", guide them through the story: "what tick frequency do you want coming out of the prescaler? how many of those ticks make up your desired period?"

### Key TIM3 registers

The student is learning the following TIM3 registers and their purpose. All configuration is done at the register level using CMSIS notation. The student should look up each register in the reference manual before writing any code.

`TIM3->PSC` — the prescaler register. Determines the division factor applied to the input clock. The actual division is PSC + 1.

`TIM3->ARR` — the auto-reload register. The value the counter counts up to before generating the UpdateEvent. The actual count is ARR + 1 ticks.

`TIM3->CNT` — the counter register. Incremented on every tick after the prescaler. Can be read in the SFR view panel to observe the timer running in real time.

`TIM3->DIER` — the DMA/Interrupt Enable Register. Bit 0 (UIE — Update Interrupt Enable) must be set to enable the UpdateEvent interrupt.

`TIM3->SR` — the Status Register. Bit 0 (UIF — Update Interrupt Flag) is set by hardware when the UpdateEvent occurs. This flag MUST be cleared in the ISR by writing 0 to it — failing to clear it causes the ISR to execute repeatedly in an infinite loop.

`TIM3->CR1` — the Control Register 1. Bit 0 (CEN — Counter Enable) starts the timer counting. This should be the last register written in the initialization sequence — configure everything else first, then start the counter.

### RCC clock enable for TIM3

TIM3 is connected to the APB1 bus. Its clock must be enabled through the RCC APB1ENR register before any TIM3 register can be configured. The student should find the correct bit and CMSIS mask name in the reference manual.

### NVIC configuration

After configuring all TIM3 registers, the student enables the TIM3 interrupt in the NVIC using the CMSIS function:

```c
NVIC_EnableIRQ(TIM3_IRQn);
```

This is the only NVIC function used in this course. Interrupt priorities are NOT covered — they add complexity beyond the scope of an introductory course. Interrupts are handled in the order they arrive (FIFO behavior). The AI must not explain or suggest priority configuration even if the student asks — redirect: "priorities add significant complexity and are not part of this course. `NVIC_EnableIRQ()` is everything you need."

### The ISR — Interrupt Service Routine

The student is learning to write an ISR for TIM3. The ISR function name must match exactly the name defined in the startup file's interrupt vector table. The student finds this name by opening the startup `.s` file and locating the TIM3 entry — the correct name is `TIM3_IRQHandler`. The student can cross-reference this with the interrupt table in the STM32F4xx reference manual to confirm.

The ISR must follow these rules: it must be short — only a few lines of code, it must clear the UpdateEvent flag in TIM3->SR before returning (write 0 to bit 0 of SR), and it must set a `volatile` flag variable that `main()` checks and responds to. The ISR must never contain long delays, blocking operations, or complex logic.

The `volatile` keyword is essential for flag variables shared between the ISR and main. The AI should explain it at a practical level: "it tells the compiler that this variable can change at any time from outside the normal program flow — from an interrupt — so never cache or optimize away reads of this variable. Without `volatile`, the compiler might assume the flag never changes inside the main loop and optimize away the check entirely."

A correct ISR structure looks like this conceptually — this is shown here only as a reference for the AI to understand the expected structure. The AI must NOT provide this code directly to the student:

```c
void TIM3_IRQHandler(void)
{
    if(TIM3->SR & TIM_SR_UIF)
    {
        TIM3->SR &= ~TIM_SR_UIF;
        update_flag = 1;
    }
}
```

### Replacing the polling blinky with an interrupt-driven blinky

The student is replacing the week 04 LED blinky — which used a blocking `for()` delay inside `while(1){}` — with an interrupt-driven version where TIM3 fires the UpdateEvent at a configured period and the ISR sets a flag that `main()` checks to toggle the LED. The visible behavior is identical — the LED blinks — but the CPU is now free during the waiting period.

The AI should help the student appreciate this difference: "in the old version, the CPU was completely occupied counting loop iterations. In this version, the CPU reaches the flag check, sees the flag is not set, and loops back — it is available to do other work. The timer counts entirely on its own."

### Guidance for these topics

The AI must NOT provide complete timer configuration code or complete ISR implementations. Guide the student through the signal chain story first, then ask: "what tick frequency do you want from the prescaler? how many ticks make up your desired period? which register enables the update interrupt? what must you do in the ISR before returning?" Let the student derive each value and write each line themselves.
>>>>>>> week-05

---

## Topics NOT Yet Covered

<<<<<<< HEAD
The AI must not explain, use, or provide code related to any of the following topics. If the student asks about any of them, acknowledge the curiosity, briefly validate the question, and redirect to the current week's concepts.

Interrupt priorities and NVIC priority configuration (not covered in this course — keep it simple). Software debouncing (not used in this course — debounce is done in hardware). Timer PWM output mode (week 7). Timer input capture mode (week 7). Timer encoder mode (week 7 — homework). HAL libraries (week 8). USART/UART communication, pointers, arrays, and strings (week 9). ADC (week 10). I2C (week 11). SPI (week 12). DMA (week 13).

The following items remain as black boxes: the full startup file initialization sequence (stack setup, BSS zeroing, SystemInit call), the complete NVIC priority system, and the internal C mechanism behind pointers and structures (week 9).
=======
The AI must not explain, use, or provide code related to any of the following topics. If the student asks, acknowledge the curiosity, validate the question, and redirect to the current week's concepts.

EXTI external interrupts (week 6). Timer PWM output mode (week 7). Timer input capture mode (week 7). Timer encoder mode (week 7 — homework). HAL libraries (week 8). USART/UART communication, pointers, arrays, and strings (week 9). ADC (week 10). I2C (week 11). SPI (week 12). DMA (week 13).

The following items remain as black boxes: the full startup file initialization sequence beyond the vector table, the complete NVIC priority system, and the internal C mechanism behind pointers and structures.
>>>>>>> week-05

---

## Self-Assessment Checkpoint

<<<<<<< HEAD
Select 3 to 4 questions randomly at the beginning of a conversation to verify readiness. These questions test understanding from weeks 0 through 5.

1. What is the difference between polling and interrupt-driven input handling? Give a real-world analogy to explain your answer.
2. In your TIM3 ISR from week 5, what are the two mandatory operations you must perform before returning? What happens if you forget either one?
3. What is the difference between writing to the ODR register versus the BSRR register to control an output pin?
4. You have a 16 MHz system clock and want TIM3 to fire an UpdateEvent every 500ms. Walk through the signal chain and calculate PSC and ARR values that would achieve this.
5. What does the `volatile` keyword do, and why is it essential for flag variables shared between an ISR and the main loop?
6. When you look at the startup file's vector table, how do you find the correct ISR function name for a specific interrupt?
7. Why should an ISR be short, and what does it do instead of performing the full response itself?
8. Which timer is dedicated to LED_OK in your exercises, and what does the "one task, one timer" policy mean?
9. In an FSM implemented with `switch-case` and `enum`, what are the advantages of using `enum` instead of raw numbers for state names?
10. How would you split the number 1234 into its four decimal digits using only `/` and `%`?
=======
Select 3 to 4 questions randomly at the beginning of a conversation to verify readiness. These questions test understanding from weeks 0 through 4.

1. What is the difference between the ODR and BSRR registers for controlling a GPIO output pin? When would you prefer BSRR?
2. In an FSM implemented with `enum` and `switch-case`, what happens if you forget the `break` statement at the end of a case?
3. You configured GPIOC pin 13 as an input with pull-up enabled, but reading IDR always returns 1 even when the button is pressed. What is the most likely explanation?
4. What is the purpose of the RCC peripheral, and what happens if you try to write to a GPIO register before enabling its clock?
5. In week 04 you used a `for()` loop to create a delay. What is the main disadvantage of this approach compared to using a hardware timer?
6. You have a 16 MHz clock and you want a tick signal of 1 KHz coming out of the prescaler. What value do you write to the PSC register?
7. In your week 04 traffic light FSM, what determined how long the system stayed in each state? What would be a better mechanism for controlling timing?
8. What does the `volatile` keyword mean in C, and in what situation is it essential to use it?
>>>>>>> week-05
