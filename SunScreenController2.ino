// Sun screen controller
////////////////////////
// file: SunScreenController2
// date: 6-Jun-2014

/*
measure value
take an average of 10 samples over 10 minutes.
check if value is VALUE_SUN or VALUE_NO_SUN
if SCREEN_DOWN then keep it at least 30 minutes down
if SCREEN_UP   then keep it at least 10 minutes up

check if value is VALUE_EVENING then SCREEN_UP immediatly
check if extern screen up or screen down command, action immediatly

v0.3 4-Jul-2013
Werkt met IR remote.

v0.4 4-Jun-2014
Structuur wijziging, metingen op interval, maar IRremote vaker checken.

v0.5 (4-Jun-2014)
Zon limit van 800 naar 850

Todo:
- IRremote in interrupt meenemen.
- ADC waarde omrekenenen naar volts
- ADC calibreren
- besturing ook via RS232 (later RFlink)
- nog twee beschermingsdioden over fotocel ingang (pin 4)
- pull up aan de IR remote aansluiting (pin 11).

*/


#include <IRremote.h>



///// PIN assignments
const byte PIN_SENSOR            = 0;      // sunlight sensor connected to this pin
const byte PIN_LED_4             = 4;      // LED on shield
const byte PIN_LED               = 13;     // LED on Arduino board
const byte PIN_RELAY_MAIN        = 5;      // 230V on or off
const byte PIN_RELAY_DIRECTION   = 6;      // determines up or down

///// IR remote vars
int        PIN_SENSOR_IR         = 11;

IRrecv     IRremote( PIN_SENSOR_IR );
decode_results IRresults;


///// control vars and constants
const int MODE_MANUAL            = 1;
const int MODE_AUTOMATIC         = 2;
int       mode                   = MODE_AUTOMATIC;

const int MANUAL_UP              = 1;     // screen up
const int MANUAL_DOWN            = 2;     // screen down
const int MANUAL_STOP            = 3;     // stop the motor
const int MANUAL_MODE_AUTOMATIC  = 4;     // reacts to sunlight AND manual commands
const int MANUAL_MODE_MANUAL     = 5;     // no reaction to sunlight
const int MANUAL_READY           = 6;     // default state of the manual_command
int       manual_command         = MANUAL_READY;

const int INTERVAL_TIME_UP       = 6;    // ... times the 10 measurements
const int INTERVAL_TIME_DOWN     = 9;

const int NUMBER_OF_READINGS_MAX = 10;    // before the screen is controlled

const int SCREEN_UP              = 1;     // no sunshine
const int SCREEN_DOWN            = 2;     // there is sunshine

int       sensorValuesForAverage = 0;
int       numberOfReadings       = 0;
int       indexReading           = 0;
int       sensorValue            = 0;     // measured actual sensor value
int       sensorValueAverage     = 0;     // averaged value after NUMBER_OF_READINGS

const int SENSOR_SUN_YES         = 870;   // changed 800 -> 870 5-Jul-14/sjp
const int SENSOR_SUN_NO          = 300;   // 80;
const int SENSOR_SUN_EVENING     = 5;     // the reading below this value when no sensor is connected (not for a suncel)


 
  
///////////////////////////////////////////////////////////
void setup(void) 
{
  // initialize inputs/outputs
  pinMode(      PIN_LED,   OUTPUT);              // LED indicates measurement
  pinMode(      PIN_LED_4, OUTPUT);              // LED debug
  digitalWrite( PIN_LED_4, HIGH );               // on for now
  
  pinMode(      PIN_RELAY_MAIN, OUTPUT );
  digitalWrite( PIN_RELAY_MAIN, HIGH );          // immediatly un-power the relay
  pinMode(      PIN_RELAY_DIRECTION, OUTPUT );
  digitalWrite( PIN_RELAY_DIRECTION, HIGH );     // immediatly un-power the relay

  IRremote.enableIRIn();                         // Start the IR receiver

  // start serial port
  Serial.begin(9600);
  
  Serial.println();
  Serial.println( "SunScreenController v0.5 (5-Jul-2014)" );
  Serial.println();
  
  SensorCalibrate();
  
} // end setup()



///////////////////////////////////////////////////////////
void loop( void ) 
{
  static int interval_time = 10;

  sensorValue = SensorRead();
 
 
 if( mode == MODE_AUTOMATIC )
 { 
//   time_stamp();
 //  Serial.print( "[Automatic] Sensor value: " );
  // Serial.println( sensorValue );
 
   numberOfReadings++;
   sensorValuesForAverage = sensorValuesForAverage + sensorValue;
   
   if ( numberOfReadings >= NUMBER_OF_READINGS_MAX )
   { 
     sensorValueAverage = sensorValuesForAverage / NUMBER_OF_READINGS_MAX;
     
     // reset for next measures
     sensorValuesForAverage = 0;  // start again
     numberOfReadings       = 0;
     
     time_stamp();
     Serial.print( "[Automatic] Average sensor value: " );
     Serial.println( sensorValueAverage );
    
     if ( sensorValueAverage < SENSOR_SUN_NO)
     {
       control_screen( SCREEN_UP );
       interval_time = INTERVAL_TIME_UP;
     }
     
     if ( sensorValueAverage > SENSOR_SUN_YES)
     { 
       control_screen( SCREEN_DOWN );
       interval_time = INTERVAL_TIME_DOWN;
     }
     
   } // if numberOfReadings
    
   WaitSeconds( interval_time );
   Serial.print( "." );
  
  } // end mode Automatic
     
     
 if( mode == MODE_MANUAL )
 { 
   WaitSeconds( 1 );
   LED_toggle( );
 }
    
  switch ( CheckIRremote()  )
  {
    case MANUAL_UP:
      screen_up();
      manual_command = MANUAL_READY;
    break;
   
    case MANUAL_DOWN:
      screen_down();
      manual_command = MANUAL_READY;
    break;
     
    case MANUAL_STOP:
      screen_stop();
      manual_command = MANUAL_READY;
    break;
     
    case MANUAL_MODE_AUTOMATIC:
      Serial.println( "Mode is AUTOMATIC" );  
      mode           = MODE_AUTOMATIC;
      manual_command = MANUAL_READY;
      interval_time  = 1;
    break;
     
    case MANUAL_MODE_MANUAL:
      Serial.println( "Mode is MANUAL" );  
      mode           = MODE_MANUAL;     
      manual_command = MANUAL_READY;
      interval_time = 1;
    break;
     
    default:
      manual_command = MANUAL_READY;
      // interval_time = 1;
    break;
  } // end switch  
      
} // end main loop




///////////////////////////////////////////////////////////
// put the screen up or down and do it once! 
///////////////////////////////////////////////////////////
int control_screen( int ScreenUpOrDown )
{
  
  const int SCREEN_STATE_UP    = 1;
  const int SCREEN_STATE_DOWN  = 2;
  const int SCREEN_STATE_READY = 3;

  static int ScreenStateActual;
  static int ScreenState;

  
  if( ScreenUpOrDown == SCREEN_UP )   ScreenState = SCREEN_STATE_UP;
  if( ScreenUpOrDown == SCREEN_DOWN ) ScreenState = SCREEN_STATE_DOWN; 
  
  switch( ScreenState )
  {
     case SCREEN_STATE_UP:
     { 
       if( ScreenStateActual != SCREEN_STATE_UP ) 
       {     
        screen_up(); 
        ScreenStateActual = SCREEN_STATE_UP;
       }
       ScreenState = SCREEN_STATE_READY;
     } // case SCREEN_UP
     break;
     
     case SCREEN_STATE_DOWN:
     {
       if( ScreenStateActual != SCREEN_STATE_DOWN ) 
       {     
         screen_down(); 
         ScreenStateActual = SCREEN_STATE_DOWN;

       }
       ScreenState = SCREEN_STATE_READY;
     } // case SCREEN_DOWN
     break;
     
     case SCREEN_STATE_READY:
     {
       // do nothing
     }
     break;
     
     default:
     {
       // ScreenState  = SCREEN_STATE_UP;
     }
     break;
  }

  return( 0 ); // $$ change  
  
} // end control_screen();



int sensor_calibration_factor = 1;

///////////////////////////////////////////////////////////
// SensorRead() is a function to make it easy to change
// a sensor. only recalibrate
///////////////////////////////////////////////////////////
int SensorRead( void )
{
  int adc_value;
  
  adc_value = analogRead( PIN_SENSOR );
  
  // todo: return the milli volts of the solar cell. 
    // adc_value =  (VCC * sensor_calibration_factor * 1000 mV) / analog reading;
  return( adc_value * sensor_calibration_factor );
  
} // end SensorRead()



///////////////////////////////////////////////////////////
// sensor_calibration() uses a global var
///////////////////////////////////////////////////////////
int SensorCalibrate( void )
{
  // todo: measure the VCC or reference for the analog converter.
  // to make the measurement independent of the VCC
  // for now the factor is 1.

  sensor_calibration_factor = 1;

} // end SensorCalibrate()


///////////////////////////////////////////////////////////
void screen_up()
{
  time_stamp();
  Serial.print( "[Control] Screen up " );
  digitalWrite( PIN_RELAY_MAIN, LOW ); 
  Wait100msec( 600 );  // wait until screen is up
  digitalWrite( PIN_RELAY_MAIN, HIGH ); 
  Serial.println(" Ready"); 
} // end screen_up()

///////////////////////////////////////////////////////////
void screen_down()
{
  time_stamp();
  Serial.print( "[Control] SUN! Screen down "  );
  digitalWrite( PIN_RELAY_DIRECTION, LOW );
  Wait100msec( 2 ); 
  digitalWrite( PIN_RELAY_MAIN, LOW ); 
  Wait100msec( 600 );  // wait until screen is up
  digitalWrite( PIN_RELAY_MAIN, HIGH ); 
  digitalWrite( PIN_RELAY_DIRECTION, HIGH );
  Serial.println(" Ready" ); 
} // end screen_down()

///////////////////////////////////////////////////////////
void screen_stop()
{
  time_stamp();
  Serial.print( "[Control] Screen stop! " );
  digitalWrite( PIN_RELAY_MAIN, HIGH );
  Wait100msec( 2 ); 
  digitalWrite( PIN_RELAY_DIRECTION, HIGH );
  Serial.println(" Ready"); 
} // end screen_up()



///////////////////////////////////////////////////////////
// time_stamp()
// print a time stamp
///////////////////////////////////////////////////////////
static long timestamp;
///////////////////////////////////////////////////////////
void time_stamp( void )
{
  
  timestamp = millis() / (1000);  // 60000
  Serial.print( " " );
  Serial.print( timestamp );
  Serial.print( ": " );
} // time_stamp()



///////////////////////////////////////////////////////////
// WaitMinutes
///////////////////////////////////////////////////////////
void WaitMinutes( int WaitMinutes )
{
  int WaitTime = 0;
  
  do
  {
     WaitSeconds( 60 );
     WaitTime++;
  } 
  while( WaitTime <= WaitMinutes );
  
} // end WaitMinutes()


///////////////////////////////////////////////////////////
// WaitSeconds
///////////////////////////////////////////////////////////
void WaitSeconds( int WaitSeconds )
{
  int WaitTime  = 0;
  int IRcommand = 0;
  
  do
  {
     WaitTime++;
     digitalWrite(PIN_LED,   LOW);
     delay(800);     // 900 msec LED off   
     digitalWrite(PIN_LED,   HIGH);
     delay(200);     // 100 msec LED on
     LED_toggle();

  } 
  while( WaitTime <= WaitSeconds );
  
} // end WaitSeconds()


///////////////////////////////////////////////////////////
// Wait100msec()
// used by up, down and stop function.
// primary function is to flash the LED fast :)
// an IRremote signal up/down aborts the up/down movement
///////////////////////////////////////////////////////////
void Wait100msec( int Wait100msec )
{
  int WaitTime = 0;
  int IRcommand = 0;
  
  do
  {
     WaitTime++;
     digitalWrite(PIN_LED,   LOW);
     delay(90);     // 90 msec LED off
     digitalWrite(PIN_LED,   HIGH);
     delay(10);     // 10 msec LED on
     
     LED_toggle();
     
     // if manual command is up or down, only then stop the up or down action.
     IRcommand = CheckIRremote();
     if(( IRcommand == MANUAL_DOWN ) |
        ( IRcommand == MANUAL_UP ))
      {
        screen_stop();
        WaitTime = Wait100msec + 1;   // exit the while loop
      }
      
  } 
  while( WaitTime <= Wait100msec );
  
} // end Wait100msec()


//////////////////////////
// LED indicator functions
//////////////////////////
void LED_toggle( void )
{
  digitalWrite( PIN_LED_4, digitalRead( PIN_LED_4 ) ^ 1);
} // end LED_toggle()


void LED_ON( void )
{
  digitalWrite( PIN_LED_4, HIGH );
} // end LED_ON()

void LED_OFF( void )
{
  digitalWrite(PIN_LED_4, LOW);
} // end LED_ON()



// check the infrared remote control
////////////////////////////////////
long parse_value;

int CheckIRremote( void )
{
  if ( IRremote.decode( &IRresults ) ) 
  {
    time_stamp();
    Serial.print( "[IRremote] " );
     
    switch ( IRresults.decode_type )
    { 
      case NEC:     Serial.print( "NEC  " ); break;
      case SONY:    Serial.print( "SONY " ); break;
      case RC5:     Serial.print( "RC5  " ); break;
      case RC6:     Serial.print( "RC6  " ); break;
      case UNKNOWN: Serial.print( "IR?  " ); break;
    }
    
    parse_value = IRresults.value;
    Serial.print( "0x" );
    Serial.println( parse_value, HEX );
    

    if ( IRresults.decode_type == NEC )
    {   
      switch ( IRresults.value )
      { 
        case 0xFF22DD: 
          manual_command = MANUAL_UP;  
        break;
        case 0xFF02FD:
          manual_command = MANUAL_DOWN; 
        break;
        case 0xFFC23D:
          manual_command = MANUAL_STOP; 
        break;
        case 0xFFA25D:   
          manual_command = MANUAL_MODE_AUTOMATIC; 
        break;
        case 0xFFE21D:   
          manual_command = MANUAL_MODE_MANUAL; 
        break;
  
        default:      
          //Serial.println( IRresults.value, HEX); 
        break;
      } // switch
    } // end if NEC
    

    if ( IRresults.decode_type == RC5 )
    {
      
      switch ( IRresults.value )
      { 
        case 0x39:   
        case 0x839: 
          manual_command = MANUAL_UP;  
        break;
        case 0x38:
        case 0x838:   
          manual_command = MANUAL_DOWN; 
        break;
        case 0x407:
      //  case 0x407:   
          manual_command = MANUAL_STOP; 
        break;
        case 0xD:
        case 0x80D:   
          manual_command = MANUAL_MODE_AUTOMATIC; 
        break;
        case 0xC:
        case 0x80C:   
          manual_command = MANUAL_MODE_MANUAL; 
        break;
  
        default:      
          //Serial.println( IRresults.value, HEX); 
        break;
      } // switch
    } // end if RC5
    
    
    
    delay( 300 );
    IRremote.resume(); // Receive the next value

    return manual_command;
  }
  
  return 0;
  
} // end CheckIRremote()

 
    

