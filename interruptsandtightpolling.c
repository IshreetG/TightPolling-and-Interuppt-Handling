#include <stdio.h>
#include "sys/alt_irq.h" // Altera's interrupt controller interface code
#include "system.h" // Macros that allow easy access to PIO hardware
#include "altera_avalon_pio_regs.h" // Allows us to easily read/write to PIO registers


// Given code to execute Background Task calls:
int background()
{
	// LED 3 signal pulse to indicate background task call
	IOWR(LED_PIO_BASE, 0, 0x8);

	int j;
	int x = 0;
	int grainsize = 4;
	int g_taskProcessed = 0;

	for(j = 0; j < grainsize; j++)
	{
		g_taskProcessed++;
	}

	// Turn LED 3 off
	IOWR(LED_PIO_BASE, 0, 0x0);
	return x;
}


// Interrupt Service Routine
// Declared static to ensure the compiler does not optimize it out of compilation
static void stimulus_detected_ISR(void* context, alt_u32 id)
{
	// LED 2 signal pulse to indicate ISR call
	IOWR(LED_PIO_BASE, 0, 0x4); //turning on and off led 2
	IOWR(LED_PIO_BASE, 0, 0x0);

	// Pulse 'response_out'
	IOWR(RESPONSE_OUT_BASE, 0, 0x1);
	IOWR(RESPONSE_OUT_BASE, 0, 0x0);

	// Clear the interrupt at the end of the ISR, to prevent a deadlock
	IOWR(STIMULUS_IN_BASE, 3, 0);
}


int main()
{
	// Select tightpolling if Switch1 is up
	alt_8 switches = IORD(SWITCH_PIO_BASE,0); //here we know that switch 1 is up forf tightpolling 
	int tight_polling_selected = 0;
	if(switches & 0x1) { //if the first bit is on then we know tight polling is selected
		tight_polling_selected = 1;
	}

	 if(!tight_polling_selected) {
	 	////// APPROACH 1: INTERRUPTS //////
		printf("Interrupt method selected.\n");
		printf("Period, Pulse_Width, BG_Tasks Run, Latency, Missed, Multiple\n\n");
		// Wait for PB[0] to be pressed
		printf("Please, press PB0 to continue.\n");
		while(1){
			if(~(IORD(BUTTON_PIO_BASE,0)) & 1){ //if button is pressed we exit this loop (keep in mind that the buttons is active low so button would be 0 when pressed)
				break;
			}
		}

	 	// Register the ISR with the NIOS HAL (Hardware Abstraction Later) exception handling system
	 	// This tells the HAL which ISR to execute for a given hardware interrupt id
	 	alt_irq_register(STIMULUS_IN_IRQ, (void*)0, stimulus_detected_ISR); // Pass in the hardware interrupt number, null pointer, and ISR name
		//regiser the interupts
	 	IOWR(STIMULUS_IN_BASE, 2, 1); // Enable the stimulus interrupt

	 	int i = 0;
	 	for(i = 1; i <= 2500; i++){ //here since we want the period 2-5000 for 2 clock cycles

	 		int avg_latency = 0;
	 		int missed_pulses = 0;
	 		int multi_pulses = 0;
	 		int background_tasks_count = 0;

	 		// LED 1 signal pulse to indicate test start
	 		IOWR(LED_PIO_BASE, 0, 0x2); //turning on and off led 1 
	 		IOWR(LED_PIO_BASE, 0, 0x0);

	 		// Configure the EGM to send out the appropriate type of pulses
	 		int period = 2 * i;
	 		int pulse_width = i; //pulse with is period/2
	 		IOWR(EGM_BASE, 2, period); //2 represents the offset which is given in appendix 
	 		IOWR(EGM_BASE, 3, pulse_width);

	 		// Enable the EGM for each run of the test.
	 		IOWR(EGM_BASE, 0,1);

	 		// Continue the test as long as the EGM is busy
	 		while(IORD(EGM_BASE,1)) {
	 			background();
	 			background_tasks_count++; //keep track of the background tasks done 
	 		}
			//At this point the egm is no longer busy 
	 		// Retrieve data
	 		avg_latency = IORD(EGM_BASE, 4);
			missed_pulses = IORD(EGM_BASE, 5);
			multi_pulses = IORD(EGM_BASE, 6);

	 		// The EGM is no longer busy, meaning we have finished the test
	 		// Disable the EGM
	 		IOWR(EGM_BASE, 0, 0);

	 		// Output results
			printf("%d, %d, %d, %d, %d, %d \n", period, pulse_width, background_tasks_count, avg_latency, missed_pulses, multi_pulses);
	 	}
	 } else {
	 	////// APPROACH 2: TIGHT POLLING //////
		printf("Tight polling method selected.\n");
		printf("Period, Pulse_Width, BG_Tasks Run, Latency, Missed, Multiple\n\n");
		// Wait for PB[0] to be pressed
		printf("Please, press PB0 to continue.\n");
		while(1){
			if(~(IORD(BUTTON_PIO_BASE,0)) & 1){ //if button is pressed we exit this loop (keep in mind that the buttons is active low so button would be 0 when pressed)
				break;
			}
		}

		for(int i = 1; i <= 2500; i++){  //here since we want the period 2-5000 for 2 clock cycles, we are looping for each step (2,4,6)
			int background_tasks_count = 0;

	 		// LED 1 signal pulse to indicate test start
	 		IOWR(LED_PIO_BASE, 0, 0x2);
	 		IOWR(LED_PIO_BASE, 0, 0x0);

	 		// Configure the EGM to send out the appropriate type of pulses
	 		int period = 2 * i;
	 		int pulse_width = i; //pulse with is period/2
	 		IOWR(EGM_BASE, 2, period);
	 		IOWR(EGM_BASE, 3, pulse_width);

			// Create variables for characterization
	 		int max_background_tasks_per_cycle = 0;

	 		// Enable the EGM for each run of the test.
	 		IOWR(EGM_BASE, 0, 1);

	 		// FIRST CYCLE OF CHARACTERIZATION which is to see how many backgrounds tasks can fit within a period 
			// Wait for the stimulus signal to go high (And ensure EGM is enabled)
			while(!IORD(STIMULUS_IN_BASE,0)){}

			//At this point the stiumuls signal is high s
			// Pulse response because stimulus signal is high
			IOWR(RESPONSE_OUT_BASE, 0, 0x1);
			IOWR(RESPONSE_OUT_BASE, 0, 0x0);

			// Determine the number of background tasks that can fit in a period
			alt_u8 stimulus_signal = IORD(STIMULUS_IN_BASE, 0) & 1;

			// First rest of first half of first cycle after response
			while(stimulus_signal && IORD(EGM_BASE,1)){ //while the stimulus signal is high and egm  is busy
				stimulus_signal = IORD(STIMULUS_IN_BASE, 0) & 1;
				background(); //call to run the background task
				background_tasks_count++;
				max_background_tasks_per_cycle++; //max background tasks per cycle that can within a period 
			}

			// Second half of first cycle
			while(!stimulus_signal && IORD(EGM_BASE,1)){ //when the stimulus is low and the egm is still busy 
				stimulus_signal = IORD(STIMULUS_IN_BASE, 0) & 1;
				background(); //call to run the background task 
				background_tasks_count++; //total backgrounds tasks 
				max_background_tasks_per_cycle++;//max background tasks per cycle that can within a period 
			}


			int correct_backgrounds = max_background_tasks_per_cycle - 1; //The reason we do this is because if we look at the diagram the backgrounds tasks go over 1 cycle so we need to subtract


			// FOR THE REST OF THE CYCLES FOR THE TEST:
			while(IORD(EGM_BASE,1) & 1){ // While the EGM is busy (test is still running)
				// Pulse response since we are at rising edge of stimulus signal
				IOWR(RESPONSE_OUT_BASE, 0, 0x1);
				IOWR(RESPONSE_OUT_BASE, 0, 0x0);
				// Repeat the background task for the max number of times
				for(int i = 0; i < correct_backgrounds; i++){
					background();
					background_tasks_count++;
				}
				while(!(IORD(STIMULUS_IN_BASE,0) & 1) && (IORD(EGM_BASE,1) & 1)) {}; //if the stimulus is off and the egm base is on we dont want to do anything
				//we are doing bit & of 1 since we only care about that bit which will give us the information that we need 
			}

	 		// End of test. Output results
	 		int avg_latency = IORD(EGM_BASE, 4);
	 		int missed_pulses = IORD(EGM_BASE, 5);
	 		int multi_pulses = IORD(EGM_BASE, 6);
	 		// Disable the EGM at the end of each test
	 		IOWR(EGM_BASE, 0, 0x0);
			printf("%d, %d, %d, %d, %d, %d \n", period, pulse_width, background_tasks_count, avg_latency, missed_pulses, multi_pulses);

	 	}
	 }

	return 0;
}

