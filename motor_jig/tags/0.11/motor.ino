#include <LiquidCrystal.h>

// Port for Motor control
int BACKLIGHT = 10;
int PWM1 = 11;
int DIR1 = 12;
int EN1 = 13;
int PWM2 = 3;
int DIR2 = 2;
int EN2 = 1;

boolean CH2EN = true;

// parameter for loop
int speed = 0;
int state = 0;
unsigned long int start_time = 0;
unsigned long int click_time = 0;

int prog_seq = 0;
boolean DIRFLAG = true;
boolean btnClicked = false;

// avg current
long int sum_cw = 0;
int cnt;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7); 

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
  // Debugging
  if(!CH2EN)   Serial.begin(115200);
  
  // LCD Settings
  lcd.begin(16, 2);               // 16x2 : LCD Size
  print_disp(0,0, "CW",   2);
  print_disp(0,1, "CCW",  3);
  print_disp(11,0,"READY",5);

  pinMode(BACKLIGHT,OUTPUT);
  pinMode(PWM1,OUTPUT);
  pinMode(DIR1,OUTPUT);
  pinMode(EN1,OUTPUT);
  
  if(CH2EN) {
    pinMode(PWM2,OUTPUT);
    pinMode(DIR2,OUTPUT);
    pinMode(EN2,OUTPUT);
  } 
  digitalWrite(BACKLIGHT, HIGH);
}

void loop() {
  // Analog Read
  int volt = 5000.0/1024.0*analogRead(A5);
  int curr = volt-2500;
  int row;

  //Serial.println(state);
  
  if(DIRFLAG) row = 0;
  else        row = 1;
  
  // key interrupt
  lcd_key = read_LCD_buttons();
  if(lcd_key == btnSELECT && !btnClicked) {
    click_time = millis();
    if(state == 0) {
      state = 1; sum_cw = 0;
      clear_disp(4,0,12); clear_disp(4,1,12);
      print_disp(11,row,"ON",3);
      digitalWrite(10, HIGH);
    }
    btnClicked = true;
  }

  else if(lcd_key != btnNONE)
    click_time = millis();

  else
    btnClicked = false;
  
  /*
  else if(lcd_key == btnRIGHT && !btnClicked) {
    lcd.scrollDisplayRight();
    btnClicked = true;
  }

  else if(lcd_key == btnLEFT && !btnClicked) {
    lcd.scrollDisplayLeft();
    btnClicked = true;
  }
  */


  // State
  switch(state) {
    case 0:  
      /*
      Serial.print(click_time);
      Serial.print("/");
      Serial.println(millis());
      */
      if(millis()-click_time > 10000)
        digitalWrite(10, LOW);
      else
        digitalWrite(10, HIGH);
      break;
    case 1:
      if(speed < 255) {
        speed += 5;
      }
      else {
        speed = 255;
        state += 1;
        while(millis()%1000 == 0) {
          delay(10);
        }
        start_time = millis();
        clear_disp(11,row,5);
      }
      delay(50);
      break;
      
    case 2:
      /*
      Serial.print(millis());
      Serial.print("/");
      Serial.println(start_time);
      */
      
      if(millis() - start_time > 10000) {
        state += 1;
        print_disp(4,row,sum_cw/cnt,4);
        lcd.print(".");
        if(DIRFLAG) {
          if(sum_cw/cnt > 100 && sum_cw/cnt < 200)  print_disp(10,row,"[PASS]",6);
          else                                      print_disp(10,row,"[FAIL]",6);
        }
        else {
          if(sum_cw/cnt < -100 && sum_cw/cnt > -200)  print_disp(10,row,"[PASS]",6);
          else                                      print_disp(10,row,"[FAIL]",6);
        }
      }
      else {
        if(millis()%200 == 0) {
          clear_disp(4,row,5);
          print_disp(4,row,curr,4);
          sum_cw = sum_cw + curr;
          cnt += 1;
          print_disp(13,row,cnt/5,2);
          prog_view(11,row);
          /*
          Serial.print(cnt);
          Serial.print("/");
          Serial.print(curr);
          Serial.print("/");
          Serial.println(sum_cw);
          */
        }
      }
      break;
      
    case 3:
     if(speed > 5) {
      speed -= 5;
     }
    
    else {
      speed = 0;
      if(DIRFLAG) {
        state = 1;
        clear_disp(4,1,11);
        print_disp(11,1,"ON",3);
      }
      else {
        state = 0;
        click_time = millis();
      }
      cnt = 0;
      sum_cw = 0;
      DIRFLAG = !DIRFLAG;
    }
    delay(50);
    break;
  }
  
  analogWrite(PWM1,speed); //0~255
  
  if(DIRFLAG) {
    digitalWrite(DIR1, HIGH);
  }
  else {
    digitalWrite(DIR1, LOW);
  }
  digitalWrite(EN1, LOW);
  
  if(CH2EN) {
    analogWrite(PWM2,speed);
   
    if(DIRFLAG) {
      digitalWrite(DIR2, HIGH);
    }
    else {
      digitalWrite(DIR2, LOW);
    }
    
    digitalWrite(EN2, LOW);
  }
  
  else {
    //Read hall sensor
    Serial.print("U1 : ");
    Serial.print(analogRead(A1));
    Serial.print("/");
    Serial.print("U2 : ");
    Serial.println(analogRead(A2));
  } 
}


void clear_disp(int col, int row, int len)
{
  int i;
  lcd.setCursor(col,row);
  for(i = 0 ; i < len ; i++) {
    lcd.print(" ");
  }
}

void print_disp(int col, int row, int num, int len)
{
  int i;
  String temp = (String) num;
  int size = temp.length();
  lcd.setCursor(col,row);
  for (i = 0 ; i < len - size ; i++) {
      lcd.print(" ");
  }
  lcd.print(num);
}

void print_disp(int col, int row, String str, int len)
{
  int i;
  int size = str.length();
  lcd.setCursor(col,row);
  for (i = 0 ; i < len - size ; i++) {
    lcd.print(" ");
  }
  lcd.print(str);
}

void prog_view(int col,int row) {
  lcd.setCursor(col,row);
  if(prog_seq == 0)
    lcd.print("-");
  else if(prog_seq == 1)
    lcd.print("/");
  else 
    lcd.print("|");
  prog_seq = (prog_seq + 1)%3;
}

