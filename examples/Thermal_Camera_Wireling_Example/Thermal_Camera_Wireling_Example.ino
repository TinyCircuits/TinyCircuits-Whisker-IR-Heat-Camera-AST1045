/****************************************************************************
 * IR Array Thermal Camera Wireling Example
 * This Arduino sketch assumes the use of a TinyScreen+ processor being used
 * with a Thermal Camera Wireling plugged into Port 1 of a Wireling Adapter 
 * TinyShield. The program will display the 8x8 thermal array of temperatures 
 * ranging from MINTEMP degC (blue) to MAXTEMP degC (red).
 * 
 * Hardware by: TinyCircuits
 * Adapted from Adafruit Example by: Hunter Hykes for TinyCircuits
 * Initiated 8/07/19
 * Last updated 10/18/19
 ****************************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <TinyScreen.h>
#include <Wireling.h>
#include <Adafruit_AMG88xx.h> 

TinyScreen display = TinyScreen(TinyScreenPlus);
uint8_t buffer[96 * 64 * 2];

Adafruit_AMG88xx amg; // thermal camera object

float pixels[AMG88xx_PIXEL_ARRAY_SIZE]; // data array from thermal camera
uint16_t displayPixelWidth, displayPixelHeight;

const uint8_t xCameraRes = 8;
const uint8_t yCameraRes = 8;

float minimum = 0x7F7FFFFF; // maximum value for a float
float maximum = -273.15; // absolute zero (you won't get this cold)

//low range of the sensor (this will be blue on the screen)
#define MINTEMP 0

//high range of the sensor (this will be red on the screen)
#define MAXTEMP 34

#define backgroundColor TS_16b_Black

//the colors we will be using
const uint16_t camColors[] = {0x480F,
0x400F,0x400F,0x400F,0x4010,0x3810,0x3810,0x3810,0x3810,0x3010,0x3010,
0x3010,0x2810,0x2810,0x2810,0x2810,0x2010,0x2010,0x2010,0x1810,0x1810,
0x1811,0x1811,0x1011,0x1011,0x1011,0x0811,0x0811,0x0811,0x0011,0x0011,
0x0011,0x0011,0x0011,0x0031,0x0031,0x0051,0x0072,0x0072,0x0092,0x00B2,
0x00B2,0x00D2,0x00F2,0x00F2,0x0112,0x0132,0x0152,0x0152,0x0172,0x0192,
0x0192,0x01B2,0x01D2,0x01F3,0x01F3,0x0213,0x0233,0x0253,0x0253,0x0273,
0x0293,0x02B3,0x02D3,0x02D3,0x02F3,0x0313,0x0333,0x0333,0x0353,0x0373,
0x0394,0x03B4,0x03D4,0x03D4,0x03F4,0x0414,0x0434,0x0454,0x0474,0x0474,
0x0494,0x04B4,0x04D4,0x04F4,0x0514,0x0534,0x0534,0x0554,0x0554,0x0574,
0x0574,0x0573,0x0573,0x0573,0x0572,0x0572,0x0572,0x0571,0x0591,0x0591,
0x0590,0x0590,0x058F,0x058F,0x058F,0x058E,0x05AE,0x05AE,0x05AD,0x05AD,
0x05AD,0x05AC,0x05AC,0x05AB,0x05CB,0x05CB,0x05CA,0x05CA,0x05CA,0x05C9,
0x05C9,0x05C8,0x05E8,0x05E8,0x05E7,0x05E7,0x05E6,0x05E6,0x05E6,0x05E5,
0x05E5,0x0604,0x0604,0x0604,0x0603,0x0603,0x0602,0x0602,0x0601,0x0621,
0x0621,0x0620,0x0620,0x0620,0x0620,0x0E20,0x0E20,0x0E40,0x1640,0x1640,
0x1E40,0x1E40,0x2640,0x2640,0x2E40,0x2E60,0x3660,0x3660,0x3E60,0x3E60,
0x3E60,0x4660,0x4660,0x4E60,0x4E80,0x5680,0x5680,0x5E80,0x5E80,0x6680,
0x6680,0x6E80,0x6EA0,0x76A0,0x76A0,0x7EA0,0x7EA0,0x86A0,0x86A0,0x8EA0,
0x8EC0,0x96C0,0x96C0,0x9EC0,0x9EC0,0xA6C0,0xAEC0,0xAEC0,0xB6E0,0xB6E0,
0xBEE0,0xBEE0,0xC6E0,0xC6E0,0xCEE0,0xCEE0,0xD6E0,0xD700,0xDF00,0xDEE0,
0xDEC0,0xDEA0,0xDE80,0xDE80,0xE660,0xE640,0xE620,0xE600,0xE5E0,0xE5C0,
0xE5A0,0xE580,0xE560,0xE540,0xE520,0xE500,0xE4E0,0xE4C0,0xE4A0,0xE480,
0xE460,0xEC40,0xEC20,0xEC00,0xEBE0,0xEBC0,0xEBA0,0xEB80,0xEB60,0xEB40,
0xEB20,0xEB00,0xEAE0,0xEAC0,0xEAA0,0xEA80,0xEA60,0xEA40,0xF220,0xF200,
0xF1E0,0xF1C0,0xF1A0,0xF180,0xF160,0xF140,0xF100,0xF0E0,0xF0C0,0xF0A0,
0xF080,0xF060,0xF040,0xF020,0xF800,};

uint8_t dispColors[AMG88xx_PIXEL_ARRAY_SIZE]; // used to store 8x8 grid of colors to display (*2 since actually 16-bit)

int pixelSize;
int xMax = display.xMax;
int yMax = display.yMax;

void setup() {
  Wire.begin();
  Wireling.begin();
  display.begin();
  display.setBrightness(10);
  display.clearScreen();
  display.setFlip(0);
  display.setBitDepth(TSBitDepth16);
  
  displayPixelWidth = yMax / yCameraRes;
  displayPixelHeight = xMax / xCameraRes;

  pixelSize = findPixelSize(xMax, yMax);
  
  Wireling.selectPort(1);
  delay(100); // let sensor boot up
  amg.begin();
  
}

void loop() {
  amg.readPixels(pixels);
  getMinMax();
  drawBuffer();
}

void getMinMax() {
  minimum = 0x7F7FFFFF; // maximum value for a float
  maximum = -273.15; // absolute zero (you won't get this cold)
  
  for(int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
    float val = pixels[i];
    if(val > maximum)
      maximum = val;
    if(val < minimum)
      minimum = val;
  }

  float range = maximum - minimum;
  int minimumRange = 7;
  if(range < minimumRange) {
    minimum += (minimumRange - (maximum-minimum)) / 2;
    maximum += (minimumRange - (maximum-minimum)) / 2;
  }
}

// part of the color workaround
uint16_t convertColor(uint16_t original) {
  uint16_t converted = 0x0000;

  converted |= (original & 0xF800) >> 11; // RED
  converted |= (original & 0x07E0);       // GREEN
  converted |= (original & 0x001F) << 11; // BLUE

  return converted;
}

//"pixels" on display will be n by n pixels, where n is the returned value
int findPixelSize(int screenXwidth, int screenYheight) {
  if(screenXwidth == screenYheight) { // screen is a square (convenient!)
    return screenXwidth / xCameraRes; // 8 is from the 8x8 thermal camera resolution
  } else if(screenXwidth < screenYheight){
    return screenXwidth / xCameraRes;
  } else {
    return screenYheight / yCameraRes;
  }
}

void drawBuffer() {                             //populates the buffer, then writes it to the screen
  for (int i = 0; i < 64 * 96; i++) {           //For every pixel in the buffer, write the background color first
    buffer[i * 2] = backgroundColor >> 8;
    buffer[i * 2 + 1] = backgroundColor;
  }

  //puts specified string into the buffer in the font and colors specified at the given coordinates
  putImage();

  display.goTo(0, 0);
  display.startData();                          //Activates the OLED driver chip to ready it for receiving commands (drives CS line HIGH)
  display.writeBuffer(buffer, 96 * 64 * 2);     //Write all of the pixel data (saved in buffer) to the screen (update what is being displayed on screen)
  display.endTransfer();                        //Deactivate the OLED driver chip now that it has been updated (drives CS pin LOW)
}

void putImage() {
  // get adjusted origin to center image
  int xOrigin = (xMax - (pixelSize * xCameraRes)) / 2; // use 8 since camera is 8x8 resolution
  int yOrigin = (yMax - (pixelSize * yCameraRes)) / 2; // use 8 since camera is 8x8 resolution
  uint16_t color = 0x00;

  int xCamera = 0;
  int yCamera = 0;
  int xScreen = 0;
  int yScreen = 0;
  int colorIndex = 0;
  
  for(int i = 0; i < (xCameraRes * yCameraRes); i++) {
    xCamera = i % xCameraRes;
    yCamera = (i / yCameraRes);
    xScreen = (xCamera * pixelSize) + xOrigin;
    yScreen = yCamera * pixelSize;

    // map temperature reading to the appropriate color array index
    colorIndex = map(pixels[yCamera * yCameraRes + xCamera], minimum, maximum, 0, 255);
    //colorIndex = map(pixels[yCamera * yCameraRes + xCamera], MINTEMP, MAXTEMP, 0, 255);
    // constrain array index between 0 and 255 so we do not surpass array bounds
    colorIndex = constrain(colorIndex, 0, 255);
    
    // set color to the converted value from the color array at the proper index found above
    color = convertColor(camColors[colorIndex]);

    // for each color we get, we must insert a pixelSize by pixelSize square onto the screen
    for(int j = 0; j < pixelSize; j ++) {
      for(int k = 0; k < pixelSize; k ++) {
        putPixel(buffer, xScreen + j, yScreen + k, color);
      }
    }
    
  }
}

void putPixel(uint8_t * buff, int x0, int y0, uint16_t color) {                                 
  x0 = constrain(x0, 0, xMax);          //constrains pixel's x-coordinate to the screen bounds
  y0 = constrain(y0, 0, yMax);          //constrains pixel's y-coordinate to the screen bounds
  buff[(y0 * 96 + x0) * 2] = color >> 8;    //set the buffer at appropriate index to first 8 bits of color
  buff[(y0 * 96 + x0) * 2 + 1] = color;     //set the buffer at the following index to the last 8 bits of color
}
