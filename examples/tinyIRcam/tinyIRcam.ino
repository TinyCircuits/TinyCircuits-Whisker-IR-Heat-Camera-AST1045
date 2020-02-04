//////////////////////////////////////////////////
/// tinyscreen+ amg88 thermal camera
/// (C) 2018 JAMcInnes, Use freely for any purpose
///
/// based on some example code found at adafruit, tinyscreen, and other

#include <Wire.h>
#include <Adafruit_AMG88xx.h>
#include <TinyScreen.h> 

// tinyscreen stuff
TinyScreen display = TinyScreen(TinyScreenPlus);

// grideye stuff
Adafruit_AMG88xx amg;
float irdata[ AMG88xx_PIXEL_ARRAY_SIZE ];
const int AMG88xx_HEIGHT = 8;
const int AMG88xx_WIDTH = 8;

// ir bitmap
const int bmp_scale = 2;
const int bmp_width = AMG88xx_HEIGHT * bmp_scale;
const int bmp_height = AMG88xx_WIDTH * bmp_scale;
//uint8_t bmp[ bmp_width * bmp_height ]; <--don't need this anymore

// offscreen buffer
const int obuf_dim = 64;
uint8_t obuf[ obuf_dim * obuf_dim ];

/// a temperature to color mapping helper
struct AMapColor
{
  float val;
  uint8_t r, g, b; // 8bit R G B

  uint8_t Color8()
  {
    return Color8( r, g, b );
  }

  static uint8_t Color8( uint8_t rin, uint8_t gin, uint8_t bin )
  {
    // packed 8 bit 332 BGR
    uint8_t bb = bin >> 5;
    uint8_t gg = gin >> 5;
    uint8_t rr = rin >> 6;
    uint8_t col8 = (bb << 5) | (gg << 2) | rr;
    return col8;
  }
};

/// a color map is an array of amapcolors
// colorful
AMapColor gMap0[] =
{
  { -10, 0, 0, 0 },
  { 0, 20, 0, 20 },
  { 24, 0, 0, 120 },
  { 27, 0, 240, 0 },
  { 34, 255, 255, 0 },
  { 42, 255, 0, 0 },
  { 65, 255, 50, 50 },
  { 80, 255, 190, 190 }
};

// low range blue
AMapColor gMap1[] =
{
  { -10, 0, 0, 15 },
  { 0, 0, 0, 255 },
  { 20, 0, 100, 255 },
  { 40, 0, 255, 255 }
};

// high range red
AMapColor gMap2[] =
{
  { 20, 15, 0, 0 },
  { 80, 255, 0, 0 },
  { 120, 255, 120, 0 }
};

// array of colormaps
struct AMap
{
  AMapColor *col_array;
  int num_col;
};

AMap gMapArray[] =    // eeccch this is getting ugly
{
  { gMap0, sizeof(gMap0)/sizeof(AMapColor) },
  { gMap1, sizeof(gMap1)/sizeof(AMapColor) },
  { gMap2, sizeof(gMap2)/sizeof(AMapColor) }
};
const int CMAPARRAY_SIZE = sizeof(gMapArray)/sizeof(AMap);
int gCMapIdx = 0; // this is a user control

// some user controls
bool gfEmphasis = true;

enum
{
  INTERPOLATION_NEAREST = 0,
  INTERPOLATION_BILINEAR,
  INTERPOLATION_BICUBIC
};
const int INTERPOLATION_MAX = 2;
int gInterpolation = INTERPOLATION_BILINEAR;


float GetIRPixel( int px, int py )
{ 
  // reverse x 
  px = AMG88xx_WIDTH - 1 - px;
  
  if ( px < 0 )
    px = 0;
  else if ( px >= AMG88xx_WIDTH )
    px = AMG88xx_WIDTH - 1;

  // we'll just go with Y as it is.. not much help in datasheet
  // ..or on web.
  if ( py < 0 )
    py = 0;
  else if ( py >= AMG88xx_HEIGHT )
    py = AMG88xx_HEIGHT - 1;
    
  return irdata[ py * AMG88xx_HEIGHT + px ]; // column order?
}

/*
float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
*/

float SampleBilinear( float u, float v )
{
  float x = (u * (AMG88xx_WIDTH-1));// - 0.5f;
  int xi = int(x);
  float x_frac = x - floor(x);
  
  float y = (v * (AMG88xx_HEIGHT-1));// - 0.5f;
  int yi = int(y);
  float y_frac = y - floor(y);

  float inv_y_frac = 1.0f - y_frac;
  float inv_x_frac = 1.0f - x_frac;

  float c0 = GetIRPixel( xi, yi );
  float c1 = GetIRPixel( xi+1, yi );
  float c2 = GetIRPixel( xi+1, yi+1 );
  float c3 = GetIRPixel( xi, yi+1 );

  float tc = c0 * inv_y_frac * inv_x_frac +
      c1 * inv_y_frac * x_frac +
      c2 * y_frac * x_frac +
      c3 * y_frac * inv_x_frac;

  return tc;
}

// t is a value that goes from 0 to 1 to interpolate in a C1 continuous way across uniformly sampled data points.
// when t is 0, this will return B.  When t is 1, this will return C.  Inbetween values will return an interpolation
// between B and C.  A and B are used to calculate slopes at the edges.
float CubicHermite (float A, float B, float C, float D, float t)
{       
  float a = -A / 2.0f + (3.0f*B) / 2.0f - (3.0f*C) / 2.0f + D / 2.0f;
  float b = A - (5.0f*B) / 2.0f + 2.0f*C - D / 2.0f;
  float c = -A / 2.0f + C / 2.0f;
  float d = B;
  return a*t*t*t + b*t*t + c*t + d;
}

float SampleBicubic ( float u, float v )
{       
  float x = (u * (AMG88xx_WIDTH-1));
  int xi = int(x); 
  float x_frac = x - floor(x);

  float y = (v * (AMG88xx_HEIGHT-1));
  int yi = int(y); 
  float y_frac = y - floor(y);

  // 1st row 
  float p00 = GetIRPixel( xi - 1, yi - 1);
  float p10 = GetIRPixel( xi + 0, yi - 1);
  float p20 = GetIRPixel( xi + 1, yi - 1);
  float p30 = GetIRPixel( xi + 2, yi - 1);
  // 2nd row 
  float p01 = GetIRPixel( xi - 1, yi + 0);
  float p11 = GetIRPixel( xi + 0, yi + 0);
  float p21 = GetIRPixel( xi + 1, yi + 0);
  float p31 = GetIRPixel( xi + 2, yi + 0);
  // 3rd row 
  float p02 = GetIRPixel( xi - 1, yi + 1);
  float p12 = GetIRPixel( xi + 0, yi + 1);
  float p22 = GetIRPixel( xi + 1, yi + 1);
  float p32 = GetIRPixel( xi + 2, yi + 1);
  // 4th row 
  float p03 = GetIRPixel( xi - 1, yi + 2);
  float p13 = GetIRPixel( xi + 0, yi + 2);
  float p23 = GetIRPixel( xi + 1, yi + 2);
  float p33 = GetIRPixel( xi + 2, yi + 2);

  // bicubic interpolation
  float col0 = CubicHermite( p00, p10, p20, p30, x_frac );
  float col1 = CubicHermite( p01, p11, p21, p31, x_frac );
  float col2 = CubicHermite( p02, p12, p22, p32, x_frac );
  float col3 = CubicHermite( p03, p13, p23, p33, x_frac );
  float tc = CubicHermite( col0, col1, col2, col3, y_frac );

  return tc;
}

/// map a value to a color
uint8_t MapColor8( float val, int idx, AMap &cmap )
{  
  int max_col = cmap.num_col - 1;
  
  if( idx < 0 )
    return cmap.col_array[0].Color8();
  else if ( idx >= max_col )
    return cmap.col_array[ max_col ].Color8();

  AMapColor &mc0 = cmap.col_array[idx];
  AMapColor &mc1 = cmap.col_array[idx+1];

  uint8_t red = map( val, mc0.val, mc1.val, mc0.r, mc1.r );
  uint8_t green = map( val, mc0.val, mc1.val, mc0.g, mc1.g );
  uint8_t blue = map( val, mc0.val, mc1.val, mc0.b, mc1.b );

  return AMapColor::Color8( red, green, blue );
}

uint8_t TempToColor8( float temp_c, AMap &cmap )
{
  // search color map for temp_c
  uint8_t col = 0;
  for ( int i = 0; i < cmap.num_col; i++ )
  {
    if ( temp_c <= cmap.col_array[i].val )
    {
      col = MapColor8( temp_c, i-1, cmap );
      return col;
    }
  }

  // use last color in map
  col = MapColor8( temp_c, cmap.num_col-1, cmap );
  return col;
}

float ToFarenheit( float tc )
{
  return (tc * 9/5) + 32;
}

void setup()
{
  Wire.begin();
  SerialUSB.begin(9600);
  delay(50);
//  while (!SerialUSB);

  /// Setup for the TinyScreen
  display.begin();
  display.setBitDepth( TSBitDepth8 );
  display.setBrightness(12);  // 0 to 15
  display.setFlip(true);
  display.clearScreen();
  display.setFont( thinPixel7_10ptFontInfo );

  /// setup AMG
  // default settings
  bool fstatus = amg.begin();
  if ( !fstatus )
  {
    const char *err_msg = "No AMG88xx sensor!";
    display.println( err_msg );
    SerialUSB.println( err_msg );
    while (1);
  }

  delay(100); // let sensor boot up
}

void loop()
{ 
  AMap &cmap = gMapArray[ gCMapIdx ]; // get a reference to the current color map
  
  // which way is up on an amg88??
  // not sure what the intended orientation or order is here.. the amg datasheet
  // ..is ambiguous. same for project photos on the web.. we'll just go with
  // our own. see getirpixel()
  amg.readPixels( irdata );

  // find min and max temp
  float temp_min = 999999999;
  float temp_max = -999999999;
  for ( int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++ )
  {
    if ( irdata[i] < temp_min )
      temp_min = irdata[i];
      
    if ( irdata[i] > temp_max )
      temp_max = irdata[i];
  }
  float temp_range = temp_max - temp_min;

  // bilinear blit irdata to bmp
  int o_scale = obuf_dim / bmp_width;  // uniform scale assumed
  for ( int by=0; by < bmp_height; by++ )
  {
    float pv = float(by) / float(bmp_height - 1);
    for ( int bx=0; bx < bmp_width; bx++ )
    {
      float pu = float(bx) / float(bmp_width - 1);

      // sample irdata
      float temp_c;
      if ( gInterpolation == INTERPOLATION_NEAREST )      
        temp_c = GetIRPixel( pu*AMG88xx_WIDTH, pv*AMG88xx_HEIGHT ); // nearest neighbor
      else if ( gInterpolation == INTERPOLATION_BILINEAR )
        temp_c = SampleBilinear( pu, pv );
      else if ( gInterpolation == INTERPOLATION_BICUBIC )
        temp_c = SampleBicubic( pu, pv );
      else
        temp_c = 0;

      // do emphasis?
      if ( gfEmphasis )
      {
        // square it
        float em = (temp_c - temp_min) / temp_range;
        em = em * em;
        temp_c = em * temp_range + temp_min;
      }
      else
      {
        temp_c = constrain( temp_c, temp_min, temp_max );
      }
      
      // do dynamic range?
      if ( false )
      {
        // fixme: map to current colormap, not just fixed 80C
        temp_c = (temp_c - temp_min) / temp_range * 80.0f;
      }

      uint8_t col = TempToColor8( temp_c, cmap );
//      bmp[ by * bmp_width + bx ] = col;
      // render direct to obuf
      uint8_t *pobuf_col = obuf + by * o_scale * obuf_dim;
      for ( int j = 0; j < o_scale; j++ )
      {
        uint8_t *pobuf_row = pobuf_col + bx * o_scale;
        pobuf_col += obuf_dim;
        for ( int i = 0; i < o_scale; i++ )
        {
          *(pobuf_row++) = col;
//          obuf[oy * obuf_dim + ox] = col;
        }
      }

    }
  }
/*
  // strech blit bmp to offscreen buffer
  int o_scale = obuf_dim / bmp_width;
  
  for ( int by=0; by < bmp_height; by++ )
  {
    for ( int bx=0; bx < bmp_width; bx++ )
    {
      uint8_t bmp_col = bmp[by * bmp_width + bx];      
      for ( int oy=by*o_scale; oy<(by+1)*o_scale; oy++ )
      {
        for ( int ox=bx*o_scale; ox<(bx+1)*o_scale; ox++ )
        {
          obuf[oy * obuf_dim + ox] = bmp_col;
        }
      }
    }
  }
*/

  // blit obuf to screen
  display.setX( 0, 0 + obuf_dim-1 );
  display.setY( 0, 0 + obuf_dim-1 );  
  display.startData();
  display.writeBuffer( obuf, obuf_dim * obuf_dim );
  display.endTransfer();

  // print min max temperature
  display.fontColor( TS_8b_Red, TS_8b_Black );
  display.setCursor( 65, 0 );
  display.print( "high:" );
  display.setCursor( 65, 10 );
  display.print( ToFarenheit( temp_max ), 1 );
  display.print( "F" );

  display.fontColor( TS_8b_Blue, TS_8b_Black );
  display.setCursor( 65, 20 );
  display.print( "low:" );
  display.setCursor( 65, 30 );
  display.print( ToFarenheit( temp_min ), 1 );
  display.print( "F" );

  // handle buttons
  DoInput();
  
  // print battery voltage & amg internal thermistor to serial
  float battVoltageReading = getBattVoltage();
  SerialUSB.print( battVoltageReading) ;
  SerialUSB.print("V battery, ");

  SerialUSB.print("thermistor ");
  SerialUSB.print( amg.readThermistor() );
  SerialUSB.println(" *C");
}

int CenterCursor( const char *msg )
{
  int mheight = display.getFontHeight();
  int mwidth = display.getPrintWidth( const_cast<char *>( msg ) );
  int cx = (display.xMax + 1)/2 - mwidth/2;
  int cy = (display.yMax + 1)/2 - mheight/2;
  display.setCursor( cx, cy );
}


void DoQuickMsg( const char *msg )
{
  // print the message and briefly pause, this also serves as a btn debounce
  display.fontColor( TS_8b_White, TS_8b_Black );
  CenterCursor( msg );
  display.print( msg );
  delay( 1000 );

  // clear everything to cleanup text
  display.clearScreen();
}

void DoInput()
{
  if ( display.getButtons(TSButtonUpperLeft) )
  {
    // cycle colormap
    if ( (++gCMapIdx) >= CMAPARRAY_SIZE )
    {
      gCMapIdx = 0;
    }
    DoQuickMsg( "colormap" );    
  }
  else if ( display.getButtons(TSButtonLowerLeft) )
  {
    // cycle emphasis on/off
    gfEmphasis = !gfEmphasis;    
    gfEmphasis ? DoQuickMsg( "emphasis on" ) : DoQuickMsg( "emphasis off" );
  }
  else if ( display.getButtons(TSButtonUpperRight) )
  {
    // cycle interpolation
    if ( (++gInterpolation) > INTERPOLATION_MAX )
    {
      gInterpolation = 0;
    }
    
    if ( gInterpolation == INTERPOLATION_NEAREST )
      DoQuickMsg( "nearest" );
    else if ( gInterpolation == INTERPOLATION_BILINEAR )
      DoQuickMsg( "bilinear" );
    else if ( gInterpolation == INTERPOLATION_BICUBIC )
      DoQuickMsg( "bicubic" );
  }

}

// This function gets the battery VCC internally, you can checkout this link 
// if you want to know more about how: 
// http://atmel.force.com/support/articles/en_US/FAQ/ADC-example
float getVCC()
{
  SYSCTRL->VREF.reg |= SYSCTRL_VREF_BGOUTEN;
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->SAMPCTRL.bit.SAMPLEN = 0x1;
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->INPUTCTRL.bit.MUXPOS = 0x19;         // Internal bandgap input
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->CTRLA.bit.ENABLE = 0x01;             // Enable ADC
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->SWTRIG.bit.START = 1;  // Start conversion
  ADC->INTFLAG.bit.RESRDY = 1;  // Clear the Data Ready flag
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->SWTRIG.bit.START = 1;  // Start the conversion again to throw out first value
  while ( ADC->INTFLAG.bit.RESRDY == 0 );   // Waiting for conversion to complete
  uint32_t valueRead = ADC->RESULT.reg;
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->CTRLA.bit.ENABLE = 0x00;             // Disable ADC
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  SYSCTRL->VREF.reg &= ~SYSCTRL_VREF_BGOUTEN;
  float vcc = (1.1 * 1023.0) / valueRead;
  return vcc;
}

// Calculate the battery voltage
float getBattVoltage()
{
  const int VBATTpin = A4;
  float VCC = getVCC();

  // Use resistor division and math to get the voltage
  float resistorDiv = 0.5;
  float ADCres = 1023.0;
  float battVoltageReading = analogRead(VBATTpin);
  battVoltageReading = analogRead(VBATTpin); // Throw out first value
  float battVoltage = VCC * battVoltageReading / ADCres / resistorDiv;

  return battVoltage;
}
