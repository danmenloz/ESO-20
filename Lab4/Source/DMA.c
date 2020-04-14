#include <stdint.h>
#include <MKL25Z4.h>
#include <cmsis_os2.h>

#include "timers.h"
#include "LEDs.h"
#include "threads.h"
#include "gpio_defs.h"
#include "debug.h"
#include "sound.h"

uint16_t * Reload_DMA_Source[2]={0,0};
uint32_t Reload_DMA_Byte_Count=0;

void DMA_Init(void) {
	SIM->SCGC7 |= SIM_SCGC7_DMA_MASK;
	SIM->SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
}

void Configure_DMA_For_Playback(uint16_t * source1, uint16_t * source2, uint32_t count) {
	// Disable DMA channel in order to allow changes
	DMAMUX0->CHCFG[0] = 0;

	Reload_DMA_Source[0] = source1;
	Reload_DMA_Source[1] = source2;

	Reload_DMA_Byte_Count = count*2;
	
	// Generate DMA interrupt when done
	// Increment source, transfer words (16 bits)
	// Enable peripheral request
	DMA0->DMA[0].DCR = DMA_DCR_EINT_MASK | DMA_DCR_SINC_MASK | 
											DMA_DCR_SSIZE(2) | DMA_DCR_DSIZE(2) |
											DMA_DCR_ERQ_MASK | DMA_DCR_CS_MASK;
											// DMA_DCR_D_REQ_MASK;  // DON'T disable peripheral request when done

	
	// Configure NVIC for DMA ISR
	NVIC_SetPriority(DMA0_IRQn, 128); // 0, 64, 128 or 192
	NVIC_ClearPendingIRQ(DMA0_IRQn); 
	NVIC_EnableIRQ(DMA0_IRQn);	

	// Select TPM0 overflow as trigger via mux
	// Don't use periodic triggering
	DMAMUX0->CHCFG[0] = DMAMUX_CHCFG_SOURCE(54);   
}

void Start_DMA_Playback() {
	// initialize source and destination pointers
#if USE_DOUBLE_BUFFER
	DMA0->DMA[0].SAR = DMA_SAR_SAR((uint32_t) Reload_DMA_Source[1 - wr_nrd_buffer_num]);
#else
	DMA0->DMA[0].SAR = DMA_SAR_SAR((uint32_t) Reload_DMA_Source[0]);
#endif
	DMA0->DMA[0].DAR = DMA_DAR_DAR((uint32_t) (&(DAC0->DAT[0])));
	
	// byte count
	DMA0->DMA[0].DSR_BCR = DMA_DSR_BCR_BCR(Reload_DMA_Byte_Count);
	
	// Enable DMA
	DMAMUX0->CHCFG[0] |= DMAMUX_CHCFG_ENBL_MASK;

	// ensure the timer is running 
	TPM0_Start();
}

void Restart_DMA_Playback() {
	// initialize source and destination pointers
#if USE_DOUBLE_BUFFER
	DMA0->DMA[0].SAR = DMA_SAR_SAR((uint32_t) Reload_DMA_Source[1 - wr_nrd_buffer_num]);
#else
	DMA0->DMA[0].SAR = DMA_SAR_SAR((uint32_t) Reload_DMA_Source[0]);
#endif

	// byte count
	DMA0->DMA[0].DSR_BCR = DMA_DSR_BCR_BCR(Reload_DMA_Byte_Count);
	
	// Enable DMA -- TODO confirm is needed
	DMAMUX0->CHCFG[0] |= DMAMUX_CHCFG_ENBL_MASK;
}


void DMA0_IRQHandler(void) {
	// Clear done flag 
	DMA0->DMA[0].DSR_BCR = DMA_DSR_BCR_DONE_MASK; 
#if USE_DOUBLE_BUFFER		
	// switch to other buffer
	wr_nrd_buffer_num = 1 - wr_nrd_buffer_num;
#endif
	// Signal event requesting source buffer refill
	osThreadFlagsSet(t_Refill_Sound_Buffer, EV_REFILL_SOUND_BUFFER);
	// Start playback again
	Restart_DMA_Playback();
}
// *******************************ARM University Program Copyright © ARM Ltd 2013*************************************   
