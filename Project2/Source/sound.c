#include <stdint.h>
#include <MKL25Z4.h>
#include <math.h>
#include <cmsis_os2.h>

#include "sound.h"
#include "misc.h"
#include "delay.h"
#include "gpio_defs.h"
#include "timers.h"
#include "DMA.h"
#include "threads.h"
#include "debug.h"

int16_t SineTable[NUM_STEPS];
uint16_t Waveform[2][NUM_WAVEFORM_SAMPLES];
uint8_t wr_nrd_buffer_num= 0; // Number of waveform buffer currently being written 

VOICE_T Voice[NUM_VOICES];

void DAC_Init(void) {
  // Init DAC output
	
	SIM->SCGC6 |= MASK(SIM_SCGC6_DAC0_SHIFT); 
	SIM->SCGC5 |= MASK(SIM_SCGC5_PORTE_SHIFT); 
	
	PORTE->PCR[DAC_POS] &= ~PORT_PCR_MUX_MASK;	
	PORTE->PCR[DAC_POS] |= PORT_PCR_MUX(0);	// Select analog 
		
	// Disable buffer mode
	DAC0->C1 = 0;
	DAC0->C2 = 0;
	
	// Enable DAC, select VDDA as reference voltage
	DAC0->C0 = MASK(DAC_C0_DACEN_SHIFT) | MASK(DAC_C0_DACRFS_SHIFT);
}

/*
	Code for driving DAC
*/
void Play_Sound_Sample(uint16_t val) {
	DAC0->DAT[0].DATH = DAC_DATH_DATA1(val >> 8);
	DAC0->DAT[0].DATL = DAC_DATL_DATA0(val);
}

void SineTable_Init(void) {
	unsigned n;
	
	for (n=0; n<NUM_STEPS; n++) {
		SineTable[n] = (MAX_DAC_CODE/2)*sinf(n*(2*3.1415927/NUM_STEPS));
	}
}

/* Fill waveform buffers with silence. */
void Init_Waveform(void) {
	uint32_t i;
	
	for (i=0; i<NUM_WAVEFORM_SAMPLES; i++) {
		Waveform[0][i] = (MAX_DAC_CODE/2);
		Waveform[1][i] = (MAX_DAC_CODE/2);
	}
}

void Init_Voices(void) {
	uint16_t i;
	
	for (i=0; i<NUM_VOICES; i++) {
		Voice[i].Volume = 0;
		Voice[i].Decay = 0;
		Voice[i].Duration = 0;
		Voice[i].Period = 0;
		Voice[i].Counter = 0;
		Voice[i].CounterIncrement = 0;
		Voice[i].Type = VW_UNINIT;
	}
}

/* Initialize sound hardware, sine table, and waveform buffer. */
void Sound_Init(void) {
	SineTable_Init();	
	Init_Waveform();
	Init_Voices();
	wr_nrd_buffer_num = 0; // Start writing to waveform buffer 0
	
	DAC_Init();
	DMA_Init();
	Configure_DMA_For_Playback(Waveform[0], Waveform[1], NUM_WAVEFORM_SAMPLES);
	TPM0_Init();
	Configure_TPM0_for_DMA(AUDIO_SAMPLE_PERIOD_US); 

	SIM->SOPT2 |= (SIM_SOPT2_TPMSRC(1) | SIM_SOPT2_PLLFLLSEL_MASK);

	SIM->SCGC5 |= (1UL << SIM_SCGC5_PORTE_SHIFT); 
	
	PORTE->PCR[AMP_ENABLE_POS] &= ~PORT_PCR_MUX_MASK;	
	PORTE->PCR[AMP_ENABLE_POS] |= PORT_PCR_MUX(1);	// Select GPIO
	PTE->PDDR |= MASK(AMP_ENABLE_POS); // set to output
	PTE->PSOR = MASK(AMP_ENABLE_POS);  // enable audio amp
}

void Sound_Enable_Amp(void) {
	PTE->PSOR = MASK(AMP_ENABLE_POS);  // enable audio amp
}

void Sound_Disable_Amp(void) {
	PTE->PCOR = MASK(AMP_ENABLE_POS);  // disable audio amp
}
/* Simple audio test function using busy-waiting. */
void Play_Tone(void) {
	int i, d=MAX_DAC_CODE>>7, n;
	int p[12] = {813, 1150, 813, 813, 813, 1150, 813, 813, 813, 813, 1150, 1150};
	int c[12] = {800, 283,  800, 800, 800, 283,  800, 800, 800, 800,  283, 283};
	
	for (i=0; i<4; i++) {
		n = c[i];
		while (n--) {
			Play_Sound_Sample((MAX_DAC_CODE>>1)+d);
			ShortDelay(p[i]);
			Play_Sound_Sample((MAX_DAC_CODE>>1)-d);
			ShortDelay(p[i]);
		}
		Delay(40);
	}
}

int16_t Sound_Generate_Next_Sample (VOICE_T *voice) {
	uint16_t lfsr;
	uint16_t bit;
	int16_t sample;

	switch (voice->Type) {
		case VW_NOISE:
			lfsr = voice->Counter;
			// source code from http://en.wikipedia.org/wiki/Linear_feedback_shift_register
			/* taps: 16 14 13 11; characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
			bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
			lfsr =  (lfsr >> 1) | (bit << 15);
			voice->Counter = lfsr;
			sample = (lfsr >> 4) - (MAX_DAC_CODE/2); // scale to get 12-bit value
			break;
		case VW_SQUARE:
			if (voice->Counter < voice->Period/2) {
				sample = MAX_DAC_CODE/2 - 1;
			} else {
				sample = -MAX_DAC_CODE/2;
			}
			voice->Counter++;
			if (voice->Counter == voice->Period) {
				voice->Counter = 0;
			}
			break;
		case VW_SINE:
			sample = SineTable[((voice->Counter)/256)]; // & (NUM_STEPS-1)]; 
			voice->Counter += voice->CounterIncrement;
			if (voice->Counter > voice->Period * voice->CounterIncrement){
				voice->Counter = 0;
			}
			break;
		default:
			sample = 0;
			break;
	}
	return sample;
}

void Play_Waveform_with_DMA(void) {
	Configure_DMA_For_Playback(Waveform[0], Waveform[1], NUM_WAVEFORM_SAMPLES);
	Start_DMA_Playback();
}

 void Thread_Sound_Manager(void * arg) {
	uint16_t lfsr=1234;
	uint16_t bit;
	uint8_t vn=0;

	Start_DMA_Playback();
	osDelay(100);
	 
	while (1) {
		// make a new random sound periodically
		Voice[vn].Volume = 0xA000; // 0xAFFF; 
		Voice[vn].Duration = 6000;
		bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
		lfsr =  (lfsr >> 1) | (bit << 15);
		Voice[vn].Period = FREQ_TO_PERIOD((lfsr & 0x03FF) + 40); 
		Voice[vn].Decay = 20; // 30;
		Voice[vn].Counter = 0; 
		Voice[vn].CounterIncrement = (NUM_STEPS*256)/Voice[0].Period; 
		Voice[vn].Type = VW_SINE;
		osDelay(200);
	}
}

 void Thread_Refill_Sound_Buffer(void * arg) {
	uint32_t i, flags;
	uint16_t v;
	int32_t sum, sample;
	 
	while (1) {
//		osThreadFlagsWait(EV_REFILL_SOUND_BUFFER, osFlagsWaitAny, osWaitForever); // wait for trigger
		flags = osThreadFlagsWait(EV_REFILL_SOUND_BUFFER | EV_UPDATE_SOUND_BUFFER, osFlagsWaitAny, osWaitForever); // wait for trigger
		if (flags & EV_REFILL_SOUND_BUFFER) {  // Overwrite wr buffer with new data
			for (i=0; i<NUM_WAVEFORM_SAMPLES; i++) {
				sum = 0;
				for (v=0; v<NUM_VOICES; v++) {
					if (Voice[v].Duration > 0) {
						sample = Sound_Generate_Next_Sample(&(Voice[v]));					
						sample = (sample*Voice[v].Volume)>>16;
						sum += sample;
						// update volume with decayed version
						Voice[v].Volume = (Voice[v].Volume * (((uint32_t) 65536) - Voice[v].Decay)) >> 16; 
						Voice[v].Duration--;
					} 
				}
				sum = sum + (MAX_DAC_CODE/2);
				sum = MIN(sum, MAX_DAC_CODE-1);
				Waveform[wr_nrd_buffer_num][i] = sum; 
			}
		} else if (flags & EV_UPDATE_SOUND_BUFFER) {
//			for (i=0; i<NUM_WAVEFORM_SAMPLES; i++) { // update rd buffer with new samples
//				sum = 0;
//				for (v=0; v<NUM_VOICES; v++) {
//					if (Voice[v].Duration > 0) {
//						sample = Sound_Generate_Next_Sample(&(Voice[v]));					
//						sample = (sample*Voice[v].Volume)>>16;
//						sum += sample;
//						// update volume with decayed version
//						Voice[v].Volume = (Voice[v].Volume * (((uint32_t) 65536) - Voice[v].Decay)) >> 16; 
//						Voice[v].Duration--;
//					} 
//				}
//				sum = sum + (MAX_DAC_CODE/2);
//				sum = MIN(sum, MAX_DAC_CODE-1);
//				Waveform[wr_nrd_buffer_num][i] = sum; 
		} else {
			// error
		}
	}
}

// Future expansion code below disabled
#if 0
const float startup_sound[] = {740, 880, 622.3, 740};
const float startup_chord[] = {659.3, 493.9, 329.6, 246.9};

 void Thread_Sequencer(void) {
	uint32_t p = 128;
	uint32_t n=0, v=0;
	
	while (1) {
		// Insert OS delay call here 
		if (n<4) {
			v = 0;
			Voice[v].Volume = 0xFFFF; 
			Voice[v].Duration = 10000;
			Voice[v].Period = FREQ_TO_PERIOD(startup_sound[n]); 
			Voice[v].Decay = 20;
			Voice[v].Counter = 0; 
			Voice[v].CounterIncrement = (NUM_STEPS*256)/Voice[v].Period; 
			Voice[v].Type = VW_SINE;

			Voice[1].Duration = 0;
			Voice[2].Duration = 0;
			Voice[3].Duration = 0;

			n++;
		} else if (n==4) {
			for (v=0; v<4; v++) {
				Voice[v].Volume = 0xE000; 
				Voice[v].Duration = 60000;
				Voice[v].Period = FREQ_TO_PERIOD(startup_chord[v]); 
				Voice[v].Decay = 5;
				Voice[v].Counter = 0; 
				Voice[v].CounterIncrement = (NUM_STEPS*256)/Voice[v].Period; 
				Voice[v].Type = VW_SINE;
			}
			n++;
		}
		os_evt_set(EV_REFILL_SOUND, t_Refill_Sound_Buffer);	
		Play_Waveform_with_DMA();
	}
}
#endif
