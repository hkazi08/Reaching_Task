/* Dispense water statically to either the left or right solenoid at random */

#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_Soundboard.h"
#include "Adafruit_MPR121.h"


//For touch sensor
#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

/* Pins */
#define R_SOLENOID 31
#define BEAM 2
#define R_LED 7
#define L_SOLENOID 32
#define L_LED 8
#define RANDOM_SEED_PIN 14     //don't use pin 14 for anything since its seeding the random generator
#define SXT_RST 21
#define FLUSH_BUTTON 27
#define VAC 29

/* Thresholds */
#define maxTrials 450
#define resting_delay_min 1
#define resting_delay_max 6
#define resting_threshold 925

/* Macros */
#define currMillis millis() - prevMillis
#define pinMap(x) ((x) + trial_array[trialNum])
#define trueTrialNum ((maxTrials*numResets) + trialNum)

unsigned long prevMillis;
unsigned long retrievalTimeStamp; //time btwn LED/sound signal and beam break (how long it took to grab water)
int trialNum = 0;
int numResets = 0;
bool beam_broken = false;

/* Edit the following variable to change solenoid behavior */
unsigned int solenoid_open_time = 25;
//25 ms will produce a 2 uL drop of water

/* Edit the following variables to change timeouts and delays (ms) */
unsigned int retrieval_timeout = 20000;                               //How long the mouse has to grab the water drop in ms

/* States */
enum state {initialize, resting_state, retrieval_state, success, fail};
enum state current_state;


/* Random Variables */
unsigned int resting_delay;
byte trial_array[maxTrials];                                            //Array of 1s and 0s, 1 -> Left solenoid, 0-> Right solenoid

//Sound board
Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial1, NULL, SFX_RST);

//Touch sensor
Adafruit_MPR121 cap = Adafruit_MPR121();
uint16_t currtouched = 0;

void setup() {

  Serial1.begin(9600);
  Serial.begin(115200);
  while(!Serial)
  {
    safeDelay(10);
  }
  
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found");
    exit(1);
  }
  
  pinMode(REST_BAR, INPUT_PULLUP);
  pinMode(R_BEAM, INPUT);
  digitalWrite(R_BEAM, HIGH);
  pinMode(L_BEAM, INPUT);
  digitalWrite(L_BEAM, HIGH);
  pinMode(L_SOLENOID, OUTPUT);
  pinMode(R_SOLENOID, OUTPUT);
  pinMode(R_LED, OUTPUT);
  pinMode(L_LED, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  current_state = initialize;
  randomSeed(analogRead(RANDOM_SEED_PIN));
  
  for(int i=0; i<maxTrials; i++)
  {
    trial_array[i] = random(2);
  }
  
  attachInterrupt(digitalPinToInterrupt(FLUSH_BUTTON), solenoid_flush_ISR, CHANGE);

}

void loop() {
  
  switch(current_state){
    
    case initialize:
      initializer();
      current_state = resting_state;
      break;
    
    case resting_state:
      rest() == true ? current_state = retrieval_state : current_state = resting_state;
      break;
    
    case retrieval_state:
      dispense_water();
      signal_mouse();
      retrieval() == true ? current_state = success : current_state = fail;
      break;
    
    case success:
      print_row(1);
      trialNum += 1;
      current_state = initialize;
      break;
    
    case fail:
      withdraw_drop();
      print_row(0);
      trialNum += 1;
      current_state = initialize;
      break;
  }
  
}


//Delay helper function. Use this instead of delay(x).
void safeDelay(unsigned int milliseconds)
{
  prevMillis = millis();
  while((currMillis - prevMillis) <= milliseconds)
  {
    continue;
  }
  return;
}


//Initialize for next trial
void initializer()
{
  
  resting_delay = random(resting_delay_min, resting_delay_max) * 1000;
  digitalWrite(R_LED, LOW);
  digitalWrite(L_LED, LOW);

  if(trialNum > maxTrials-1)
  {
    for(int i=0; i<maxTrials; i++)
    {
      trial_array[i] = random(2);
    }
    numResets += 1;
    trialNum = 0;
  }
  return;
}

//Wait until the mouse enters resting position (touching both pads). Then ensure resting position is maintained for x amount of time. If they let go, return false and retry. Otherwise, return true and proceed.
bool rest()
{
  
  //Await resting position
  while(1)
  {
    currtouched = cap.touched();
    if((currtouched & _BV(6)) && (currtouched & _BV(8)))
    {
      break;
    }
    safeDelay(10);
  }
  
  //Ensure resting position is maintained for resting_delay period.
  prevMillis = millis();
  while(currMillis <= resting_delay)
  {
    //Checking 4 times per second
    safeDelay(250);
    
    currtouched = cap.touched();
    if((currtouched & _BV(6)) && (currtouched & _BV(8)))
    {
      continue;
    }
    else
    {
      return false;
    }
  }
  
  return true;
}


void dispense_water()
{
  digitalWrite(pinMap(R_SOLENOID), HIGH);
  prevMillis = millis();
  while(currMillis <= solenoid_open_time) {}
  digitalWrite(pinMap(R_SOLENOID), LOW);
  return;
}

//Sound & LED cue function
void signal_mouse()
{
  digitalWrite(pinMap(R_LED), HIGH);
  sfx.playTrack("T02     OGG");
  return;
}

//Monitor beam to see if it breaks after the reach cue is given
bool retrieval()
{
  prevMillis = millis();
  while(currMillis <= retrieval_timeout)
  {
    beam_broken = !(digitalRead(BEAM));
    if(beam_broken)
    {
      retrievalTimeStamp = currMillis;
      digitalWrite(pinMap(R_LED), LOW);
      return true;
    }
  }
  digitalWrite(pinMap(R_LED), LOW);
  return false;
}

//Print the row of data associated with a completed trial
//Trial Number, Success/Fail, Left/Right, Rest bar delay, Retireval time
void print_row(int result)
{
  Serial.print(trueTrialNum);
  Serial.print(",");
  if(result == 1)
  {
    Serial.print("success,");
  }
  else
  {
    Serial.print("fail,");
  }


  if(trial_array[trialNum]==1)
  {
    Serial.print("left,");
  }
  else
  {
    Serial.print("right,");
  }

  Serial.print(resting_delay);
  Serial.print(",");
  Serial.print(retrievalTimeStamp);
  Serial.println(";");
  
  return;
}

//Open vacuum line solenoid to revoke water drop
void withdraw_drop()
{
  digitalWrite(VAC, HIGH);
  safeDelay(2000);
  digitalWrite(VAC, LOW);
  return;
}


//Button down => Solenoid OPEN
//Hold down to flush solenoid tubing in case of airbubbles or  tap it once to manually dispense a single drop (irrespective of where you are in the program itself)
void solenoid_flush_ISR()
{
  if(digitalRead(FLUSH_BUTTON)==HIGH)
  {
    open_solenoid_ISR();
  }
  else
  {
    close_solenoid_ISR();
  }
  
}

void open_solenoid_ISR()
{
  digitalWrite(R_SOLENOID, HIGH);
  safeDelay(10);
  digitalWrite(L_SOLENOID, HIGH);
  return;
}

void close_solenoid_ISR()
{
  digitalWrite(R_SOLENOID, LOW);
  safeDelay(10);
  digitalWrite(L_SOLENOID, LOW);
  return;
}
