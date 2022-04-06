/*
 * Originally posted on October 27, 2015: 
 * https://diyrecorder.wordpress.com/2015/10/27/using-tis-fir-indexing-hrir-issues-sw2-is-messed-up/
 *
 * fir_filter.c
 * A crude implementation for using HRIRs. A 200 tap HRIR was loaded for each channel.
 *      Author: GSI
 */
 
#include <usbstk5515.h>
#include <usbstk5515_i2c.h>
#include <AIC_func.h>
#include <sar.h>
#include <stdio.h>
#include <Dsplib.h>
#include "hrir_l_subject3_el10_azAll.h"
#include "hrir_r_subject3_el10_azAll.h"
 
#define ASIZE       200
 
#define TCR0        *((ioport volatile Uint16 *)0x1810)
#define TIMCNT1_0   *((ioport volatile Uint16 *)0x1814)
#define TIME_START  0x8001
#define TIME_STOP   0x8000
 
#define LED_out1 *((ioport volatile Uint16*) 0x1C0A )
#define LED_out2 *((ioport volatile Uint16*) 0x1C0B )
#define LED_dir1 *((ioport volatile Uint16*) 0x1C06 )
#define LED_dir2 *((ioport volatile Uint16*) 0x10C7 )
 
//initializing LED GPIOs as outputs. LEDs are active low => writing 1s to keep them off
void LED_init() {
    LED_dir1 |= (Uint16) (3 << 14); //GPIOs 14, 15
    LED_dir2 |= (Uint16) 3;         //GPIOs 16, 17
    //switching them off (driving high)
    LED_out1 |= (Uint16) (3 << 14);
    LED_out2 |= (Uint16) 3;
}
 
//LED toggling based on index inputted
void toggle_LED(int index) {
    if      (index == 3){   //Blue:                     GPIO[14]
        LED_out1 = (Uint16)(3<< (14) );
        LED_out2 = (Uint16)(1<< (0) );
    }
    else if(index == 2) {   //Yellow(ish) <-- LOL:       GPIO[15]
        LED_out1 = (Uint16)(3<< (14) );
        LED_out2 = (Uint16)(2<< (0) );
    }
    else if(index == 1) {   //Red:                      GPIO[16]
        LED_out1 = (Uint16)(1<< (14) );
        LED_out2 = (Uint16)(3<< (0) );
    }
    else if(index == 0) {   //Green:                    GPIO[17]
        LED_out1 = (Uint16)(2<< (14) );
        LED_out2 = (Uint16)(3<< (0) );
    }
    else {  //Green:                    GPIO[17]
        LED_out1 = (Uint16)(3<< (14) );
        LED_out2 = (Uint16)(3<< (0) );
    }
}
 
Int16 in_left[ASIZE], in_right[ASIZE];
Uint16 delta_time;
 
Int16 FIR(Int16* in, Int16* hrir,  Uint16 i, Uint16 az)
{
    Int32 sum;
    Uint16 j;
    Uint32 index;
    sum=0;
    //The actual filter work
    for(j=0; j<ASIZE; j++) { if(i>=j)
            index = i - j;
        else
            index = ASIZE + i - j;
        sum += (Int32)in[index] * (Int32)hrir[az*ASIZE + j];
    }
    sum = sum + 0x00004000;         // So we round rather than truncate.
    return (Int16) (sum >> 15);   // Conversion from 32 Q30 to 16 Q15.
}
 
void main(void)
{
    LED_init();
    Uint16 i, n, az = 0;
//  Uint16 start_time;
//  Uint16 end_time;
    Int16 right[1], left[1]; //AIC inputs
    Int16 out_left[1], out_right[1];
    Int16 dbuffer_l[ASIZE+2] = {0};
    Int16 dbuffer_r[ASIZE+2] = {0};
    Int16 hrir_left[ASIZE], hrir_right[ASIZE];
    Int16 scaling = 16;
    Uint16 key;
    Uint16 filter = 3;
 
    USBSTK5515_init();  //Initializing the Processor
    AIC_init();         //Initializing the Audio Codec
    Init_SAR();         //Initializing the SAR ADC for key presses
    //Priming the PUMP
    for(i = 0; i < (ASIZE); i++) { AIC_read2(right, left); in_right[i] = right[0]; in_left[i] = left[0]; dbuffer_r[i] = right[0]; dbuffer_l[i] = left[0]; } while(1) { if(i>=ASIZE) i=0;
        //if(j>=WTIME){
        //  if(key == SW2) az += 1;
        //  j = 0;
        //}
        AIC_read2(right, left);
        in_left[i] = left[0];
        in_right[i] = right[0];
 
        if (filter == 0){ //bypass
            out_left[0]     = left[0];
            out_right[0]    = right[0];
        }
        else if (filter == 1){ //redundant calculation (slow down)
            //scaling*FIR(in_left, hrir_l, i, az);
            //scaling*FIR(in_right, hrir_r, i, az);
            out_left[0]     = left[0]-right[0];
            out_right[0]    = right[0]-left[0];
        }
        else if (filter == 2){
            fir(in_left,
                hrir_left,
                out_left,
                dbuffer_l,
                1,
                ASIZE);
            fir(in_right,
                hrir_right,
                out_right,
                dbuffer_r,
                1,
                ASIZE);
            out_left[0]     = left[0];
            out_right[0]    = right[0];
            //using the HRIR output
            //out_left[0]   = scaling*FIR(in_left, hrir_l, i, az);
            //out_right[0]  = scaling*FIR(in_right, hrir_r, i, az);
 
        }
        else if (filter == 3){
            for (n=0; n<ASIZE; n++){ hrir_left[n] = hrir_l[az*ASIZE +n]; hrir_right[n] = hrir_r[az*ASIZE +n]; } fir(left, hrir_left, out_left, dbuffer_l, 1, ASIZE); fir(right, hrir_right, out_right, dbuffer_r, 1, ASIZE); out_left[0] = scaling*out_left[0]; out_right[0] = scaling*out_right[0]; //mono summed (using left channel) //out_left = scaling*FIR(in_left, hrir_l, i, az); //out_right = scaling*FIR(in_left, hrir_r, i, az); } else{ out_left[0] = 0; out_right[0] = 0; } //POSTFILTER: AIC_write2(out_right[0], out_left[0]); toggle_LED(az%4);//filter); key = Get_Key_Human(); //if (key == SW1) filter += 1; if (key == SW1) az += 1; if (filter>4) filter = 0;
        if (az > 47) az = 0; //az = 48, 49, 50 scaryyy
        i++;
 
        //printf("%i\n", az);
    }
}