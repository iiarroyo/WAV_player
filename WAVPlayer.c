#include <io.h>
#include <delay.h>
#include <stdio.h>
#include <ff.h>
#include <string.h>
#include <display.h>

#asm
    .equ __lcd_port=0x02
    .equ __lcd_EN=1
    .equ __lcd_RS=0
    .equ __lcd_D4=2
    .equ __lcd_D5=3
    .equ __lcd_D6=4
    .equ __lcd_D7=5
#endasm

//C�digo base que reproduce A001.WAV que es un WAV, Mono, 8-bit, y frec de muestreo de 22050HZ
    
char bufferL[256];
char bufferH[256];
char NombreArchivo[]  = "0:A001.wav";
char song = '1';
bit stereo = 0;
unsigned int i=0;
unsigned int aux=0;
bit LeerBufferH,LeerBufferL;
unsigned long muestras;
char* null_char;
bit pausa = 0;
unsigned char Back[] = {0x00,0x00,0x05,0x0F,0x1F,0x0F,0x05,0x00};
unsigned char Play[] = {0x08,0x0C,0x0E,0x0F,0x0F,0x0E,0x0C,0x08};
unsigned char Pause[] = {0x00,0x1B,0x1B,0x1B,0x1B,0x1B,0x1B,0x00};
unsigned char Forward[] = {0x00,0x00,0x14,0x1E,0x1F,0x1E,0x14,0x00};
unsigned char itr = 0;
char impresion[100];
char auxImpresion[] = "                ";
char pantalla[16];
char espacio[] = " - ";
unsigned char j = 0;
unsigned int aux2 = 0;

void scroll(){    
    
    MoveCursor(0,0);   
    memcpy(pantalla,impresion+j,16);     
    StringLCDVar(pantalla);   
    j++;     
    if(j == (aux+aux2+5))  
        j = 0;    
}

interrupt [TIM1_COMPA] void timer1_compa_isr(void)
{
    disk_timerproc();
    if(itr<10)
        scroll();
    else
        itr=0;    
    itr++;

/* MMC/SD/SD HC card access low level timing function */
}
        
//Interrupci�n que se ejecuta cada T=1/Fmuestreo_Wav
interrupt [TIM2_COMPA] void timer2_compa_isr(void)         
{
    if(stereo == 0){
        if (i<256)
          OCR0A=bufferL[i++];
        else{
          OCR0A=bufferH[i-256];
          i++;
        }   
        if (i==256)
           LeerBufferL=1;
        if (i==512){
           LeerBufferH=1;
           i=0;
        }
    }
    
    else{
        if (i<256){
          OCR0A=bufferL[i++];
          OCR0B=bufferL[i++];
        }
        else{
          OCR0A=bufferH[i-256]; 
          i++;
          OCR0B=bufferH[i-256];
          i++;
        }   
        if (i==256)
           LeerBufferL=1;
        if (i==512){
           LeerBufferH=1;
           i=0;
        }
    }   
}

void main()
{
    unsigned int  br;
       
    /* FAT function result */
    FRESULT res;
                  
   
    /* will hold the information for logical drive 0: */
    FATFS drive;
    FIL archivo; // file objects 
                                  
    CLKPR=0x80;         
    CLKPR=0x01;         //Cambiar a 8MHz la frecuencia de operaci�n del micro 
       
    // C�digo para hacer una interrupci�n peri�dica cada 10ms
    // Timer/Counter 1 initialization
    // Clock source: System Clock
    // Clock value: 1000.000 kHz
    // Mode: CTC top=OCR1A
    // Compare A Match Interrupt: On
    TCCR1B=0x0A;     //CK/8 10ms con oscilador de 8MHz
    OCR1AH=0x27;
    OCR1AL=0x10;
    TIMSK1=0x02; 
                                                    
    //PWM para conversi�n Digital Anal�gica WAV->Sonido
    // Timer/Counter 0 initialization
    // Clock source: System Clock
    // Clock value: 8000.000 kHz
    // Mode: Fast PWM top=0xFF
    // OC0A output: Non-Inverted PWM
    TCCR0A=0x83;       
    
    DDRB.7=1;  //Salida bocina (OC0A) 
    DDRG.5=1;  //Salida bocina (OC0B) 
    
    // Timer/Counter 2 initialization
    // Clock source: System Clock
    // Clock value: 1000.000 kHz
    // Mode: CTC top=OCR2A
    ASSR=0x00;
    TCCR2A=0x02;
    TCCR2B=0x02;
    OCR2A=0x2C;
        
    // Timer/Counter 2 Interrupt(s) initialization
    TIMSK2=0x02; 
    
    //botones
    PORTD = 0x07;
    
    DDRD.7=1;
    #asm("sei")   
    disk_initialize(0);  /* Inicia el puerto SPI para la SD */
    delay_ms(500);
       
    SetupLCD();
    CreateChar(0,Back);
    CreateChar(1,Play);
    CreateChar(2,Pause);
    CreateChar(3,Forward);
                                
    MoveCursor(7,1);
    CharLCD(2);
    MoveCursor(5,1);
    CharLCD(0);
    MoveCursor(9,1);
    CharLCD(3);    
    
    /* mount logical drive 0: */
    if ((res=f_mount(0,&drive))==FR_OK){  
        while(1)
        { 
            
        /*Lectura de Archivo*/
        res = f_open(&archivo, NombreArchivo, FA_OPEN_EXISTING | FA_READ);
        if (res==FR_OK){ 
            PORTD.7=1;
            f_read(&archivo, bufferL, 44,&br); //leer encabezado 
            
            //Canal
            switch(bufferL[22])
            {
                case 0x01: // mono
                    TCCR0A=0x83;
                    stereo=0;
                    break;
                case 0x02: //stereo
                    TCCR0A=0xA3;
                    stereo=1;
                    break;       
            }
            
            //frecuencias
             switch(bufferL[24])
             {
                case 0x22: // 22050
                    OCR2A = 0x2C;
                    break;
                case 0xC0: //24000
                    OCR2A =  0x29;
                    break;
                case 0x80: // 16000
                    OCR2A = 0x3E;
                    break;       
             }
               
            muestras = (long)bufferL[43]*16777216 + (long)bufferL[42]*65536 + (long)bufferL[41]*256 + bufferL[40];
            f_lseek(&archivo,muestras+44 + 20);
            f_read(&archivo,bufferH,100,&br);
            f_lseek(&archivo,44);
            
            null_char = strchr(bufferH,'\0');
            aux = null_char-bufferH;
            memcpy(impresion,bufferH,aux);
            memmove(bufferH, bufferH + aux+1+8,100); 
            
            memcpy(impresion+aux,espacio,3);
                        
            null_char = strchr(bufferH,'\0');
            aux2 = null_char-bufferH;
            memcpy(impresion+aux+3, bufferH, aux2);             
            memcpy(impresion+aux+aux2+3, auxImpresion, 16);  
            
            f_read(&archivo, bufferL, 256,&br); //leer los primeros 512 bytes del WAV
            f_read(&archivo, bufferH, 256,&br);    
            LeerBufferL=0;     
            LeerBufferH=0;
            TCCR0B=0x01;    //Prende sonido
            do{   
                 DDRB.7=1; 
                 
                 while((LeerBufferH==0)&&(LeerBufferL==0));
                 if (LeerBufferL)
                 {                       
                     f_read(&archivo, bufferL, 256,&br); //leer encabezado
                     LeerBufferL=0;  
                 }
                 else
                 { 
                     f_read(&archivo, bufferH, 256,&br); //leer encabezado
                     LeerBufferH=0;
                    
                 }            
                 
                 if(PIND.0 == 0)
                 {
                    pausa = ~pausa;
                    TCCR2B=0x00;
                    MoveCursor(7,1);
                    CharLCD(1);
                    delay_ms(200);
                    do{
                    }while(PIND.0==1);
                    delay_ms(200);
                    TCCR2B=0x02;
                 }
                 
                 if(PIND.1 == 0) //siguiente
                 {
                    DDRB.7=0;
                    delay_ms(200);
                    break;
                 }
                 
                 if(PIND.2 == 0) //regreso
                 {
                    DDRB.7=0;
                    song--;
                    break;
                 }
                 
                 //C�digo para estatus switches
                          
            }while(br==256);
            i=0;
            TCCR0B=0x00;   //Apaga sonido
            song++;
            if(song == '7')
                song = '1';
            NombreArchivo[5] = song;
            EraseLCD();
            f_close(&archivo); 
            
        }              
        }
    }
    f_mount(0, 0); //Cerrar drive de SD
    while(1);
}