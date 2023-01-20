/** 
 * @file arcade-ExtraScreen.ino
 * @author your name (you@domain.com)
 * @brief 
 * worked well at 27fps(depends on arcade)
 * tested on both my st7735s red/blue pcb, 6~7ms/frame
 * 
 * [Arcade side] :
 *  SPIDmaSlave, VSPI (CS:  5, CLK: 18, MOSI: 23, MISO: 19)
 *  arcade(master) -> esp32(slave) worked, HSPI/VSPI & mode0 & batch transfer, MOSI<->MOSI ...
 *  only 19249-byte package. Each frame include Screen Pixels + Pallete + Brightness, calc at esp side.
 *  Stat msg printed on screen at arcade side. 
 *  NOTE:
    * arcade send image data in tranposed order
    * remove 0-size packets: arcade side don't switch CS when tranfer, just set CS low once at start.

 * [TFT side]:
   // HSPI: 40Mhz
    TFT_RST 26
    TFT_CS  -1, wired at EN pin
    TFT_DC  27
    SCK     14
    MOSI    13
    MISO    12  not connect
 *  120*160= 21ms/frame @27Mhz; 240*320= 17ms @40Mhz+DMA
 *  [some perf tuning]:
 *  -8ms, tft.endWrite() wait prev DMA transfer finish, so do it at next loop, can take advantage of DMA.
 *  -4ms, tft.setSwapBytes(false);    //use false with swaped colors,avoid swap everytime 
 *  TFT 2-segments DMA buffer of 240*320. Avoid 2 setWindow() vs 4-segments.
 * 
 * [issue]
 * Display abnormal for seconds at starting sometime.
      SPI Slave rx didn't sync up with arcade package. Work around: add a magic number check.
 * Stat Update need a printf after Dma transfer, or screen edge will tremble. IDK Y? 
      cause: arcade send too fast to deal&send frame to extraScreen by esp.
      solution: scale down framerate. temply, done at Arcade side.
 * Screen flicker at low fps, eg: 3D with Raycasting
 * 
 * @todo 
 * resolve Status text blink
      done. by custom drawChar_rvX(), write into DMA buf before transfer
 * auto scale down refresh rate to about 31 fps, for botleneck of extra screen bandwidth，40M/(240*320*2*8)=32.55fps @SPI40Mhz
 * 
 * @version 0.1
 * @date 2023-01-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

//generic const
#define ARCADE_WIDTH  120
#define ARCADE_HEIGHT  160

//package
#define PACKAGE_DEVIDER 4
#define BUFFER_SIZE_SCREEN   ARCADE_WIDTH*ARCADE_HEIGHT /PACKAGE_DEVIDER
#define BUFFER_SIZE  (BUFFER_SIZE_SCREEN + 4) //align to 4-byte
#define BUFFER_SIZE_PALLETE   48
#define BUFFER_SIZE_STATUS   20
#define BUFFER_SIZE_BRIGHTNESS 1
#define OFFSET_PALLETE      0
#define OFFSET_STATUS       OFFSET_PALLETE+BUFFER_SIZE_PALLETE
#define OFFSET_BRIGHTNESS   OFFSET_STATUS+BUFFER_SIZE_STATUS
#define OFFSET_INDEX        BUFFER_SIZE_SCREEN
// #define ARCADE_STAT_WIDTH  8

//SPI Slave
#include <ESP32DMASPISlave.h>
ESP32DMASPI::Slave slave;
uint8_t* spi_slave_rx_buf;
uint32_t newdata=0;

//TFT
#include <TFT_eSPI.h> // Graphics and font library for driver chips
#include <SPI.h>
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
uint16_t *tft_dma_bufs[2];//for DMA TFT
// uint16_t *tft_dma_statbufs[2];//for DMA TFT
uint8_t bufIdx=0;

//debug/diag
const int btnOutputLog=0;
// uint32_t countTotal=0,count=0;
// uint32_t msStart=0;
uint32_t ms=0;
#define FpsFrameCount 20
uint32_t msFrames[FpsFrameCount];
uint8_t msFrameIdx=0;

void initSpiSlave(){
    // to use DMA buffer, use these methods to allocate buffer
    spi_slave_rx_buf = slave.allocDMABuffer(BUFFER_SIZE);//4 byte align, extra bytes to avoid trigger extra rx 0-size packet.

    // slave device configuration
    slave.setDataMode(SPI_MODE0);
    slave.setMaxTransferSize(BUFFER_SIZE+16);
    // slave.setDMAChannel(2); //2 or defaut Auto, TFT_eSPI using channel 1

    // begin() after setting
    slave.begin(VSPI);  // HSPI = CS: 15, CLK: 14, MOSI: 13, MISO: 12 -> default
                    // VSPI (CS:  5, CLK: 18, MOSI: 23, MISO: 19)

    Serial.println("slave.begin()");
    printf("Buffer Size=%d\n",BUFFER_SIZE);
}

void initTft(){
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("Makecode Arcade",120,260,2);
    tft.drawCentreString("Extra Screen v0.11",120,280,2);
    tft.drawCentreString("ready",120,300,1);
    tft.setSwapBytes(false);    //use false with swaped colors,avoid swap everytime, -4ms

    setup_t tftSetup; 
    tft.getSetup(tftSetup); //
    tft.drawString("Display SPI frequency =", 20, 30); 
    tft.drawFloat(tftSetup.tft_spi_freq/10.0,2,160, 30);
    tft.drawNumber(tftSetup.tft_driver, 160, 20,1); // // Hexadecimal code
    tft.drawNumber(tftSetup.tft_width, 160, 40,1);  // Rotation 0 width and height
    tft.drawNumber(tftSetup.tft_height, 160, 50,1);
    tft.drawNumber(tftSetup.pin_tft_dc, 160, 60,1);   // Control pins
    tft.drawNumber(tftSetup.pin_tft_rd, 160, 70,1);
    tft.drawNumber(tftSetup.pin_tft_wr, 160, 80,1);
    tft.drawNumber(tftSetup.pin_tft_rst, 160, 90,1);

    tft.setTextColor(TFT_WHITE);
    tft.writecommand(TFT_MADCTL);tft.writedata(TFT_MAD_COLOR_ORDER);
    tft.setAddrWindow(0, 0, 120, 160);
    tft.initDMA();
    tft_dma_bufs[0] =(uint16_t*)(void*) slave.allocDMABuffer(BUFFER_SIZE_SCREEN*2*4); // double pixel, in 16bit, /PACKAGE_DEVIDER
    tft_dma_bufs[1] =(uint16_t*)(void*) slave.allocDMABuffer(BUFFER_SIZE_SCREEN*2*4); // double pixel, in 16bit, /PACKAGE_DEVIDER
    // tft_dma_statbufs[0] =(uint16_t*)(void*) slave.allocDMABuffer(BUFFER_SIZE_STAT*2); 
    // tft_dma_statbufs[1] =(uint16_t*)(void*) slave.allocDMABuffer(BUFFER_SIZE_STAT*2); 
}

void setup() {
    Serial.begin(115200);
    while(!Serial) delay(10);

    initTft();

    initSpiSlave();

    //debug dianostic
    pinMode(btnOutputLog, INPUT_PULLUP);
    // msStart=millis();
    for(uint8_t i=0;i<FpsFrameCount;i++)
        msFrames[i]=millis();
}


void loop() {
    // if transaction has completed from master,
    // available() returns size of results of transaction,
    // and buffer is automatically updated

    // if there is no transaction in queue, add transaction
    if (slave.remained() <2) {
        // Serial.println("slave.remained() == 0");
        slave.queue(spi_slave_rx_buf, BUFFER_SIZE); //spi_slave_tx_buf
    }


    // updateTft();
    // while(0)
    // while (slave.available()) 
    if (slave.available()) 
    {
        // do something with received data: spi_slave_rx_buf

        newdata=slave.size();
        // memcpy(spi_slave_bak_buf, spi_slave_rx_buf, newdata);

        // show received data
        // printf("\n%dth got %d, q:%d ", countTotal++,newdata, slave.available());

        // if(!digitalRead(btnOutputLog)){
        //     if(newdata>128){
        //         for (size_t i = 0; i < 64; ++i) { 
        //             printf("%02X ", spi_slave_rx_buf[i]);
        //         }
        //         printf("\n...\n");
        //         for (size_t i = newdata-64; i < newdata; ++i) { 
        //             printf("%02X ", spi_slave_rx_buf[i]);
        //         }
                
        //     }else{
        //         for (size_t i = 0; i < newdata+10; ++i) { 
        //             printf("%02X ", spi_slave_rx_buf[i]);
        //         }
        //     }
        //     printf("\n");
        // }

        if(newdata==BUFFER_SIZE){
            if(spi_slave_rx_buf[BUFFER_SIZE-3]==0x77){
                ms=millis();
                updateTft();
                msFrames[msFrameIdx++]=ms;
                if(msFrameIdx>=FpsFrameCount) {
                    msFrameIdx=0;
                    printf("\n%d, fps:%3.2f \n",millis()-ms, 1000.0*FpsFrameCount/(PACKAGE_DEVIDER+1)/(millis()-msFrames[msFrameIdx]));
                }
            }else{//test, failed to resolve 
                printf("invalid frame!\n");
                // for (size_t i = newdata-32; i < newdata; ++i) { 
                //     printf("%02X ", spi_slave_rx_buf[i]);
                // }
                // printf("\n");

                //try to sync up frame, failed.
                // printf("remaind: %d, ",slave.remained());
                // slave.yield();
                // printf("remaind: %d, ",slave.remained());
                // while(slave.remained()>1){   
                //     slave.pop();
                // }
                // printf("remaind: %d ",slave.remained());
                for (int16_t i = 0; i < slave.size()-2; ++i) { 
                    if(spi_slave_rx_buf[i]==0x77&&spi_slave_rx_buf[i+1]==0x07&&spi_slave_rx_buf[i+2]==0x05){
                        printf("found at %d\n",i);
                        // slave.queue(spi_slave_rx_buf, i+3); //spi_slave_tx_buf
                        // slave.queue(spi_slave_rx_buf, BUFFER_SIZE); //spi_slave_tx_buf
                        break;
                    }
                }
            }
        // }else if(newdata==BUFFER_SIZE_STAT){
        //     updateStat();
        }else{
            printf("size: %d, ", slave.size());
        }
        slave.pop();
    }
}

//draw reversing in x dir, for arcade screen fliped x.
void drawChar_rvX(int32_t dx, int32_t dy, uint16_t c, uint16_t color){
  // printf("dx:%d, dy:%d, char:%d, color:%d\n",dx,dy,c,color);
    uint8_t column[6];
    uint8_t mask = 0x1;

    for (int8_t i = 0; i < 5; i++ ) 
      column[i] = pgm_read_byte(font + (c * 5) + i);
    column[5] = 0;

    for (int8_t j = 0; j < 8; j++) {
      for (int8_t k = 0; k < 5; k++ ) {
        if (column[k] & mask) {
          tft_dma_bufs[0][(dy+j)*TFT_WIDTH+dx-k]=color;
          // printf("%d ", (dy+j)*TFT_WIDTH+dy+(5-k));
        }
        // else {tft_Write_16(bg);}
      }
      mask <<= 1;
      // tft_Write_16(bg);
    }
}

uint16_t palette[]={
    //Makecode Arcade Indexed Colors
    // 0x0000, 0xFFFF, 0xF904, 0xFC98, 0xFC06, 0xFFA1, 0x24F4, 0x7EEA, 0x01F5, 0x879F, 0x8978, 0xA413, 0x5A0D, 0xE678, 0x9227, 0x0000, 
    //swapbyte
    0x0000, 0xFFFF, 0x04F9, 0x98FC, 0x06FC, 0xA1FF, 0xF424, 0xEA7E, 0xF501, 0x9F87, 0x7889, 0x13A4, 0x0D5A, 0x78E6, 0x2792, 0x0000,
    };

//HSPI
//SPI=40M: 36ms, everything good, except 0-size packages
//SPI=55M: 36ms, same with 40M. be normalized to 40Mhz, beyond max dividable freq=80Mhz/2
//VSPI
//SPI=27M: 120*160= 21ms, 240*320=56ms(51ms /wo digitlRead), with 0-size rx packages.
//SPI=40M: 240*320=37ms /wo digitlRead, with more wrong rx packages.
//SPI=20M: 240*320=67ms /wo digitlRead, with more wrong rx packages. Screen flash with error data.
uint8_t statusMsg[20];
void updateTft(){
    uint8_t index=spi_slave_rx_buf[OFFSET_INDEX];
    uint8_t *ptrRx;

  if(index==0xFF){ // Misc 
    //Brightness
    #define BRIGHTNESS_FIXED (100+25) //my ili9341 a little bright than arcade 7735
    const uint8_t brightness= spi_slave_rx_buf[OFFSET_BRIGHTNESS];
    
    //Pallete
    ptrRx= &spi_slave_rx_buf[OFFSET_PALLETE];
    for(int i=0;i<16;i++){
        uint16_t c= 
            (((uint16_t)(*ptrRx++)*brightness/BRIGHTNESS_FIXED)&0xF8)<<8 | //R 5bit
            (((uint16_t)(*ptrRx++)*brightness/BRIGHTNESS_FIXED)&0xFC)<<3 | //G 6bit
            (((uint16_t)(*ptrRx++)*brightness/BRIGHTNESS_FIXED)&0xF8)>>3 ; //B 5bit
        palette[i]= c<<8|c>>8;
    }

    for(int i=0;i<20;i++){
        statusMsg[i]= spi_slave_rx_buf[OFFSET_STATUS+i];
    }

  }else{ //Screen
    // printf(" %dth start", count);
        ptrRx=&spi_slave_rx_buf[0];
        for (size_t y = 0; y < ARCADE_HEIGHT/PACKAGE_DEVIDER; ++y) {
            //read from, every row
            // uint8_t *ptrRx=&spi_slave_rx_buf[(y+(j<1?0:ARCADE_HEIGHT>>1))*ARCADE_WIDTH]; //+((j%2==0)?0:(ARCADE_WIDTH>>1))
            //write to, every 2 rows,
            uint16_t *ptr=&tft_dma_bufs[bufIdx][2*y*TFT_WIDTH];             
            uint16_t *ptr2=&tft_dma_bufs[bufIdx][(2*y+1)*TFT_WIDTH];
            for (size_t x = 0; x < ARCADE_WIDTH ; ++x) { 
                uint16_t const c=palette[*ptrRx++];
                *ptr++ =c;  *ptr++ =c;
                *ptr2++ =c; *ptr2++ =c;
                
                // if(!digitalRead(btnOutputLog)){
                //     if(i<64||i>(newdata-64))
                //         printf("%04X ", tft_dma_bufs[bufIdx][i]);
                // }
            }
        }

        //Status
        if(index==0){
// unsigned long ms2=micros();
            // tft.endWrite();  //this will wait completed of prev transfer
            // DMA_BUSY_CHECK;
            // printf("Status begin, ");
            // spi_slave_rx_buf[OFFSET_STATUS+19]=0x0;
            // const char* ptrStatus=(char*)(void*)&spi_slave_rx_buf[OFFSET_STATUS];
            // tft.drawString(ptrStatus, 0, 0, 2);
            for(int i=0;i<20;i++){
                drawChar_rvX(TFT_WIDTH-(i*6+2), 2,statusMsg[i], 0xFFFF);
            }
            // printf("Status end\n");
// printf("%d ",micros()-ms2);
        }

    // printf(" %d# ",j);
      tft.endWrite();  //this will wait completed of prev transfer
      tft.setAddrWindow(0, (TFT_HEIGHT/PACKAGE_DEVIDER)*index, TFT_WIDTH, TFT_HEIGHT/PACKAGE_DEVIDER);
      tft.startWrite();
      tft.pushPixelsDMA(&tft_dma_bufs[bufIdx][0], BUFFER_SIZE_SCREEN*4);
      
      bufIdx=1-bufIdx;
    }

    // printf("%dth end", count);
}

// void updateStat(){
//     uint8_t *ptrRx=&spi_slave_rx_buf[0];
//     uint16_t *ptr=&tft_dma_statbufs[bufIdx][0];             
//     for (size_t x = 0; x < BUFFER_SIZE_STAT ; ++x) { 
//         *ptr++ =palette[*ptrRx++]; 
//     }

//     unsigned long ms2=millis();
//     tft.endWrite();  //this will wait completed of prev transfer
//     tft.setAddrWindow(ARCADE_WIDTH, 0, ARCADE_STAT_WIDTH, ARCADE_HEIGHT);
//     tft.startWrite();
//     tft.pushPixelsDMA(&tft_dma_statbufs[bufIdx][0], BUFFER_SIZE_STAT);
//     printf("%d ",millis()-ms2); // ? screen tremble if commented.

//     bufIdx=1-bufIdx;
// }
