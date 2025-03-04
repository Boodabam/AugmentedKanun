//Code for Augmented Kanun Project
//Written by Alexandre Bestandji
//Last update 03/01/2024 -- Hardware pin swap
#include <SPI.h>
#include <SD.h>


//HW Sizes
const int N = 27;
const int Nbank = 5;
const int Nmodes = 8;
float strokeRate = 0.5; //strokeDistance/timeHigh in FaderGraduationUnit per ms //TODO to be measured

//Pins
const int clockPin = 0; //Same clock for DMUX 1 and 2
const int latchPin = 1; //Same latch for DMUX 1 and 2
const int dataPin1 = 2; //for DMUX 1 and 2
const int dataPin2 = 3 ; //for DMUX 3 and 4
const int pedalPin = 23;
const int writeButtonPin = 7;
const int bankButtonPin = 8;
const int motorEnablePins[N] = {22,24,26,28,30,32,34,36,38,40,42,44,46,48,27,29,31,33,35,37,39,41,43,45,47,49,25};
const int analogPins[4] = {A0,A1,A2,A3};
const int digPinsForAnalog[3] = {4,5,6};
const int lightPinsMode[Nmodes] = {14,15,16,17,18,19,20,21};
const int lightPinsBank[Nbank] = {10,9,11,12,13};

//Declare variables
int upper[N];
int lower[N];
int state = 0;
int bank = 0;
int analogRoughState[N]; //0=>1023
float analogState[N];
int pedalSave;
//float matrixModes[Nbank][Nmodes][N]={0};
int matrixModes[Nbank][Nmodes][N]={0};
bool matrixLights[Nbank][Nmodes]={0};
int count = 0;
int blinkingLedState = 0;

void setup(){
  Serial.begin(9600);
  delay(500);
  //for Timer blinking
  TCCR1A = 0;
  TCCR1B = 0;
  bitSet(TCCR1B, CS00);  //64 bit blinking // see ATMEGA1280_Datasheet p130
  bitSet(TCCR1B, CS01);  // change these two lines to CS02 (only) for 256 bit blinking
  bitSet(TIMSK1, TOIE1); // timer overflow interrupt

  //pinModes
  if (true){
    for (int i=0; i<N;i++){
      pinMode(motorEnablePins[i], OUTPUT);
    }
    for (int i=0; i<3;i++){
      pinMode(digPinsForAnalog[i],OUTPUT);
    }
    for (int i=0;i<Nmodes;i++){
      pinMode(lightPinsMode[i], OUTPUT);
      digitalWrite(lightPinsMode[bank],LOW);
    }
    for (int i=0;i<Nbank;i++){
      pinMode(lightPinsBank[i], OUTPUT);
      digitalWrite(lightPinsBank[bank],LOW);
    }
    pinMode(clockPin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    pinMode(dataPin1, OUTPUT);
    pinMode(dataPin2, OUTPUT);
    for (int i=0; i<4;i++){
      pinMode(analogPins[i], INPUT);
    }
    pinMode(writeButtonPin, INPUT);
    pinMode(bankButtonPin, INPUT);
    pinMode(pedalPin, INPUT_PULLUP);
  }
  //init
	state = 0;
	bank = 0;
  digitalWrite(lightPinsBank[bank],HIGH); //TODO verify if lights have to be setup
  pedalSave = digitalRead(pedalPin);
	matrixModes[Nbank][Nmodes][N]={0};
  matrixLights[Nbank][Nmodes]={0};
  analogReadAllRough();
}


void loop(){
  //Pedal pressed => Next mode
  //If current mode is active, searches for next active mode
  //If not next mode (active or not)
	if (digitalRead(pedalPin) != pedalSave){
    pedalSave = digitalRead(pedalPin);
    delay(20);
    //if current mode is active
    if (matrixLights[bank][state]){
      digitalWrite(lightPinsMode[state],HIGH);
      //find next active mode
      do{
        state = (state +1)%Nmodes;
      }while(!matrixLights[bank][state]);
      moveFaders(matrixModes[bank][state]);
    }else{
      //next mode
      digitalWrite(lightPinsMode[state],LOW);
      state = (state +1)%Nmodes;
      if (matrixLights[bank][state]){
        moveFaders(matrixModes[bank][state]);
      }
    }
	}
  //Bank button pressed => Next bank (first mode)
	if (digitalRead(bankButtonPin)){
    digitalWrite(lightPinsBank[bank],LOW);
    bank = (bank + 1)%Nbank;
    state = 0;
    //Update modes lights
		for (int i=0; i<Nmodes;i++){
      digitalWrite(lightPinsMode[i], matrixLights[bank][i]);
    }
    digitalWrite(lightPinsBank[bank],HIGH);
    if(matrixLights[bank][state]){
      moveFaders(matrixModes[bank][state]);
    }
    while(digitalRead(bankButtonPin)){};
	}
  // Write button pressed => Mode written
  // Write button pressed longer => Mode deleted
  // Pedal pressed while write pressed => Next (potentially non-active) mode (and no writing or deleting)
	if (digitalRead(writeButtonPin)){
    bool pass = 0 ; //0=write, 1=clear, 2+=pass
    count = 1;
    //While write button maintained
    while (digitalRead(writeButtonPin)){
      //if pedal is pressed => move state
      if (digitalRead(pedalPin) != pedalSave){
        pedalSave = digitalRead(pedalPin);
        delay(20);
        digitalWrite(lightPinsMode[state],matrixLights[bank][state]);
        state = (state +1)%Nmodes;
        //If the next mode is active
        if (matrixLights[bank][state]){
          moveFaders(matrixModes[bank][state]);
        }
        pass = 1;
      }
      //After 5 ticks if the pedal havent been pressed => Clear Mode
      if (count >= 5 and pass == 0){
        matrixLights[bank][state]=0;
        digitalWrite(lightPinsMode[state],LOW);
        blinkAll();
        pass = 1;
      }
    }
    count = 0;
    //write Mode
    if (pass == 0){
      matrixLights[bank][state]=1;
      //analogReadAll();
      analogReadAllRough();
      writeMode();
      blinkAll();
    }
    
	}
}

void serialEvent(){
	if (Serial.available()>0) {
		String str = Serial.readString();
    //calibration
		if (str == "calibrate"){
			Serial.println("Calibration");
			Serial.println("Ready for lower bound");
			//wait for pedal
			while (digitalRead(pedalPin) == pedalSave){}
      pedalSave = digitalRead(pedalPin);
      //write
      analogReadAllRough();
			//lower = analogRoughState;
      memcpy(lower, analogRoughState, N);
			Serial.println("Lower bound saved");
			Serial.println("Ready for upper bound");
			//wait for pedal
			while (digitalRead(pedalPin) == pedalSave){}
      pedalSave = digitalRead(pedalPin);
      analogReadAllRough();
			//upper = analogRoughState;
      memcpy(upper, analogRoughState, N);
			Serial.println("Upper bound saved");
      Serial.println("Succesfully calibrated");
		}
    //saving
    if (str.substring(0, 4)=="save"){//TODO change condition for driver control
      Serial.write(N);
      for (int i=0;i<Nbank;i++){
        for (int j=0;j<Nmodes;j++){
          Serial.write(matrixLights[i][j]);
          for (int k =0; k<N; k++){
            //Serial.write(roughToNorm(matrixModes[i][j][k],k));//TODO fix byte serial sending
            Serial.write(matrixModes[i][j][k]);
          }
        }
      }
    }
    //loading
    if (str.substring(0, 4)=="load"){//TODO change condition for driver control
      if (Serial.read()!=N){ 
        Serial.print("Error : Number of strings not matching");
      }else{
        for (int i=0;i<Nbank;i++){
          for (int j=0;j<Nmodes;j++){
            matrixLights[i][j] = Serial.read();
            for (int k =0; k<N; k++){
              matrixModes[i][j][k] = normToRough(Serial.read(),k);
            }
          }
        }
        //move to bank 0 state 0
        digitalWrite(lightPinsBank[bank],LOW);
        bank = 0;
        state = 0;
        //Update modes lights
        for (int i=0; i<Nmodes;i++){
          digitalWrite(lightPinsMode[i], matrixLights[bank][i]);
        }
        digitalWrite(lightPinsBank[bank],HIGH);
        if(matrixLights[bank][state]){
          moveFaders(matrixModes[bank][state]);
        }
      }
    }
	}
}

ISR(TIMER1_OVF_vect) {
  blinkingLedState = !blinkingLedState;
  digitalWrite(lightPinsMode[state], blinkingLedState);
  if (count){count++;} //Can improve time complexity by counting locally where needed OR add another timer
}

void writeMode(){
  for (int i=0; i<N;i++){
    //matrixModes[bank][state][i]=analogState[i];
    matrixModes[bank][state][i]=analogRoughState[i];
  }
}

//Improveable
void analogReadAllRough(){
  for (int i=0; i<N; i++){
    //which analog channel to read
    int whichMUX = i/8;
    //which value to encode //TODO: check if ABC = 421 or 124
    int val = i%8;
    //Set analogMUX
    int j=0;
    while (j<3){
      if (val%2==1){
        digitalWrite(digPinsForAnalog[j], HIGH);
      }else{
        digitalWrite(digPinsForAnalog[j], LOW);    
      }
      val = val/2;
      j++;
    }
    //Read value
    analogRoughState[i] = analogRead(analogPins[whichMUX]);
  }
}


float roughToNorm(int val, int fad){
  return (val-lower[fad])/(upper[fad]-lower[fad]);
}

int normToRough(float val, int fad){
  return round(val*(upper[fad]-lower[fad])+lower[fad]);
}

void analogReadAll(){
  analogReadAllRough();
  for (int i =0; i<N; i++){
    analogState[i]=roughToNorm(analogRoughState[i],i);
  }
}

void blinkAll(){
  //Repeat 3 times
  for (int j=0; j<3;j++){
    //all off
    for (int i=0;i<Nmodes;i++){
      if (matrixLights[bank][i]==1){
        digitalWrite(lightPinsMode[i],LOW);
      }
    }
    delay(100);
    //all on
    for (int i=0;i<Nmodes;i++){
      if (matrixLights[bank][i]==1){
        digitalWrite(lightPinsMode[i],HIGH);
      }
    } 
    delay(100);
  }
}

void moveFaders(int targetMode[N]){
  //analogReadAll()
  analogReadAllRough();
  int sigTime[N]={0};
  int bin = 0;
  //Update direction and calculate pulse time
  for (int i=0;i<N;i++){
    if (analogRoughState[i]<targetMode[i]){ //TODO verify direction (< or >)
      bin = bin + pow(2,i);
    }
    sigTime[i]=round(abs(analogRoughState[i]-targetMode[i])/strokeRate);
  }
  registerWrite(bin);
  pulseAllAtOnce(sigTime);
  //pulseSuccessive(sigTime);
}

void pulseSuccessive(int times[N]){
  for (int i=0; i<N; i++){
    digitalWrite(motorEnablePins[i], HIGH);
    delay(times[i]);
    digitalWrite(motorEnablePins[i], HIGH);
  }
}

void pulseAllAtOnce(int times[N]){
  int indx[N];
  for (int i=0;i<N;i++){
    indx[i]=i;
  }
  //insertion sort
  int i = 1;
  int j;
  int t;
  int ti;
  while (i<N){
    j=i;
    while (j>0 and times[j-1]>times[j]){
      t=times[j];
      ti=indx[j];
      times[j]=times[j-1];
      indx[j]=indx[j-1];
      times[j-1]=t;
      indx[j-1]=ti;
      j--;
    }
    i++;
  }
  //moving
  int highTime=times[0];
  //all ON
  for (int j=0; j<N; j++){
    digitalWrite(motorEnablePins[indx[j]], HIGH);
  }
  //OFF one by one
  delay(highTime);
  digitalWrite(motorEnablePins[indx[0]], LOW);
  for (int i=1; i<N;i++){
    highTime = times[i]-times[i-1];
    delay(highTime);
    digitalWrite(motorEnablePins[indx[i]], LOW);
  }
}

//Write value for all registers
void registerWrite(int bitToWrite) {
  //first division 2**16
  int rest16bit = bitToWrite%65536;
  int div16bit = bitToWrite/65536; 
  //Shifts 2nd and 4th registers
  shiftOut(dataPin2, clockPin, MSBFIRST, div16bit/256);
  shiftOut(dataPin1, clockPin, MSBFIRST, rest16bit/256);
  digitalWrite(latchPin, LOW);
  //Shifts 1st and 3rd registers
  shiftOut(dataPin2, clockPin, MSBFIRST, div16bit%256);
  shiftOut(dataPin1, clockPin, MSBFIRST, rest16bit%256);
  digitalWrite(latchPin, HIGH);
}