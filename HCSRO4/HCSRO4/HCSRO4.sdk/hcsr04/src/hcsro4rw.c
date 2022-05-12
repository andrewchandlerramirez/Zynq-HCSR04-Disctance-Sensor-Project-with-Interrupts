


#include "xparameters.h" //Accessing Definitions
#include "xgpio.h"		//Accessing GPIO driver functions
#include "xtmrctr.h"	//Accessing Xilinx Axi timer driver functions
#include "xscugic.h"	//Accessing Xilinx Interrupt gic driver functions
#include "xil_exception.h"
#include "xil_printf.h"

#include "hcsr04_ip.h" //Accessing HCSR04 driving functions

// Parameter definitions
#define INTC_DEVICE_ID 		XPAR_PS7_SCUGIC_0_DEVICE_ID //Interrupt device ID
#define TMR0_DEVICE_ID		XPAR_TMRCTR_0_DEVICE_ID //Timer 0 device ID
#define TMR1_DEVICE_ID		XPAR_TMRCTR_1_DEVICE_ID//Timer 1 device ID
#define BTNS_DEVICE_ID		XPAR_BUTTONS_DEVICE_ID //Buttons device ID
#define SWITCH_DEVICE_ID	XPAR_SWITCHES_DEVICE_ID //Switches Device ID
#define LEDS_DEVICE_ID		XPAR_LEDS_DEVICE_ID // LEDs device ID
#define INTC_GPIO_INTERRUPT_ID XPAR_FABRIC_BUTTONS_IP2INTC_IRPT_INTR //Button interrupt ID
#define INTC_TMR0_INTERRUPT_ID XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR //Timer 0 interrupt ID
#define INTC_TMR1_INTERRUPT_ID XPAR_FABRIC_AXI_TIMER_1_INTERRUPT_INTR //Timer 1 interrupt ID


#define BTN_INT 			XGPIO_IR_CH1_MASK //Btn channel 1 interrupt mask
#define TMR0_LOAD			0x05F5E100 //100_000_000 = 1 second
#define ONE_MICRO			0x00000064 //1 micro second
#define TMR1_LOAD			ONE_MICRO //Timer 1 will be loaded with 1 micro second


XGpio LEDInst, BTNInst, SWITCHInst;// Led, Button, and Switch gpio instance
XScuGic INTCInst; //Interrupt gic instance
XTmrCtr TMR0Inst,TMR1Inst; // Timer 0 and Timer 1 instance


static int led_data; //Leds written with led_data
static int btn_value;// button press will be read and control Leds
static int count_direction = 1; //will be used to control the direction of Led data count
static int timer0_toggle = 0; //will be used to turn on or off Timer 0


int prev_echo = 0, //will be used with echo for edge detection
		echo = 0, //echo
		echo_duration = 0, //will be the duration between positive and negative edge detection
		tmr1_count = 0, //will keep track of amount of time passing as the hander gets called
		distance = 0; //used linear regression time to distance


//----------------------------------------------------
// PROTOTYPE FUNCTIONS
//----------------------------------------------------
static void BTN_Intr_Handler(void *baseaddr_p);
static void TMR0_Intr_Handler(void *baseaddr_p);
static void TMR1_Intr_Handler(void *baseaddr_p);
static int InterruptSystemSetup(XScuGic *XScuGicInstancePtr);
static int IntcInitFunction(u16 DeviceId, XTmrCtr *TMR0InstancePtr, XTmrCtr *TMR1InstancePtr, XGpio *GpioInstancePtr);



//----------------------------------------------------
// MAIN FUNCTION
//----------------------------------------------------
int main (void)
{
	xil_printf("\r\nEntering main\r\n");

  int status;
  //----------------------------------------------------
  // INITIALIZE THE PERIPHERALS & SET DIRECTIONS OF GPIO
  //----------------------------------------------------
  // Initialise LEDs
  status = XGpio_Initialize(&LEDInst, LEDS_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;
  // Initialise switches
  status = XGpio_Initialize(&SWITCHInst, SWITCH_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;
  // Initialise Push Buttons
  status = XGpio_Initialize(&BTNInst, BTNS_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;
  // Set LEDs direction to outputs
  XGpio_SetDataDirection(&LEDInst, 1, 0x00);
  // Set SWITCHES direction to outputs
  XGpio_SetDataDirection(&SWITCHInst, 1, 0x00);
  // Set all buttons direction to inputs
  XGpio_SetDataDirection(&BTNInst, 1, 0xFF);


  //----------------------------------------------------
  // SETUP THE TIMER 0
  //----------------------------------------------------
  status = XTmrCtr_Initialize(&TMR0Inst, TMR0_DEVICE_ID); //Initialize timer 0
  if(status != XST_SUCCESS) return XST_FAILURE; //check for failure
  XTmrCtr_SetHandler(&TMR0Inst, TMR0_Intr_Handler, &TMR0Inst);//setup timer0 handler
  XTmrCtr_SetResetValue(&TMR0Inst, 0, TMR0_LOAD);// set reset value
  //set timer0 in interrupt mode, auto reload mode, and count down mode initially
  XTmrCtr_SetOptions(&TMR0Inst, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);


  	//----------------------------------------------------
    // SETUP THE TIMER 1
    //----------------------------------------------------
    status = XTmrCtr_Initialize(&TMR1Inst, TMR1_DEVICE_ID);//Initialize timer 1
    if(status != XST_SUCCESS) return XST_FAILURE;//check for failure
    XTmrCtr_SetHandler(&TMR1Inst, TMR1_Intr_Handler, &TMR1Inst);//setup timer1 handler
    XTmrCtr_SetResetValue(&TMR1Inst, 0, TMR1_LOAD);//set reset value
    //set timer1 in interrupt mode, auto reload mode, and count down mode initially
    XTmrCtr_SetOptions(&TMR1Inst, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);


  // Initialize interrupt controller
    //setup interrupt for Timer0, Timer1, and Buttons
  status = IntcInitFunction(INTC_DEVICE_ID, &TMR0Inst, &TMR1Inst, &BTNInst);
  if(status != XST_SUCCESS) return XST_FAILURE;//check for failure

  XTmrCtr_Start(&TMR0Inst, 0);//start timer 0
  XTmrCtr_Start(&TMR1Inst, 0);//start timer 1




  while(1);//inf loop

  return 0;
}

//----------------------------------------------------
// INTERRUPT HANDLER FUNCTIONS
// - called by the timer, button interrupt, performs
// - LED flashing
//----------------------------------------------------


void BTN_Intr_Handler(void *InstancePtr)
{
	// Disable GPIO interrupts
	XGpio_InterruptDisable(&BTNInst, BTN_INT);
	// Ignore additional button presses
	if ((XGpio_InterruptGetStatus(&BTNInst) & BTN_INT) !=
			BTN_INT) {
			return;
		}
	btn_value = XGpio_DiscreteRead(&BTNInst, 1);
	if(btn_value == 0x08){//BTN 3 pressed
		led_data = 0; //Leds start counting from 0 in current direction
	}
	else if(btn_value == 0x04){//BTN 2 pressed
			count_direction = 1;
			led_data = XGpio_DiscreteRead(&SWITCHInst, 1);//leds count up from current led_data
		}
	else if(btn_value == 0x02){//BTN 1 pressed
			count_direction = 0;
			led_data = XGpio_DiscreteRead(&SWITCHInst, 1);//leds count down from current led_data
		}
	else if(btn_value == 0x01){//BTN 0 pressed
		timer0_toggle = (timer0_toggle == 1)? 0: 1; //turn timer 0 on or off

		if(timer0_toggle == 1){//if timer toggle is on stop mode
			XTmrCtr_Stop(&TMR0Inst,0);//stop timer
		}
		else{
			XTmrCtr_Reset(&TMR0Inst,0);//reset timer
			XTmrCtr_Start(&TMR0Inst,0);//start timer
		}
	}

	XGpio_DiscreteWrite(&LEDInst, 1, led_data);//update leds with current count
    (void)XGpio_InterruptClear(&BTNInst, BTN_INT);//clear btn interrupt
    // Enable GPIO interrupts
    XGpio_InterruptEnable(&BTNInst, BTN_INT);
}

void TMR0_Intr_Handler(void *data)
{

	XTmrCtr_Stop(&TMR0Inst,0);//stop timer 0
	if(count_direction == 1){//if count direction indicates count up
		led_data++;//count up
	}
	else{//if count direction indicates count down
		led_data--;//count down
	}

	XGpio_DiscreteWrite(&LEDInst, 1, led_data);//update leds with current count
	XTmrCtr_Reset(&TMR0Inst,0);//reset timer 0
	XTmrCtr_Start(&TMR0Inst,0);//reset timer 1

	return;

}



void TMR1_Intr_Handler(void *data)
{

	XTmrCtr_Stop(&TMR1Inst,0);//stop timer1 while in the handler
	tmr1_count = (tmr1_count == 250000)? 0: tmr1_count; // should restart trigger ever quarter second

	if(tmr1_count < 50000){

		if(tmr1_count == 0){ //beginning of trigger
			HCSR04_IP_mWriteReg(XPAR_HCSR04_IP_0_S_AXI_BASEADDR, HCSR04_IP_S_AXI_SLV_REG3_OFFSET, 0x01);
		}
		if(tmr1_count == 10){//end of trigger
			HCSR04_IP_mWriteReg(XPAR_HCSR04_IP_0_S_AXI_BASEADDR, HCSR04_IP_S_AXI_SLV_REG3_OFFSET, 0x00);

		}
		//read echo
		echo = HCSR04_IP_mReadReg(XPAR_HCSR04_IP_0_S_AXI_BASEADDR, HCSR04_IP_S_AXI_SLV_REG0_OFFSET);


		if((prev_echo == 0x00) & (echo == 0x01)){ // pos edge detect
			echo_duration = tmr1_count;
		}
		if((prev_echo == 0x01) & (echo == 0x00)){ //neg edge detect
			echo_duration = tmr1_count - echo_duration;//duration between posedge and negedge
			distance = (2024*tmr1_count) - 326130; //distance converstion * 100000
			distance /= 100000; // dividing by 100_000 to get it into inches
			xil_printf("Distance:\t %d inches\r\n", distance);//print distance to terminal
		}


		prev_echo = echo; // save current echo into prev echo for edge detection

	}

	tmr1_count += 1; //increment timer1 counter


	XTmrCtr_Reset(&TMR1Inst,0);//reset timer1
	XTmrCtr_Start(&TMR1Inst,0);//start timer1

	return;
}

//----------------------------------------------------
// INITIAL SETUP FUNCTIONS
//----------------------------------------------------

int InterruptSystemSetup(XScuGic *XScuGicInstancePtr) //setup GIC
{
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 	 	 	 	 	 (Xil_ExceptionHandler)XScuGic_InterruptHandler,
			 	 	 	 	 	 XScuGicInstancePtr);
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

int IntcInitFunction(u16 DeviceId, XTmrCtr *TMR0InstancePtr, XTmrCtr *TMR1InstancePtr, XGpio *GpioInstancePtr)
{
	XScuGic_Config *IntcConfig;
	int status;
	// Interrupt controller initialisation
	IntcConfig = XScuGic_LookupConfig(DeviceId);
	status = XScuGic_CfgInitialize(&INTCInst, IntcConfig, IntcConfig->CpuBaseAddress);
	if(status != XST_SUCCESS) return XST_FAILURE;
	// Call to interrupt setup
	status = InterruptSystemSetup(&INTCInst);
	if(status != XST_SUCCESS) return XST_FAILURE;
	// Connect GPIO interrupt to handler
	status = XScuGic_Connect(&INTCInst,
					  	  	 INTC_GPIO_INTERRUPT_ID,
					  	  	 (Xil_ExceptionHandler)BTN_Intr_Handler,
					  	  	 (void *)GpioInstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;
	// Connect timer 0 interrupt to handler
	status = XScuGic_Connect(&INTCInst,
							 INTC_TMR0_INTERRUPT_ID,
							 (Xil_ExceptionHandler)TMR0_Intr_Handler,
							 (void *)TMR0InstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;
	// Connect timer 1 interrupt to handler
	status = XScuGic_Connect(&INTCInst,
							 INTC_TMR1_INTERRUPT_ID,
							 (Xil_ExceptionHandler)TMR1_Intr_Handler,
							 (void *)TMR1InstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;
	// Enable GPIO interrupts interrupt
	XGpio_InterruptEnable(GpioInstancePtr, 1);
	XGpio_InterruptGlobalEnable(GpioInstancePtr);
	// Enable GPIO and timer interrupts in the controller
	XScuGic_Enable(&INTCInst, INTC_GPIO_INTERRUPT_ID);
	XScuGic_Enable(&INTCInst, INTC_TMR0_INTERRUPT_ID);
	XScuGic_Enable(&INTCInst, INTC_TMR1_INTERRUPT_ID);
	return XST_SUCCESS;
}

