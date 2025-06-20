#include <MultiStepper.h>
#include <AccelStepper.h>

#define Servo1_pul 54
#define Servo2_pul 36
#define Servo3_pul 26
#define Servo4_pul 60

#define Servo1_dir 55
#define Servo2_dir 34
#define Servo3_dir 28
#define Servo4_dir 61

#define Servo1_en 38
#define Servo2_en 30
#define Servo3_en 24
#define Servo4_en 56

//limit switches for follower
#define UpperLim 19
#define LowerLim 15
#define runnerLim 18

String msg;

//-----------Initialize stepper objects--------------//
AccelStepper ColdDrawStepper(AccelStepper::DRIVER, Servo4_pul, Servo4_dir);      // Traveller control
AccelStepper FollowerStepper(AccelStepper::DRIVER, Servo1_pul, Servo1_dir);      // Z motor travelling control
AccelStepper UpperMandrelStepper(AccelStepper::DRIVER, Servo2_pul, Servo2_dir);  // Twisting motor
AccelStepper LowerMandrelStepper(AccelStepper::DRIVER, Servo3_pul, Servo3_dir);  // Mandrel coiling motor top

MultiStepper FR_Group;    //Group of steppers which coordinate to perfrom FR
MultiStepper Coil_Group;  //Group of steppers which coordinate to perfrom coiling

//--------------Ratios and Speeds-------------//
const int stepsPerRevolution = 400;
int jogSpeed = 400;
int coldDrawSpeed = 400;
int TwistSpeed = 950;
float FRSpeed = 1000;

float LeadStepsPerMM = stepsPerRevolution / 8.0;
float ColdDrawReduction = 10.0 * (stepsPerRevolution / (3.14159 * 25.11));  //ratio of steps to mm of cold draw GearReduction*(stepsPerRevs/(pi*pulleyDiam))
float TubeLengthBuffer = 120;                                               //mm -- the amount of the tube left without FR
float CoilBuffer = 3;                                                       // number of coils removed from estimate of the number of coils to prevent collision
float MandrelDiam = 3;                                                      //mm
float upperMandrelReduction = -5.4;
float SpinSpeed = 1000;

//set globals------------------------------------//
float twistAngle;
float InitialTubeLength;
float tubeDiameter;
float coldDrawRatio;
float FRPitch;
float CoilPitch;
float FinalTubeLength;
float Muscle_Length;
float NumCoils;
float coilAngle;
int buttonMode = 0;  //Sets "mode" that corresponds with a button and its specific motor combinations


void setup() {
  pinMode(UpperLim, INPUT_PULLUP);
  pinMode(LowerLim, INPUT_PULLUP);
  pinMode(runnerLim, INPUT_PULLUP);

  pinMode(Servo1_en, OUTPUT);
  pinMode(Servo2_en, OUTPUT);
  pinMode(Servo3_en, OUTPUT);
  pinMode(Servo4_en, OUTPUT);

  digitalWrite(Servo1_en, LOW);
  digitalWrite(Servo2_en, LOW);
  digitalWrite(Servo3_en, LOW);
  digitalWrite(Servo4_en, LOW);

  // Set Max Speed
  ColdDrawStepper.setMaxSpeed(1000);
  UpperMandrelStepper.setMaxSpeed(1000);
  FollowerStepper.setMaxSpeed(1000);
  LowerMandrelStepper.setMaxSpeed(1000);

  ColdDrawStepper.setAcceleration(100000);
  UpperMandrelStepper.setAcceleration(100000);
  FollowerStepper.setAcceleration(100000);
  LowerMandrelStepper.setAcceleration(100000);

  FR_Group.addStepper(FollowerStepper);
  FR_Group.addStepper(UpperMandrelStepper);
  FR_Group.addStepper(LowerMandrelStepper);

  Coil_Group.addStepper(FollowerStepper);
  Coil_Group.addStepper(UpperMandrelStepper);
  Coil_Group.addStepper(LowerMandrelStepper);
  Coil_Group.addStepper(ColdDrawStepper);

  Serial.begin(9600);
}

void loop() {
  ///////////////////////////////////////////////////////////////////
  //check for messages and update controller state
  ///////////////////////////////////////////////////////////////////
  if (Serial.available()) {
    //A,35,100,2.6,8,7,3Z
    //A,35,220,2.5,12,6,3Z
    //A,35,255,2.4,2.5,12,6,3Z

    msg = Serial.readStringUntil('Z');
    String CommandString = getValue(msg, ',', 0);
    twistAngle = getValue(msg, ',', 1).toFloat();
    InitialTubeLength = getValue(msg, ',', 2).toFloat();
    tubeDiameter = getValue(msg, ',', 3).toFloat();
    coldDrawRatio = getValue(msg, ',', 4).toFloat();
    FRPitch = getValue(msg, ',', 5).toFloat();
    CoilPitch = getValue(msg, ',', 6).toFloat();
    MandrelDiam = getValue(msg, ',', 7).toFloat();

    //calculate final tube length after cold draw
    FinalTubeLength = coldDrawRatio * InitialTubeLength;

    //These are expensive to compute, so i am moving it up here so it only gets calculated once
    NumCoils = (FinalTubeLength - TubeLengthBuffer) / sqrt(CoilPitch * CoilPitch + pow(3.14 * (MandrelDiam + tubeDiameter), 2)) - CoilBuffer;
    Muscle_Length = NumCoils * CoilPitch;
    //Muscle_Length = sqrt(pow((FinalTubeLength - TubeLengthBuffer) * CoilPitch, 2) / (pow(3.14 * (MandrelDiam + tubeDiameter), 2) + pow(CoilPitch, 2)));
    coilAngle = atan2(CoilPitch, 3.14159 * (MandrelDiam + tubeDiameter));
    //Serial.print(sqrt(CoilPitch * CoilPitch + pow(3.14 * (MandrelDiam + tubeDiameter), 2)));
    // Serial.print("  ");
    // Serial.print(Muscle_Length);
    // Serial.print("  ");
    // Serial.print(NumCoils);
    // Serial.print("  ");
    // Serial.println(FinalTubeLength- TubeLengthBuffer - CoilBuffer*(sqrt(CoilPitch * CoilPitch + pow(3.14 * (MandrelDiam + tubeDiameter), 2))) - Muscle_Length);

    if (CommandString.equals("A")) {
      buttonMode = 1;  // home
    } else if (CommandString.equals("B")) {
      buttonMode = 2;  //cold draw
    } else if (CommandString.equals("C")) {
      buttonMode = 3;  // FR
    } else if (CommandString.equals("D")) {
      buttonMode = 4;  // twist
    } else if (CommandString.equals("E")) {
      buttonMode = 5;  // coil
    } else if (CommandString.equals("F")) {
      buttonMode = 0;  //stop
    } else if (CommandString.equals("G")) {
      buttonMode = 6;  //Follower up
    } else if (CommandString.equals("H")) {
      buttonMode = 7;  //Follower down
    } else if (CommandString.equals("I")) {
      buttonMode = 8;  //runner up
    } else if (CommandString.equals("J")) {
      buttonMode = 9;  //runner down
    } else if (CommandString.equals("K")) {
      buttonMode = 10;  //Spin
    } else {
      buttonMode = 0;
    }
  }


  ///////////////////////////////////////////////////////////////////
  //Act on Controller state
  ///////////////////////////////////////////////////////////////////
  if (buttonMode == 0) {  //stop
    //set current position to target position
    ColdDrawStepper.stop();
    UpperMandrelStepper.stop();
    FollowerStepper.stop();
    LowerMandrelStepper.stop();
    // Serial.println("STOP");
    // Serial.println(ColdDrawStepper.distanceToGo());

  } else if (buttonMode == 1) {  // home
    //while the follower has not hit the upper limitswitch, jog up. Once we hit limit, set this as zero for ALL axes. Return to Mode0 (stop)
    ColdDrawStepper.setMaxSpeed(1000);
    UpperMandrelStepper.setMaxSpeed(1000);
    FollowerStepper.setMaxSpeed(1000);
    LowerMandrelStepper.setMaxSpeed(1000);
    if (digitalRead(UpperLim) == 1) {
      FollowerStepper.setSpeed(jogSpeed * 2);
      FollowerStepper.runSpeed();
      //delay(20);
    } else {
      FollowerStepper.setSpeed(jogSpeed);
      //Serial.println(long(LeadStepsPerMM * -10.0));
      FollowerStepper.move(long(LeadStepsPerMM * -10.0));
      FollowerStepper.runToPosition();

      ColdDrawStepper.setCurrentPosition(0);
      UpperMandrelStepper.setCurrentPosition(0);
      FollowerStepper.setCurrentPosition(0);
      LowerMandrelStepper.setCurrentPosition(0);

      buttonMode = 0;
    }

  } else if (buttonMode == 2) {  //cold draw

    ColdDrawStepper.setMaxSpeed(1000);
    UpperMandrelStepper.setMaxSpeed(1000);
    FollowerStepper.setMaxSpeed(1000);
    LowerMandrelStepper.setMaxSpeed(1000);

    float ColdDrawLength = (FinalTubeLength - InitialTubeLength) * ColdDrawReduction;  //length of the cold draw in steps

    ColdDrawStepper.setSpeed(coldDrawSpeed);
    ColdDrawStepper.move(long(ColdDrawLength));

    buttonMode = -1;

  } else if (buttonMode == 3) {  // FR
    ColdDrawStepper.setMaxSpeed(FRSpeed);
    UpperMandrelStepper.setMaxSpeed(FRSpeed);
    FollowerStepper.setMaxSpeed(FRSpeed);
    LowerMandrelStepper.setMaxSpeed(FRSpeed);
    long TargetPositions[3];

    float FR_Length = (FinalTubeLength - TubeLengthBuffer);  //length to apply FR
    TargetPositions[0] = long(-FR_Length * LeadStepsPerMM);  //Lead screw position

    TargetPositions[1] = long(upperMandrelReduction * float(stepsPerRevolution) * (FR_Length / FRPitch));  //Upper mandrel position
    TargetPositions[2] = -long(float(stepsPerRevolution) * (FR_Length / FRPitch));                         //Lower Mandrel position


    //Serial.println(TargetPositions[1]);
    FR_Group.moveTo(TargetPositions);
    //FR_Group.runSpeedToPosition();
    //buttonMode = -1;
    FR_Group.run();


  } else if (buttonMode == 4) {  // twist
    ColdDrawStepper.setMaxSpeed(1000);
    UpperMandrelStepper.setMaxSpeed(1000);
    FollowerStepper.setMaxSpeed(1000);
    LowerMandrelStepper.setMaxSpeed(1000);
    // calculate amount of twist to put into the tube and add that much twist as a relative move.
    float numTwists = sin(twistAngle * 3.14159 / 180.0) * (coldDrawRatio * InitialTubeLength) / (2 * 3.14159 * tubeDiameter);
    //Serial.println(numTwists);
    // UpperMandrelStepper.setSpeed(TwistSpeed);
    // UpperMandrelStepper.move(long(numTwists * stepsPerRevolution*upperMandrelReduction));
    // UpperMandrelStepper.runToPosition();

    LowerMandrelStepper.setSpeed(TwistSpeed);
    LowerMandrelStepper.move(long(numTwists * stepsPerRevolution));
    LowerMandrelStepper.runToPosition();
    buttonMode = -1;

  } else if (buttonMode == 5) {  //coil

    ColdDrawStepper.setMaxSpeed(1500);
    UpperMandrelStepper.setMaxSpeed(1000);
    FollowerStepper.setMaxSpeed(1000);
    LowerMandrelStepper.setMaxSpeed(1000);

    long TargetPositions[4];

    TargetPositions[0] = long(-Muscle_Length * LeadStepsPerMM);                                                  //Lead screw position
    TargetPositions[1] = long(upperMandrelReduction * float(stepsPerRevolution) * (Muscle_Length / CoilPitch));  //Upper mandrel position
    TargetPositions[2] = long(-(1 - cos(coilAngle)) * float(stepsPerRevolution) * (Muscle_Length / CoilPitch));
    //TargetPositions[2]= LowerMandrelStepper.currentPosition();    // neglecting Writhe                  //Lower Mandrel position
    TargetPositions[3] = -(FinalTubeLength - Muscle_Length - TubeLengthBuffer - CoilBuffer * (sqrt(CoilPitch * CoilPitch + pow(3.14 * (MandrelDiam + tubeDiameter), 2)))) * ColdDrawReduction;
    //TargetPositions[3] = 0;
    // Serial.println(TargetPositions[3]/ColdDrawReduction);
    Coil_Group.moveTo(TargetPositions);

    Coil_Group.run();

  } else if (buttonMode == 6) {  // follower up

    FollowerStepper.setMaxSpeed(1000);

    FollowerStepper.setSpeed(jogSpeed * 2);
    FollowerStepper.runSpeed();

  } else if (buttonMode == 7) {  //follower down
    FollowerStepper.setMaxSpeed(1000);

    FollowerStepper.setSpeed(-jogSpeed * 2);
    FollowerStepper.runSpeed();

  } else if (buttonMode == 8) {  //runner up
    ColdDrawStepper.setMaxSpeed(1000);
    ColdDrawStepper.setSpeed(-2 * coldDrawSpeed);
    ColdDrawStepper.runSpeed();

  } else if (buttonMode == 9) {  //runner down
    ColdDrawStepper.setMaxSpeed(1000);
    ColdDrawStepper.setSpeed(2 * coldDrawSpeed);
    ColdDrawStepper.runSpeed();

  } else if (buttonMode == 10) {  //Spin
    UpperMandrelStepper.setMaxSpeed(1000);
    UpperMandrelStepper.setSpeed(SpinSpeed);
    LowerMandrelStepper.setMaxSpeed(1000);
    LowerMandrelStepper.setSpeed(SpinSpeed / 5.735);

  } else {  //default--do nothing
  }


  ///////////////////////////////////////////////////////////////////
  //Check Hardstops
  ///////////////////////////////////////////////////////////////////
  if ((digitalRead(UpperLim) == 0 || digitalRead(LowerLim) == 0 || digitalRead(runnerLim) == 0) && buttonMode != 1) {


    if (digitalRead(runnerLim) == 0) {
      FollowerStepper.setSpeed(jogSpeed);
      //Serial.println(long(LeadStepsPerMM * -10.0));
      FollowerStepper.move(long(LeadStepsPerMM * 1.0));
      FollowerStepper.runToPosition();
    }
    buttonMode = 0;

    ColdDrawStepper.stop();
    UpperMandrelStepper.stop();
    FollowerStepper.stop();
    LowerMandrelStepper.stop();
    // Serial.println(digitalRead(UpperLim));
  }
  if (buttonMode != 3 && buttonMode != 5) {
    ColdDrawStepper.run();
    UpperMandrelStepper.run();
    FollowerStepper.run();
    LowerMandrelStepper.run();
  }

  // ColdDrawStepper.run();
  // UpperMandrelStepper.run();
  // FollowerStepper.run();
  // LowerMandrelStepper.run();
  //}
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
