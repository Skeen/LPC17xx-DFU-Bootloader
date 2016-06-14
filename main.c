/*****************************************************************************
 *                                                                            *
 * DFU/SD/SDHC Bootloader for LPC17xx                                         *
 *                                                                            *
 * by Triffid Hunter                                                          *
 *                                                                            *
 *                                                                            *
 * This firmware is Copyright (C) 2009-2010 Michael Moon aka Triffid_Hunter   *
 *                                                                            *
 * This program is free software; you can redistribute it and/or modify       *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software                *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include "uart.h"

#include "gpio.h"

#include "sbl_config.h"

#include "min-printf.h"

#include "lpc17xx_wdt.h"

#if !(defined DEBUG)
#define printf(...) do {} while (0)
#endif

void setleds(int leds)
{
	GPIO_write(LED1, leds &  1);
	GPIO_write(LED2, leds &  2);
	GPIO_write(LED3, leds &  4);
	GPIO_write(LED4, leds &  8);
	GPIO_write(LED5, leds & 16);
}

// this seems to fix an issue with handoff after poweroff
// found here http://knowledgebase.nxp.trimm.net/showthread.php?t=2869
static void boot(uint32_t a)
{
	asm("LDR SP, [%0]" : : "r"(a));
	asm("LDR PC, [%0, #4]" : : "r"(a));
	// never returns
}

static uint32_t delay_loop(uint32_t count)
{
	volatile uint32_t j, del;
	for(j=0; j<count; ++j){
		del=j; // volatiles, so the compiler will not optimize the loop
	}
	return del;
}

static void new_execute_user_code(void)
{
	uint32_t addr=(uint32_t)USER_FLASH_START;
	// delay
	delay_loop(3000000);
	// relocate vector table
	SCB->VTOR = (addr & 0x1FFFFF80);
	// switch to RC generator
	LPC_SC->PLL0CON = 0x1; // disconnect PLL0
	LPC_SC->PLL0FEED = 0xAA;
	LPC_SC->PLL0FEED = 0x55;
	while (LPC_SC->PLL0STAT&(1<<25));
	LPC_SC->PLL0CON = 0x0;    // power down
	LPC_SC->PLL0FEED = 0xAA;
	LPC_SC->PLL0FEED = 0x55;
	while (LPC_SC->PLL0STAT&(1<<24));
	LPC_SC->FLASHCFG &= 0x0fff;  // This is the default flash read/write setting for IRC
	LPC_SC->FLASHCFG |= 0x5000;
	LPC_SC->CCLKCFG = 0x0;     //  Select the IRC as clk
	LPC_SC->CLKSRCSEL = 0x00;
	LPC_SC->SCS = 0x00;		    // not using XTAL anymore
	delay_loop(1000);
	// reset pipeline, sync bus and memory access
	__asm (
		   "dmb\n"
		   "dsb\n"
		   "isb\n"
		  );
	boot(addr);
}

int main()
{
	WDT_Feed();

	GPIO_init(LED1); GPIO_output(LED1);
	GPIO_init(LED2); GPIO_output(LED2);
	GPIO_init(LED3); GPIO_output(LED3);
	GPIO_init(LED4); GPIO_output(LED4);
	GPIO_init(LED5); GPIO_output(LED5);

	// turn off heater outputs
	GPIO_init(P2_4); GPIO_output(P2_4); GPIO_write(P2_4, 0);
	GPIO_init(P2_5); GPIO_output(P2_5); GPIO_write(P2_5, 0);
	GPIO_init(P2_6); GPIO_output(P2_6); GPIO_write(P2_6, 0);
	GPIO_init(P2_7); GPIO_output(P2_7); GPIO_write(P2_7, 0);

	setleds(31);

	UART_init(UART_RX, UART_TX, 2000000);
	printf("Bootloader Start\n");

#ifdef WATCHDOG
	WDT_Init(WDT_CLKSRC_IRC, WDT_MODE_RESET);
	WDT_Start(1<<22);
#endif

	// grab user code reset vector
#ifdef DEBUG
	unsigned *p = (unsigned *)(USER_FLASH_START +4);
	printf("Jumping to 0x%x\n", *p);
#endif

	while (UART_busy());
	printf("Jump!\n");
	while (UART_busy());
	UART_deinit();

	new_execute_user_code();

	UART_init(UART_RX, UART_TX, 2000000);

	printf("This should never happen\n");

	while (UART_busy());

	for (volatile int i = (1<<18);i;i--);

	NVIC_SystemReset();
}

void __aeabi_unwind_cpp_pr0(void){}
void __libc_init_array(void){}

int _write(int fd, const char *buf, int buflen)
{
	if (fd < 3)
	{
		while (UART_cansend() < buflen);
		return UART_send((const uint8_t *)buf, buflen);
	}
	return buflen;
}

void NMI_Handler() {
	printf("NMI\n");
	for (;;);
}
void HardFault_Handler() {
	printf("HardFault\n");
	for (;;);
}
void MemManage_Handler() {
	printf("MemManage\n");
	for (;;);
}
void BusFault_Handler() {
	printf("BusFault\n");
	for (;;);
}
void UsageFault_Handler() {
	printf("UsageFault\n");
	for (;;);
}
