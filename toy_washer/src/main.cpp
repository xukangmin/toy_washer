/*
Rotary Encoder - Polling Example

The circuit:
* encoder pin A to Arduino pin 2
* encoder pin B to Arduino pin 3
* encoder ground pin to ground (GND)
*/

#include "Adafruit_ZeroI2S.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ezButton.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32


#define SAMPLERATE_HZ 44100  // The sample rate of the audio.  Higher sample rates have better fidelity,
                             // but these tones are so simple it won't make a difference.  44.1khz is
                             // standard CD quality sound.

#define AMPLITUDE     ((1<<29)-1)   // Set the amplitude of generated waveforms.  This controls how loud
                             // the signals are, and can be any value from 0 to 2**31 - 1.  Start with
                             // a low value to prevent damaging speakers!

#define WAV_SIZE      256    // The size of each generated waveform.  The larger the size the higher
                             // quality the signal.  A size of 256 is more than enough for these simple
                             // waveforms.


#define PIN_A A0
#define PIN_B A1
#define PIN_SW A2



#define BTN_PIN_GREEN  10
#define BTN_PIN_BLUE   11
#define BTN_PIN_RED    12


const int SHORT_PRESS_TIME = 1000; // 1000 milliseconds
const int LONG_PRESS_TIME  = 1000; // 1000 milliseconds

#define PIN_MOTOR_CTRL_A 5
#define PIN_MOTOR_CTRL_B 6

#define PIN_MOTOR_ENABLE A3


// Define the frequency of music notes (from http://www.phy.mtu.edu/~suits/notefreqs.html):
#define C4_HZ      261.63
#define D4_HZ      293.66
#define E4_HZ      329.63
#define F4_HZ      349.23
#define G4_HZ      392.00
#define A4_HZ      440.00
#define B4_HZ      493.88


#define C6_HZ   1046.502

#define C5S_HZ 554
#define A4_HZ  440
#define F4S_HZ 370
#define B4_HZ 494

#define B5_HZ 988

#include "song.h"

float scale[] = { C4_HZ, D4_HZ, E4_HZ, F4_HZ, G4_HZ, A4_HZ, B4_HZ, A4_HZ, G4_HZ, F4_HZ, E4_HZ, D4_HZ, C4_HZ };

unsigned long lastButtonPress = 0;

#include <MD_REncoder.h>

// set up encoder object
MD_REncoder R = MD_REncoder(PIN_A, PIN_B);

Adafruit_ZeroI2S i2s;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

ezButton btn_encoder(PIN_SW);

int32_t square[WAV_SIZE]   = {0};

uint8_t state = 0;


unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

unsigned long start_time = 0;
unsigned long current_time = 0;
unsigned long elapsed_time = 0;

unsigned long time_counter = 0;
unsigned long time_left = 0;
unsigned long rgb_counter = 0;

int current_motor_speed = 0;
int current_motor_dir = 0;

unsigned long current_red_value = 0;
unsigned long current_blue_value = 0;
unsigned long current_green_value = 0;

int current_rgb_dir = 0;

long current_rgb_ch = 0;

int current_note = 0;
int current_max_note = 0;
int current_whole_note = 0;
const int* current_song_arr;

enum MachineStatus
{ 
  Idle, 
  Running,
  Pause,
  Playing
};


enum MachineMode
{
   Normal = 0,
   Delicate,
   Heavy,
   Fun,
   Song
};

#define MAX_MACHINE_MODE 5
#define MAX_SONG_LIST 9

enum Clip{
  MStart,
  MStop,
  MPause,
  MResume,
  MUp,
  MDown
};

enum SongList{
  HappyBday = 0,
  FurElise,
  Mario,
  Cannon,
  GreenSle,
  OdeToJoy,
  StarWar,
  Tetris,
  HarryPotter
};

const String SongListString[] = {
  "HappyBday",
  "FurElise",
  "Mario",
  "Cannon",
  "GreenSle",
  "OdeToJoy",
  "StarWar",
  "Tetris",
  "HarryPot",
};

const int* const SongArrList[] = {
  HappyBDay_notes,
  fur_elise_notes,
  mario_notes,
  Cannon_notes,
  Greensleeves_notes,
  odetojoy_notes,
  starwar_notes,
  tetris_notes,
  harrypotter_notes
};

const int SongListMaxSize[] = {
  sizeof(HappyBDay_notes),
  sizeof(fur_elise_notes),
  sizeof(mario_notes),
  sizeof(Cannon_notes),
  sizeof(Greensleeves_notes),
  sizeof(odetojoy_notes),
  sizeof(starwar_notes),
  sizeof(tetris_notes),
  sizeof(harrypotter_notes)
};

const int SongListTempo[] = {
  80,
  80,
  200,
  100,
  70,
  114,
  108,
  144,
  144
};


enum MenuLevel{
  Main,
  Music
};

MachineStatus prev_washer_state = Idle;
MachineStatus washer_state = Idle;
MenuLevel menu_level = Main;
MachineMode main_mode = Normal;

Clip current_clip = MStart;
SongList current_song = HappyBday;

void update_screen();
void display_single_line_text(String text);

void generateSquare(int32_t amplitude, int32_t* buffer, uint16_t length) {
  // Generate a square wave signal with the provided amplitude and store it in
  // the provided buffer of size length.
  for (int i=0; i<length/2; ++i) {
    buffer[i] = -(amplitude/2);
  }
    for (int i=length/2; i<length; ++i) {
    buffer[i] = (amplitude/2);
  }
}

void playWave(int32_t* buffer, uint16_t length, float frequency, float seconds) {
  // Play back the provided waveform buffer for the specified
  // amount of seconds.
  // First calculate how many samples need to play back to run
  // for the desired amount of seconds.
  uint32_t iterations = seconds*SAMPLERATE_HZ;
  // Then calculate the 'speed' at which we move through the wave
  // buffer based on the frequency of the tone being played.
  float delta = (frequency*length)/float(SAMPLERATE_HZ);
  // Now loop through all the samples and play them, calculating the
  // position within the wave buffer for each moment in time.
  for (uint32_t i=0; i<iterations; ++i) {
    uint16_t pos = uint32_t(i*delta) % length;
    int32_t sample = buffer[pos];
    // Duplicate the sample so it's sent to both the left and right channel.
    // It appears the order is right channel, left channel if you want to write
    // stereo sound.
    i2s.write(sample, sample);
  }
}

void splash_screen(void) {
  
  display_single_line_text("M");
  delay(500);
  display_single_line_text("Mi");
  delay(500);
  display_single_line_text("Min");
  delay(500);
  display_single_line_text("Mini");
  delay(500);
  display_single_line_text("MiniW");
  delay(500);
  display_single_line_text("MiniWa");
  delay(500);
  display_single_line_text("MiniWas");
  delay(500);
  display_single_line_text("MiniWash");
  delay(500);
  display_single_line_text("MiniWashe");
  delay(500);
  display_single_line_text("MiniWasher");
  delay(1000);
  update_screen();
}

void display_two_line_text(String title, String text)
{
   display.clearDisplay();

  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner

  display.println(title);
   
  display.setTextSize(2);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,10);             // Start at top-left corner
  
   display.println(text);
   display.display();
}

void display_single_line_text(String text)
{
  display.clearDisplay();
  display.setTextSize(2);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,10);             // Start at top-left corner
  
   display.println(text);
   display.display();
}

void setup() 
{
  pinMode(PIN_SW, INPUT_PULLUP);

  btn_encoder.setDebounceTime(50);

  Serial.begin(9600);
  R.begin();
  pinMode(PIN_MOTOR_CTRL_A, OUTPUT);
  pinMode(PIN_MOTOR_CTRL_B, OUTPUT);
  pinMode(PIN_MOTOR_ENABLE, OUTPUT);
  
  

  
   // Initialize the I2S transmitter.
  if (!i2s.begin(I2S_32_BIT, SAMPLERATE_HZ)) {
    Serial.println("Failed to initialize I2S transmitter!");
    while (1);
  }
  i2s.enableTx();

  analogWrite(BTN_PIN_GREEN, 0);
  analogWrite(BTN_PIN_BLUE, 255);
  analogWrite(BTN_PIN_RED, 255);

  generateSquare(AMPLITUDE, square, WAV_SIZE);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  digitalWrite(PIN_MOTOR_CTRL_A, LOW);
  digitalWrite(PIN_MOTOR_CTRL_B, LOW);
  digitalWrite(PIN_MOTOR_ENABLE, LOW);
  
  splash_screen();

  
  

  
  

    
  

}

void play_clip(Clip clip_name)
{
    switch(clip_name)
  {
    case MUp:
      playWave(square, WAV_SIZE, B5_HZ, 0.05);    
      break;
    case MDown:
      playWave(square, WAV_SIZE, C6_HZ, 0.05);
      break;
    case MStart:
      playWave(square, WAV_SIZE, C5S_HZ, 0.3);
      
      playWave(square, WAV_SIZE, A4_HZ, 0.15);
      
      playWave(square, WAV_SIZE, F4S_HZ, 0.15);
      
      playWave(square, WAV_SIZE, B4_HZ, 0.15);
      
      playWave(square, WAV_SIZE, A4_HZ, 0.15);
      
      playWave(square, WAV_SIZE, C5S_HZ, 0.15);
      break;
    case MStop:
      playWave(square, WAV_SIZE, C5S_HZ, 0.15);
      
      playWave(square, WAV_SIZE, A4_HZ, 0.15);

      playWave(square, WAV_SIZE, B4_HZ, 0.15);

      playWave(square, WAV_SIZE, F4S_HZ, 0.15);

      playWave(square, WAV_SIZE, A4_HZ, 0.15);
      
      playWave(square, WAV_SIZE, C5S_HZ, 0.3);
      
      break;
    case MPause:
      playWave(square, WAV_SIZE, C5S_HZ, 0.3);

      playWave(square, WAV_SIZE, B4_HZ, 0.15);
      break;
    case MResume:
      playWave(square, WAV_SIZE, B4_HZ, 0.15);

      playWave(square, WAV_SIZE, C5S_HZ, 0.3);
      break;
  }
}

void play_song(SongList song_name)
{
  washer_state = Playing;
  current_note = 0;

  int index_song = (int)song_name;
  current_whole_note = (60000 * 4) / SongListTempo[index_song];
  current_song_arr = SongArrList[index_song];
  current_max_note = SongListMaxSize[index_song] / sizeof(current_song_arr[0]);

}

void update_screen()
{
  if (menu_level == Main)
   {
      switch(main_mode)
      {
        case Normal:
          display_two_line_text("Main Menu", ">Normal");
          break;
        case Delicate:
          display_two_line_text("Main Menu", ">Delicate");
          break;
        case Heavy:
          display_two_line_text("Main Menu", ">Heavy");
          break;
        case Fun:
          display_two_line_text("Main Menu", ">Fun");
          break;
        case Song:
          display_two_line_text("Main Menu", ">Song");
          break;
      }
   }
   else if (menu_level == Music)
   {
      int index_song = (int)current_song;

      
      

      display_two_line_text("Main Menu-Song List", ">" + SongListString[index_song]);

      // switch(current_song)
      // {
      //   case FurElise:
      //     display_two_line_text("Main Menu-Song List", ">FurElise");
      //     break;
      //   case Mario:
      //     display_two_line_text("Main Menu-Song List", ">Mario");
      //     break;
      // }
   }
}




void motor_cmd(int speed, int dir)
{
    if (speed == 0)
    {
        digitalWrite(PIN_MOTOR_ENABLE, LOW);
        digitalWrite(PIN_MOTOR_CTRL_A, LOW);
        digitalWrite(PIN_MOTOR_CTRL_B, LOW);
    }
    else
    { 
      if (dir == 0)
      {
          digitalWrite(PIN_MOTOR_ENABLE, HIGH);
          digitalWrite(PIN_MOTOR_CTRL_A, LOW);
          analogWrite(PIN_MOTOR_CTRL_B, speed);
      }
      else 
      {
          digitalWrite(PIN_MOTOR_ENABLE, HIGH);
          digitalWrite(PIN_MOTOR_CTRL_B, LOW);
          analogWrite(PIN_MOTOR_CTRL_A, speed);
      }
      

    }
}

void start_running(MachineMode mode)
{
  washer_state = Running;
  rgb_counter = 0;
  // all off
  current_green_value = 255;
  current_red_value = 255;
  current_blue_value = 255;
  analogWrite(BTN_PIN_GREEN,current_green_value);
  analogWrite(BTN_PIN_RED,current_red_value);
  analogWrite(BTN_PIN_BLUE,current_blue_value);
  current_rgb_ch = random(0,2);
  switch(mode)
  {
    case Normal:
      // Run for 20 seconds with 
      display_single_line_text("Washing");
      play_clip(MStart);
      start_time = millis();
      time_left = 30;
      time_counter = 1;
      current_motor_speed = 200;
      current_motor_dir = 0;
      motor_cmd(current_motor_speed,current_motor_dir);

      break;
    case Delicate:
      display_single_line_text(">Delicate");
      play_clip(MStart);
      start_time = millis();
      time_left = 20;
      time_counter = 1;
      current_motor_speed = 80;
      current_motor_dir = 0;
      motor_cmd(current_motor_speed,current_motor_dir);
      break;
    case Heavy:
      display_single_line_text(">Heavy");
      play_clip(MStart);
      start_time = millis();
      time_left = 60;
      time_counter = 1;
      current_motor_speed = 255;
      current_motor_dir = 0;
      motor_cmd(current_motor_speed,current_motor_dir);
      break;
    case Fun:
      display_single_line_text(">Fun");
      play_clip(MStart);
      start_time = millis();
      time_left = 120;
      time_counter = 1;
      current_motor_speed = 255;
      current_motor_dir = 0;
      motor_cmd(current_motor_speed,current_motor_dir);
      break;
    case Song:
      display_single_line_text(">Song");
      break;
  }
}

void stop_running()
{
  if (washer_state == Running || washer_state == Playing || washer_state == Pause)
  {
    current_motor_speed = 0;
    current_motor_dir = 0;
    motor_cmd(current_motor_speed,current_motor_dir);
    
    play_clip(MStop);
    display_single_line_text("Finished");
    delay(1000);
    washer_state = Idle;
    update_screen();
  }

  analogWrite(BTN_PIN_GREEN, 0);
  analogWrite(BTN_PIN_BLUE, 255);
  analogWrite(BTN_PIN_RED, 255);

}

void pasue_running()
{
  if (washer_state == Running)
  {
    
    motor_cmd(0,0);
    prev_washer_state = washer_state;
    washer_state = Pause;
    play_clip(MPause);
    display_single_line_text("Pause!");
  }
  else if (washer_state == Playing)
  {
    play_clip(MPause);
    prev_washer_state = washer_state;
    washer_state = Pause;
    display_single_line_text("Pause!");
  }

}

void resume_running()
{
 
  if (washer_state == Pause)
  {

    if (prev_washer_state == Running)
    {
      
      play_clip(MResume);
      washer_state = Running;
      motor_cmd(current_motor_speed,current_motor_dir);
      display_single_line_text("Resume!");
    }
    else if (prev_washer_state == Playing)
    {
      
      washer_state = Playing;
      play_clip(MResume);
      display_single_line_text("Resume!");
    }

  }
}

void running_update()
{   
    float divider = 0, noteDuration = 0, current_freq = 0;

    String display_test = "";
    switch (washer_state)
    {
      case Running:
        current_time = millis();

        elapsed_time = current_time - start_time;

        if (elapsed_time > time_counter * 1000)
        {
           // 1 second pass
           time_counter++;
           time_left--;
           display_test = String(time_left) + "s Left";
           display_single_line_text(display_test);

           if (main_mode == Heavy && time_counter % 3 == 0)
           {
             
             // change dir

             // change speed
             if (current_motor_speed >= 200)
             {
               current_motor_speed = 0;
             }
             else if (current_motor_speed == 0)
             {
               current_motor_speed = 255;
             }
          
             motor_cmd(current_motor_speed, current_motor_dir);
           }
        }

        if (time_left == 0)
        {
            stop_running();
        } 

        if (elapsed_time > rgb_counter * 100)
        {
          // RGB trigger every 100 ms
          if (current_rgb_dir == 0)
          {
            current_green_value -= 20;
            if (current_green_value < 100) 
            {
              current_rgb_dir = 1;
            }
          }
          else if (current_rgb_dir == 1)
          {
            current_green_value += 20;
            if (current_green_value >= 255)
            {
              current_rgb_dir = 0;
            }
          }
//          analogWrite(BTN_PIN_BLUE, current_green_value);
          switch (current_rgb_ch)
          {
          case 0:
            analogWrite(BTN_PIN_RED, current_green_value);
            break;
          case 1:
            analogWrite(BTN_PIN_BLUE, current_green_value);
            break;
          case 2:
            analogWrite(BTN_PIN_GREEN, current_green_value);
            break;
          }
          
          
          
          rgb_counter++;
        }
        
        break;   
      case Playing:

        if (current_note < current_max_note)
        {
          divider = pgm_read_word_near(current_song_arr + current_note + 1);
          if (divider > 0)
          {
            noteDuration = (current_whole_note) / divider;
          }
          else if (divider < 0) {
            noteDuration = (current_whole_note) / abs(divider);
            noteDuration *= 1.5;
          }

          noteDuration = noteDuration / 1000;
          
          current_freq = pgm_read_word_near(current_song_arr + current_note);
          

          playWave(square, WAV_SIZE, current_freq, noteDuration);

        }
        else
        {
          stop_running();
        }

        //playWave();

        current_note += 2;

        break; 
    }
    
}




void CW()
{
  if (washer_state == Idle)
  {
    
    if (menu_level == Music)
    {
      play_clip(MUp);
      int mm = (int)current_song;

      mm++;
      if (mm  == MAX_SONG_LIST)
      {
        mm = 0;
      }

      current_song = (SongList)mm;
    }
    else if (menu_level == Main)
    {
      play_clip(MUp);
      int mm = (int)main_mode;

      mm++;
      
      if (mm == MAX_MACHINE_MODE)
      {
        mm = 0;
      }

      main_mode = (MachineMode)mm;
    }
    


    
    update_screen();
  }
  else if (washer_state == Running)
  {
    if (main_mode == Fun)
    {
      current_motor_speed += 20;
      if (current_motor_speed > 255 )
      {
        current_motor_speed = 255;
      }
      
      
      motor_cmd(current_motor_speed, current_motor_dir);
    }
  }

}


void CCW()
{
  if (washer_state == Idle)
  {
    if (menu_level == Music)
    {
      play_clip(MDown);
      int mm = (int)current_song;

      mm--;
      if (mm < 0)
      {
        mm = MAX_SONG_LIST - 1;
      }

      current_song = (SongList)mm;
    }
    else if (menu_level == Main)
    {
      play_clip(MDown);
      int mm = (int)main_mode;

      mm--;
      if (mm < 0)
      {
        mm = MAX_MACHINE_MODE - 1;
      }

      main_mode = (MachineMode)mm;
    }
    

    
    update_screen();
  }
  else if (washer_state == Running)
  {
    if (main_mode == Fun)
    {
      current_motor_speed -= 10;
      if (current_motor_speed < 0 )
      {
        current_motor_speed = 0;
      }
      
      
      motor_cmd(current_motor_speed, current_motor_dir);
    }
  }
}



void loop() 
{
  uint8_t x = R.read();
  
  if (x) 
  {
    Serial.print(x == DIR_CW ? "\n+1" : "\n-1");
#if ENABLE_SPEED
    Serial.print("  ");
    Serial.print(R.speed());
#endif

    if (x == DIR_CW)
    {
      CW();
    }
    else if (x == DIR_CCW)
    {
      CCW();
    }
  }

  btn_encoder.loop();

  
  //int btnState = btn_encoder.getState();
  // Serial.println(btnState);

  if(btn_encoder.isPressed())
    pressedTime = millis();

  if(btn_encoder.isReleased()) {
    releasedTime = millis();

    long pressDuration = releasedTime - pressedTime;

    if( pressDuration < SHORT_PRESS_TIME )
    {
    // short press 
      Serial.println("A short press is detected");
      if (washer_state == Idle)
      {
        if (menu_level == Main)
        {
            if (main_mode == Song)
            {
              play_clip(MUp);
              menu_level = Music;
              update_screen();
            }
            else
            {
              start_running(main_mode);
            }
            
            
        }
        else if (menu_level == Music)
        {
            play_song(current_song);
            washer_state = Playing;
        }
      }
      else if (washer_state == Running || washer_state == Playing)
      {
          // Pause
          
          pasue_running();
      }
      else if (washer_state == Pause)
      {
          
          resume_running();
      }
    }


    if( pressDuration > LONG_PRESS_TIME )
    {
      // Stop
      Serial.println("A long press is detected");

      if (washer_state == Idle)
      {
          // do nothing if IDLE
          if (menu_level == Music)
          {
            play_clip(MDown);
            menu_level = Main;
            update_screen();
          }
      }
      else if (washer_state == Running || washer_state == Playing || washer_state == Pause)
      {
        stop_running();
      }
    }
      
  }

  running_update();
}
