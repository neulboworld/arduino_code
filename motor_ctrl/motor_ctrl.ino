#include <LiquidCrystal.h>

int PWM1 = 11;
int DIR1 = 12;
int EN1 = 13;
int speed = 10;
int DIRFLAG1 = 0;
int temp = 0;
int t_now = 0;
int t_prev = 0;

boolean DIRFLAG = true;
boolean INCFLAG = false;
boolean DECFLAG = false;

boolean up_prev = false;
boolean down_prev = false;
boolean toggle_prev = false;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);           // select the pins used on the LCD panel

//int curr = A5; // INPUT PIN

// define some values used by the panel and buttons
int lcd_key     = 0;
int adc_key_in  = 0;

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

int read_LCD_buttons() {              // read the buttons
  adc_key_in = analogRead(0);       // read the value from the sensor

  // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
  // we add approx 50 to those values and check to see if we are close
  // We make this the 1st option for speed reasons since it will be the most likely result

  if (adc_key_in > 1000) return btnNONE;

  // For V1.1 us this threshold
  if (adc_key_in < 50)   return btnRIGHT;
  if (adc_key_in < 250)  return btnUP;
  if (adc_key_in < 450)  return btnDOWN;
  if (adc_key_in < 650)  return btnLEFT;
  if (adc_key_in < 850)  return btnSELECT;

  // For V1.0 comment the other threshold and use the one below:
  /*
    if (adc_key_in < 50)   return btnRIGHT;
    if (adc_key_in < 195)  return btnUP;
    if (adc_key_in < 380)  return btnDOWN;
    if (adc_key_in < 555)  return btnLEFT;
    if (adc_key_in < 790)  return btnSELECT;
  */

  return btnNONE;                // when all others fail, return this.
}

void setup() {
  // LCD Settings
  lcd.begin(16, 2);               // start the library
  lcd.setCursor(0, 0);            // set the LCD cursor   position
  lcd.print("Motor Test");  // print a simple message on the LCD

  // Motor Settings
   pinMode(DIR1,OUTPUT);
   pinMode(EN1,OUTPUT);
   pinMode(PWM1,OUTPUT);

   digitalWrite(EN1, HIGH);
}

void loop() {
  int curr = analogRead(A5);
  if(INCFLAG == true) {
    if(speed < temp)
      speed += 5;
    else {
      speed = temp;
      INCFLAG = false;
      state = 2;
    }
    delay(50);
  }
  
  if(DECFLAG == true) {
    if(speed > 5)
      speed -= 5;
    else {
      speed = 0;
      DECFLAG = false;
      //delay(500);
      DIRFLAG = !DIRFLAG;
      INCFLAG = true;
    }
    delay(50);
  }

  if(state == 2) {
    
  }
  
  lcd.setCursor(9,1);
  /*if(speed < 10)
    lcd.print("   ");
  else if(speed < 100)
    lcd.print("  "); 
  else if(speed < 1000)
    lcd.print(" ");
  lcd.print(speed);
  */
  if(curr < 10)
    lcd.print("   ");
  else if(curr < 100)
    lcd.print("  "); 
  else if(curr < 1000)
    lcd.print(" ");
  if(millis()%50 == 0)
    lcd.print(curr);
  
  lcd.setCursor(0, 1);            // move to the begining of the second line

  if(INCFLAG || DECFLAG) 
  {
    if(INCFLAG)  lcd.print("ON    ");
    else         lcd.print("OFF   ");
  }
  
  else 
  {
    lcd_key = read_LCD_buttons();   // read the buttons
    switch (lcd_key) {              // depending on which button was pushed, we perform an action
  
      case btnRIGHT: {            //  push button "RIGHT" and show the word on the screen
          lcd.print("RIGHT ");
          if(!up_prev) {
            if(speed < 246) speed = speed + 10;
            else            speed = 255;
            up_prev = true;
          }
          break;
        }
      case btnLEFT: {
          lcd.print("LEFT   "); //  push button "LEFT" and show the word on the screen
          if(!down_prev) {
            if(speed > 9) speed = speed - 10;
            else          speed = 0;
            down_prev = true;
          }
          break;
        }
      case btnUP: {
          lcd.print("UP    ");  //  push button "UP" and show the word on the screen
          if(!up_prev) {
            if(speed < 255) speed = speed + 1;
            up_prev = true;
          }
          break;
        }
      case btnDOWN:
          lcd.print("DOWN  ");  //  push button "DOWN" and show the word on the screen
          if(!down_prev) {
            if(speed > 0) speed = speed - 1;
            down_prev = true;
          }
          break;
          
      case btnSELECT:
        //lcd.print("SELECT");  //  push button "SELECT" and show the word on the screen
        if(speed != 0) {
          DECFLAG = true;
          temp = speed;
        }
        else
          INCFLAG = true;
        break;
      
      case btnNONE:
          if(DIRFLAG)
            lcd.print("CW    ");
          else
            lcd.print("CCW   ");
          up_prev = false;
          down_prev = false;
          break;
    }
  }
  
  analogWrite(PWM1,speed); //0~255
  if(DIRFLAG)
    digitalWrite(DIR1, HIGH);
  else
    digitalWrite(DIR1, LOW);
    
  digitalWrite(EN1, LOW);
}
