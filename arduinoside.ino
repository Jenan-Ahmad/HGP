//libraries
#include <AccelStepper.h> //library for stepper motor
#include <Servo.h> //library for servo motor

//Pins
//serial
#define RXp2 16
#define TXp2 17
//servo motor
const int servoPin = 2;
//needle's stepper motor
const int nstepPin = 3; 
const int ndirPin = 4; 
//200 steps -> 6cm
//syringe's stepper motor
const int sstepPin = 5; 
const int sdirPin = 6; 
//disk's stepper motor
const int dstepPin = 9; 
const int ddirPin = 10; 
//valve
const int valvaPin = 7;
//frequency
const int freqPin = 8;
//needle's optocouplers
const int nhPin = 11;
const int nlPin = 12;
//disk's optocouplers
const int dPin = 13;
//syringe's optocouplers
const int shPin = 31;
const int slPin = 32;

enum State{

  initialize,//->move needle up, move servo to cleaning side, turn on valve, draw in distilled water turn off valve, draw out distilled water
  find_next,//find if a test is scheduled, if not wait until a test is scheduled or a start test is fired
  stopped,
  terminate,
  sense,
  in_liquid,
  draw_in,
  draw_out,
  next_tube,
  sense_failed
  
};

//variables
char disk[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};//if a slot is empty name it '0'
int samples[] = {0, 0};
int rcups[] = {0, 0, 0, 0};
int tubenumber = 0;//0->sample, (>0)-> reagent
float x[] = {0, 0, 0.4, 0, 0, 0, 0, 0};
float y[] = {0, 0.8, 0, 0.3, 0, 0, 0, 0};
float z[] = {0, 0, 0, 0.4, 0, 0.3, 0, 0};
int status = 0;//0->no test running, 1->test running, 2->paused
int nstests = 0;//number of scheduled tests
float ssteps;//syringe steps
float volume = 0;
float h;
float nsteps;
float pl;
int dindx;//desk index
int erroccurred = 0;//error occurred
//testnum/samplenum/reactioncupnum
int test1[] = {0, 0, 0};
int test2[] = {0, 0, 0};
int test3[] = {0, 0, 0};
int test4[] = {0, 0, 0};
Servo servo;  // servo motor object for its control
int pulseHigh; // Integer variable to capture High time of the incoming pulse
int pulseLow; // Integer variable to capture Low time of the incoming pulse
float pulseTotal; // Float variable to capture Total time of the incoming pulse
float frequency; // Calculated Frequency
State state;//state of the system
State prestop;
AccelStepper nstepper(AccelStepper::DRIVER, nstepPin, ndirPin);
AccelStepper sstepper(AccelStepper::DRIVER, sstepPin, sdirPin);
AccelStepper dstepper(AccelStepper::DRIVER, dstepPin, ddirPin);

//Functions
//Cleaning
void cleaning(){
  servo.write(179);
  delay(1000);
  digitalWrite(ndirPin, LOW);  
  for (int i = 0; i < 1500; i++) {
    digitalWrite(nstepPin, HIGH);
    delayMicroseconds(1000); 
    digitalWrite(nstepPin, LOW);
    delayMicroseconds(1000);  
  }
  digitalWrite(sdirPin, LOW);  
  for (int i = 0; i < 100000; i++) {
    digitalWrite(sstepPin, HIGH);
    delayMicroseconds(1000);  
    digitalWrite(sstepPin, LOW);
    delayMicroseconds(1000); 
    if(digitalRead(shPin) == HIGH){
      break;
    }
  }
  digitalWrite(valvaPin, HIGH);
  digitalWrite(sdirPin, HIGH);  
  for (int i = 0; i < 100000; i++) {
    digitalWrite(sstepPin, HIGH);
    delayMicroseconds(1000);  
    digitalWrite(sstepPin, LOW);
    delayMicroseconds(1000); 
    if(digitalRead(slPin) == HIGH){
      break;
    }
  }
  digitalWrite(valvaPin, LOW);
  digitalWrite(sdirPin, LOW);  
  for (int i = 0; i < 100000; i++) {
    digitalWrite(sstepPin, HIGH);
    delayMicroseconds(1000);  
    digitalWrite(sstepPin, LOW);
    delayMicroseconds(1000); 
    if(digitalRead(shPin) == HIGH){
      break;
    }
  }
  nshome();
}
//initialization
void initialization(){
  nshome();
  delay(1000);
  servo.attach(servoPin);  
  servo.write(180);
  cleaning();
  // dshome();
}
//move needle's stepper motor to home
void nshome(){
  digitalWrite(ndirPin, HIGH);  
  for (int i = 0; i < 1000000; i++) {
    digitalWrite(nstepPin, HIGH);
    delayMicroseconds(1000); 
    digitalWrite(nstepPin, LOW);
    delayMicroseconds(1000);  
    if(digitalRead(nhPin) == HIGH){
      return;
    }
  }
}
//move disk's stepper to home
void dshome(){
  digitalWrite(ddirPin, LOW);
 
  for (int i = 0; i < 20; i++) {
    digitalWrite(dstepPin, HIGH); // Set the step pin high
    delay(10); // Adjust the delay for the desired speed
    digitalWrite(dstepPin, LOW); // Set the step pin low
    delay(10); // Adjust the delay for the desired speed
  }
  delay(500);
  digitalWrite(ddirPin, HIGH);
  
  for (int i = 0; i < 300; i++) {
    digitalWrite(dstepPin, HIGH); // Set the step pin high
    delay(10); // Adjust the delay for the desired speed
    digitalWrite(dstepPin, LOW); // Set the step pin low
    delay(10); // Adjust the delay for the desired speed
    if(digitalRead(dPin) == HIGH){
      return;
    }
  }
}
void dpoistion(int togo){
  dshome();  
  Serial.print(togo);
  delay(1000);
  digitalWrite(ddirPin, LOW);
  for (int i = 0; i < (25*(togo)); i++) {
    digitalWrite(dstepPin, HIGH); // Set the step pin high
    delay(10); // Adjust the delay for the desired speed
    digitalWrite(dstepPin, LOW); // Set the step pin low
    delay(10); // Adjust the delay for the desired speed
  }
}
void sensef(){
  digitalWrite(ndirPin, LOW);  
  while(1) {
    digitalWrite(nstepPin, HIGH);
    delayMicroseconds(500); 
    digitalWrite(nstepPin, LOW);
    delayMicroseconds(500);  
    pulseHigh = pulseIn(freqPin, HIGH);
    pulseLow = pulseIn(freqPin, LOW);

    pulseTotal = pulseHigh + pulseLow; // Time period of the pulse in microseconds
    frequency = 1000000/ pulseTotal; // Frequency in Hertz (Hz)
    if(frequency < 55000){
      nstepper.setSpeed(0);
      nstepper.run();
      int cnt = 0;
      for(int i=0; i<5; i++){
        pulseHigh = pulseIn(freqPin, HIGH);
        pulseLow = pulseIn(freqPin, LOW);
        pulseTotal = pulseHigh + pulseLow; // Time period of the pulse in microseconds
        frequency = 1000000/ pulseTotal; // Frequency in Hertz (Hz)
        if(frequency < 55000){
          cnt++;
        }
      }
      // Serial.println(frequency);
      if(cnt == 5){
        state = in_liquid;  
        return;        
      }
      
    }
    Serial.println(frequency);
    if(digitalRead(nlPin) == HIGH){
      state = sense_failed;
      status = 2;
      erroccurred = 1;
      if(tubenumber == 0){
        Serial2.print("Error/Sample tube number " + String(test1[1]) + " is empty");
      }
      else{
        Serial2.print("Error/Reagent tube number " + String(tubenumber) + " is empty");
      }
      nshome();
      // dshome();//make sure disk doesn't move
      servo.write(50);  
      return;       
    }
  }
  delay(1000);
}
//isr for serial 
// Interrupt Service Routine for Serial2
void msgreceived() {
  delay(500);
  Serial.println("receivedString");
  // if (Serial.available()) 
  String receivedString = "";
    
  // Read all available characters until the complete string is received
  while (Serial2.available()) {
    //char receivedChar = Serial2.read();
    receivedString = Serial2.readString();
    Serial.print(receivedString);
    delay(2);  // Small delay to allow for receiving the full string
  }  
  if(receivedString == "status"){
    if(status == 0){
      Serial2.print("none");
    }
    else if(status == 1){
      Serial2.print("running");
    }
    else if(status == 2){
      Serial2.print("paused");
    }
  }
  else if(receivedString == "sch av"){
    Serial2.print(String(nstests));
  }
  else if(receivedString == "get available"){
    int av = 0;
    String sendString = "s";
    for(int i=0; i<2; i++){
      if(samples[i] == 1){
        av++;
        sendString += "/" + String(i+1);
      }
    }
    if(av == 0){
      Serial2.print("s/");
    }
    else{
      Serial2.print(sendString);
    }
    delay(5000);
    av = 0;
    sendString = "r";
    for(int i=0; i<4; i++){
      if(rcups[i] == 1){
        av++;
        sendString += "/" + String(i+1);
      }
    }
    if(av == 0){
      Serial2.print("r/");
    }
    else{
      Serial2.print(sendString);
    }
  }
  else if(receivedString == "get checked"){
    String tsnd = "a/";
    tsnd += String(samples[0]) + String(samples[1]) + String(rcups[0]) + String(rcups[1]) + String(rcups[2]) + String(rcups[3]);
    Serial2.print(tsnd);
  }
  else if(receivedString == "terminate"){
    cleaning();
    tubenumber = 0;
    nstests--;
    state = find_next;
    for(int i=0; i<3; i++){
      test1[i] = test2[i];
      test2[i] = test3[i];
      test3[i] = test4[i];
      test4[i] = 0;
    }

  }
  else if(receivedString == "pause"){
    status = 2;
    prestop = state;
    state = stopped;
  }
  else if(receivedString == "continue"){
    if(erroccurred == 1){
      state = sense;
    }
    else if(erroccurred == 0){
      state = prestop;
    }
    status = 1;
  }
  else if(receivedString == "get sch"){
    String tsnd = "d/";
    tsnd += String(nstests)+ "/" + String(test1[0]) +  "/" + String(test1[1]) + String(test1[2]) + String(test2[0]) + String(test2[1]) + String(test2[2]) + String(test3[0]) + String(test3[1]) + String(test3[2]) + String(test4[0]) + String(test4[1]) + String(test4[2]);
    Serial2.print(tsnd);
  }
  else if(receivedString.charAt(0) == 'p'){
    nstests++;
    test1[0] = receivedString.charAt(2) - '0';
    test1[1] = receivedString.charAt(4) - '0';
    test1[2] = receivedString.charAt(6) - '0';
    rcups[test1[2] - 1] = 0;
  }
  else if(receivedString.charAt(0) == 's'){
    switch (nstests){
      case 0:
      test1[0] = receivedString.charAt(2) - '0';
      test1[1] = receivedString.charAt(4) - '0';
      test1[2] = receivedString.charAt(6) - '0';
      rcups[test1[2] - 1] = 0;
      break;
      case 1:
      test2[0] = receivedString.charAt(2) - '0';
      test2[1] = receivedString.charAt(4) - '0';
      test2[2] = receivedString.charAt(6) - '0';
      rcups[test2[2] - 1] = 0;
      break;
      case 2:
      test3[0] = receivedString.charAt(2) - '0';
      test3[1] = receivedString.charAt(4) - '0';
      test3[2] = receivedString.charAt(6) - '0';
      rcups[test3[2] - 1] = 0;
      break;
      case 3:
      test4[0] = receivedString.charAt(2) - '0';
      test4[1] = receivedString.charAt(4) - '0';
      test4[2] = receivedString.charAt(6) - '0';
      rcups[test4[2] - 1] = 0;
      break;
    }
    nstests++; 
  }
  else if(receivedString.charAt(0) == 'y'){
    int indx = 2;
    if(receivedString.charAt(indx) == 't'){
      samples[0] = 1;
      indx += 5;
    }
    else{
      samples[0] = 0;
      indx += 6;
    }
    if(receivedString.charAt(indx) == 't'){
      samples[1] = 1;
      indx += 5;
    }
    else{
      samples[1] = 0;
      indx += 6;
    }
    if(receivedString.charAt(indx) == 't'){
      rcups[0] = 1;
      indx += 5;
    }
    else{
      rcups[0] = 0;
      indx += 6;
    }
    if(receivedString.charAt(indx) == 't'){
      rcups[1] = 1;
      indx += 5;
    }
    else{
      rcups[1] = 0;
      indx += 6;
    }
    if(receivedString.charAt(indx) == 't'){
      rcups[2] = 1;
      indx += 5;
    }
    else{
      rcups[2] = 0;
      indx += 6;
    }
    if(receivedString.charAt(indx) == 't'){
      rcups[3] = 1;
      indx += 5;
    }
    else{
      rcups[3] = 0;
      indx += 6;
    }
  }
delay(500);
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);
  //for serial communication
  // attachInterrupt(digitalPinToInterrupt(16), msgreceived, FALLING);
  state = initialize;
  //steppers' initialization
  nstepper.setMaxSpeed(1000);
	nstepper.setAcceleration(50);
	sstepper.setMaxSpeed(1000);
	sstepper.setAcceleration(50);
  dstepper.setMaxSpeed(1000);
	dstepper.setAcceleration(50);
  // servo.attach(servoPin);
  
  //modes
  //inputs
  pinMode(nlPin, INPUT);
  pinMode(nhPin, INPUT);
  pinMode(shPin, INPUT);
  pinMode(slPin, INPUT);
  pinMode(dPin, INPUT);
  //output
  pinMode(valvaPin, OUTPUT);
  pinMode(nstepPin, OUTPUT);
  pinMode(ndirPin, OUTPUT);
  pinMode(sstepPin, OUTPUT);
  pinMode(sdirPin, OUTPUT);
  pinMode(dstepPin, OUTPUT);
  pinMode(ddirPin, OUTPUT);
  pinMode(servoPin, OUTPUT);
  // digitalWrite(5, LOW);
  // digitalWrite(6, LOW);
  // digitalWrite(9, LOW);
  // digitalWrite(10, LOW);
  delay(1000);
  
}

void loop() {

  switch (state){
    case initialize:
      initialization();
      if(Serial2.available()) msgreceived();
      state = find_next;
      // nstests = 1;
      // test1[0] = 2;
      // test1[1] = 1;
      // test1[2] = 1;
    break;
    case find_next:
    if(Serial2.available()) msgreceived();
      if(nstests > 0){
        Serial.print("in find next");
        status = 1;
        state = sense; 
        // dshome();
        if(test1[1] == 1){
          servo.write(69);//angle of s1
          if(((test1[0] != 1) && (test1[0] != 2) && (test1[0] != 3)) || ((test1[2] != 1) && (test1[2] != 2) && (test1[2] != 3) && (test1[2] != 4)))
          {
            state = find_next;
            nstests = 0; 
          }
        }
        else if(test1[1] == 2){
          servo.write(81);//angle of s2
        }    
        else{
          state = find_next;
          nstests = 0;
        }    
        delay(500);
      }  
      else {
        status = 0;
      }
    break;
    case sense:
      // if(Serial2.available()) msgreceived();
      sensef();  
      if(Serial2.available()) msgreceived();
    break;
    case in_liquid:
    // if(Serial2.available()) msgreceived();
      if(tubenumber == 0){
        switch (test1[0]){
        case 1:
          h = 0.4 / (PI * 0.36);
          pl = 0.4 * 2.1 / 2.8;
        break; 
        case 2:
          h = 0.5 / (PI * 0.36);
          pl = 0.5 * 2.1 / 2.8;
        break; 
        case 3:
          h = 0.3 / (PI * 0.36);
          pl = 0.3 * 2.1 / 2.8;
        break;        
        }
      }
      else{
        switch (test1[0]){
        case 1:
          h = x[tubenumber-1] / (PI * 0.36);
          pl = x[tubenumber-1] * 2.1 / 2.8;
        break; 
        case 2:
          h = y[tubenumber-1] / (PI * 0.36);
          pl = y[tubenumber-1] * 2.1 / 2.8;
        break; 
        case 3:
          h = z[tubenumber-1] / (PI * 0.36);
          pl = z[tubenumber-1] * 2.1 / 2.8;
        break;        
        }
        
      }
      
      nsteps = 200.0 * h / 6.0;
      
      digitalWrite(ndirPin, LOW);  
      for (int i = 0; i < nsteps+100; i++) {
        digitalWrite(nstepPin, HIGH);
        delayMicroseconds(10000); 
        digitalWrite(nstepPin, LOW);
        delayMicroseconds(10000);
        if(Serial2.available()) msgreceived(); 
      }
      state = draw_in;
    if(Serial2.available()) msgreceived();
    break;
    case draw_in:
    // if(Serial2.available()) msgreceived();
      ssteps = pl * 200 / 0.1; 
      ssteps = round(ssteps);
      digitalWrite(sdirPin, HIGH);  
      for (int i = 0; i < ssteps; i++) {
        digitalWrite(sstepPin, HIGH);
        delayMicroseconds(100000);  
        digitalWrite(sstepPin, LOW);
        delayMicroseconds(100000);  
        if(Serial2.available()) msgreceived();
      }
      state = draw_out;
      if(Serial2.available()) msgreceived();
      break;
    case draw_out:
    // if(Serial2.available()) msgreceived();
      nshome();
      switch (test1[2]){
        case 1:
          servo.write(103);
        break;
        case 2:
          servo.write(115);
        break;
        case 3:
          servo.write(127);
        break;
        case 4:
          servo.write(139);
        break;
      }
      delay(1000);
       digitalWrite(ndirPin, LOW); 
      for (int i = 0; i < 1250; i++) {
        digitalWrite(nstepPin, HIGH);
        delayMicroseconds(500); 
        digitalWrite(nstepPin, LOW);
        delayMicroseconds(500);  
        if(Serial2.available()) msgreceived();
      }
      digitalWrite(sdirPin, LOW);  
      for (int i = 0; i < 100000; i++) {
        digitalWrite(sstepPin, HIGH);
        delayMicroseconds(1000);  
        digitalWrite(sstepPin, LOW);
        delayMicroseconds(1000); 
        if(digitalRead(shPin) == HIGH){
          break;
        }
        if(Serial2.available()) msgreceived();
      } 
      nshome();
      servo.write(180);
      cleaning();
      if(Serial2.available()) msgreceived();
      state = next_tube;
      // dshome();
      break;
    case next_tube: 
    // if(Serial2.available()) msgreceived();     
      if(tubenumber == 8){
        tubenumber = 0;
        state = find_next;    
         Serial2.print("Success/ Test finished");
        nstests--;
        for(int i=0; i<3; i++){
          test1[i] = test2[i];
          test2[i] = test3[i];
          test3[i] = test4[i];
          test4[i] = 0;
        }    
        // dshome();
      }   
      else{
        tubenumber++;
        switch (test1[0]){
          case 1:
            while((x[tubenumber - 1] == 0) && (tubenumber<9)){
              tubenumber++;
            }
          break;
          case 2:
            while((y[tubenumber - 1] == 0) && (tubenumber<9)){
              tubenumber++;
            }
          break;
          case3:
            while((z[tubenumber - 1] == 0) && (tubenumber<9)){
              tubenumber++;
            }
          break;
        }
        if(tubenumber > 8){
          Serial2.print("Success/ Test finished");
          tubenumber = 0;
          nstests--;
          state = find_next;
          for(int i=0; i<3; i++){
            test1[i] = test2[i];
            test2[i] = test3[i];
            test3[i] = test4[i];
            test4[i] = 0;
          }
          // dshome();
        }
        else{
          // dshome();
          dpoistion(tubenumber-1); 
          // dindx = tubenumber;
          delay(1000);
          servo.write(0);
          delay(1000);
          state = sense;
        }                 
      } 
      if(Serial2.available()) msgreceived();
    break;
    case sense_failed:
      if(Serial2.available()) msgreceived();
      if(state == sense){
        if(tubenumber == 0){
          if(test1[1] == 1){
          servo.write(70);//angle of s1
          }
          else if(test1[1] == 2){
            servo.write(81);//angle of s2
          }  
        }    
        else{
            servo.write(0);
          }  
        delay(500);
      }
    break;
    case stopped:
      if(Serial2.available()) msgreceived();
    break;
    default:
    break;
  }
  
  
}