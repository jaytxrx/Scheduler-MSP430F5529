/* Copyright (c) 2014, Jeffrey Antony     */
/* Read license file for more information */

#include <msp430.h>
#include <stdio.h>

#define BACKUP_REGS 12 /* r4 to r15*/

#define STACK_SIZE  1024
#define STACK_TOP   STACK_SIZE - 1   
#define TOTAL_TASKS 3

/*Enable GIE in SR so that the WDT never stops when we go to user task*/
/*Enable SCG0 for 25MHZ CPU execution*/
#define DEFAULT_SR  ((uint16_t)0x0048) 

#define SAVE_CONTEXT()           \
  asm volatile ( "push r4  \n\t" \
                 "push r5  \n\t" \
                 "push r6  \n\t" \
                 "push r7  \n\t" \
                 "push r8  \n\t" \
                 "push r9  \n\t" \
                 "push r10 \n\t" \
                 "push r11 \n\t" \
                 "push r12 \n\t" \
                 "push r13 \n\t" \
                 "push r14 \n\t" \
                 "push r15 \n\t" \
               );

#define RESTORE_CONTEXT()       \
  asm volatile ( "pop r15 \n\t" \
                 "pop r14 \n\t" \
                 "pop r13 \n\t" \
                 "pop r12 \n\t" \
                 "pop r11 \n\t" \
                 "pop r10 \n\t" \
                 "pop r9  \n\t" \
                 "pop r8  \n\t" \
                 "pop r7  \n\t" \
                 "pop r6  \n\t" \
                 "pop r5  \n\t" \
                 "pop r4  \n\t" \
                 "reti    \n\t" \
               );


/*stack for each task - 1024*16 = 2KB RAM*/
uint16_t task1ram[STACK_SIZE];
uint16_t task2ram[STACK_SIZE];
uint16_t task3ram[STACK_SIZE];

volatile uint8_t  task_id; /*has the current running task*/
volatile uint16_t *stack_pointer[TOTAL_TASKS]; /*address of stack pointer for each task*/

/*****************************************************/
volatile uint8_t button1 = 0x1, button2=0x1; /*volatile since its a shared resource between tasks*/
/*****************************************************/

void task1(void)
{ 
  volatile unsigned int i;

  P1DIR |= BIT0; /*for LED P1.0*/

  while(1)
  {
    if(button1)
    {
      for (i=0; i < 65535; i++);
      for (i=0; i < 65535; i++);
      for (i=0; i < 65535; i++);

      P1OUT ^= BIT0;    // Toggle P1.0 (LED)
    }
  }
}

void task2(void)
{
  volatile unsigned int i;

  P4DIR |= BIT7; /*for LED P4.7*/

  while(1)
  {
    if(button2)
    {
      for (i=0; i < 65535; i++);
   
      P4OUT ^= BIT7;
    }
  }
}

void task3(void)
{
  /*get both button states and exchange data */

  uint8_t b1,b2;
  volatile unsigned int i;
  
  P2REN |= BIT1;
  P2OUT |= BIT1;

  P1REN |= BIT1;
  P1OUT |= BIT1;

  while(1)
  {

    b1 = (P2IN & BIT1);
    b2 = (P1IN & BIT1);
    
    if(b1!=BIT1)
    {
      button1 ^= 0x1;
    }
    
    if(b2!=BIT1)
    {
      button2 ^= 0x1;
    }

    for (i=0; i < 20000; i++); //software debouncing

  }
}


/*****************************************************/

/*
 * This function will initialise stack for each task. Following are filled into the stack
 * 1) Store the PC first since it has to be poped back and loaded at end 
 * 2) then store SR register. This determines the CPU condition at which your task should execute
 * 3) leave the space for the registers r4 to r15 since the scheduler will pop these when a task switching occurs
 * 3) return the address of stack which will contain all informations for the task to execute after task switching
 * 4) TODO: fill the end of stack with known values to detect buffer overflows.
 */
uint16_t *initialise_stack(void (* func)(void), uint16_t *stack_location)
{
  uint8_t i;
  
  /*MSP430F5529 has a 20bit PC register*/
  *stack_location = (uint16_t)func; //last 16 bits will only stored. Pending 4bits will be stored with SR
  stack_location--;
  /*refer datasheet to see how 20bit PC is stored along with SR*/
  *stack_location = (((uint16_t)((uint32_t)(0xf0000 & (uint32_t)func) >> 4))| DEFAULT_SR); //TODO:fix compiler warning
  
  /*leave space in stack for r4 to r15*/
  for(i= 0; i< BACKUP_REGS; i++)
  {
    stack_location--;
  }
  
  /*TODO: fill bottom of stack to detect buffer overflow*/
  
  return stack_location;
}

void SetVcoreUp (unsigned int level)
{
  // Open PMM registers for write
  PMMCTL0_H = PMMPW_H;              
  // Set SVS/SVM high side new level
  SVSMHCTL = SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level;
  // Set SVM low side to new level
  SVSMLCTL = SVSLE + SVMLE + SVSMLRRL0 * level;
  // Wait till SVM is settled
  while ((PMMIFG & SVSMLDLYIFG) == 0);
  // Clear already set flags
  PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);
  // Set VCore to new level
  PMMCTL0_L = PMMCOREV0 * level;
  // Wait till new level reached
  if ((PMMIFG & SVMLIFG))
    while ((PMMIFG & SVMLVLRIFG) == 0);
  // Set SVS/SVM low side to new level
  SVSMLCTL = SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level;
  // Lock PMM registers for write access
  PMMCTL0_H = 0x00;
}

void InitClock(void)
{
  UCSCTL3 = SELREF_2;                       // Set DCO FLL reference = REFO
  UCSCTL4 |= SELA_2;                        // Set ACLK = REFO

  __bis_SR_register(SCG0);                  // Disable the FLL control loop
  UCSCTL0 = 0x0000;                         // Set lowest possible DCOx, MODx
  UCSCTL1 = DCORSEL_7;                      // Select DCO range 50MHz operation
  UCSCTL2 = FLLD_0 + 762;                   // Set DCO Multiplier for 25MHz
                                            // (N + 1) * FLLRef = Fdco
                                            // (762 + 1) * 32768 = 25MHz
                                            // Set FLL Div = fDCOCLK/2
  __bic_SR_register(SCG0);                  // Enable the FLL control loop

  // Worst-case settling time for the DCO when the DCO range bits have been
  // changed is n x 32 x 32 x f_MCLK / f_FLL_reference. See UCS chapter in 5xx
  // UG for optimization.
  // 32 x 32 x 25 MHz / 32,768 Hz ~ 780k MCLK cycles for DCO to settle
  __delay_cycles(782000);

  // Loop until XT1,XT2 & DCO stabilizes - In this case only DCO has to stabilize
  do
  {
    UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG);
                                            // Clear XT2,XT1,DCO fault flags
    SFRIFG1 &= ~OFIFG;                      // Clear fault flags
  }while (SFRIFG1&OFIFG);                   // Test oscillator fault flag
}
  
volatile uint16_t *temp;

void main(void)
{
  //Stop the watchdog timer until we configure our scheduler
  WDTCTL = WDTPW + WDTHOLD;
 
  // Increase Vcore setting to level3 to support fsystem=25MHz
  // NOTE: Change core voltage one level at a time..
  SetVcoreUp (0x01);
  SetVcoreUp (0x02);
  SetVcoreUp (0x03);

  InitClock();
  
  /*initialise stack for each task*/
  stack_pointer[0] = initialise_stack(task1, &task1ram[STACK_TOP]);
  stack_pointer[1] = initialise_stack(task2, &task2ram[STACK_TOP]);
  stack_pointer[2] = initialise_stack(task3, &task3ram[STACK_TOP]);
  
  //configure WDT for cyclically calling the scheduler
  //WDTCTL = WDT_MDLY_32;                     // WDT 32ms, SMCLK, interval timer
  WDTCTL = WDTPW + WDTSSEL1 +WDTTMSEL + WDTCNTCL + WDTIS_7;
  SFRIE1 |= WDTIE;                          // Enable WDT interrupt

  // WDT won't work until we enable GIE in SR. But this is enabled via our SR's #define
  // when the first task is poped out, automatically GEI is enabled

  /*initialise to first task*/
  task_id =0;
  
  //set the stack pointer to task1's RAM location
  temp = stack_pointer[task_id];
  asm volatile ("mov.w temp, r1 \n\t");
  
  // lets pop our first task out!  
  RESTORE_CONTEXT();
}

__attribute__( ( __interrupt__( WDT_VECTOR ), __naked__ ) )
void scheduler(void)
{

  SAVE_CONTEXT(); /*save current registers into stack. PC and SR will be saved automatically when an interrupt occurs*/
  
  //if required, we can stop the WDT here until we are done with all the works for context switching.
  //then start the WDT before restoring the context (this step is not required since our SR is by default is enabled with global interrupts)
  //we can do a stack overflow check here
  
  /*backup the stack pointer*/
  asm volatile ("mov.w r1, temp \n\t");
  stack_pointer[task_id] = temp;
  
  //round robin scheduling
  if(task_id < (TOTAL_TASKS-1))
  {
    task_id++;
  }
  else
  {
    task_id = 0;
  }

  /*check for stack overflow*/
  
  temp = stack_pointer[task_id];
  asm (  "mov.w  temp, r1    \n\t");

  RESTORE_CONTEXT();
  
}
