#include <LiquidCrystal.h>
#include <EEPROM.h>

// Port for Motor control
int BACKLIGHT = 10;
int PWM = 11;
int DIR = 12;
int EN  = 13;

int HALL1 = 2;
int HALL2 = 3;

boolean CH2EN = false;
boolean ISMOVE = false;

// parameter for loop
int state = 0;
unsigned long int start_time = 0;
unsigned long int click_time = 0;

volatile long hall_cnt[2] = {0, 0};

enum rotate { CW, CCW };
rotate MOTORDIR = CW;
boolean MOTORON = false;

boolean SETMODE = false;
boolean SET_IRQ = false;

int CURR_UPPER = 200;
int CURR_LOWER = 100;
int TESTLENGTH = 5;
int RPM_UPPER = 72; // x100
int RPM_LOWER = 70; // x100

// for LCD-key module
boolean btnClicked = false;
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
  if(!CH2EN)   
    Serial.begin(115200);

  // LCD Settings
  lcd.begin(16, 2);               // 16x2 : LCD Size
  print_disp(0,0, "CW",   2);
  print_disp(0,1, "CCW",  3);
  print_disp(11,0,"READY",5);

  pinMode(BACKLIGHT,OUTPUT);
  pinMode(PWM  , OUTPUT);
  pinMode(DIR  , OUTPUT);
  pinMode(EN   , OUTPUT);
  pinMode(HALL1, INPUT );
  pinMode(HALL2, INPUT );

  digitalWrite(BACKLIGHT, HIGH);
  digitalWrite(DIR,HIGH);
  digitalWrite(EN,LOW);

  // setting value
  if(EEPROM.read(1) > 250)
    EEPROM.write(1, 200);
  if(EEPROM.read(2) > 250)
    EEPROM.write(2, 100);
  if(EEPROM.read(3) > 250)
    EEPROM.write(3,  72);
  if(EEPROM.read(4) > 250)
    EEPROM.write(4,  70);
  if(EEPROM.read(5) > 250)
    EEPROM.write(5,  15);

  CURR_UPPER = EEPROM.read(1);
  CURR_LOWER = EEPROM.read(2);
  RPM_UPPER  = EEPROM.read(3);
  RPM_LOWER  = EEPROM.read(4);
  TESTLENGTH = EEPROM.read(5);

  // hall sensor interrupt set
  attachInterrupt(0, hall_countup, FALLING);
  attachInterrupt(1, hall_countup2, FALLING);
}

void loop() 
{
  if(SETMODE)
    proc_setmode();

  else
  {
    motor_loop();
    proc_state();
  }

  backlight_check();
}


void proc_state()
{
  static int prev_state = 0;
  static int lcd_row = 0;

  // measure current
  static int curr_cnt[2] = {0, 0};
  static long curr_sum[2] = {0, 0};
  static int curr_avg[2][2] = {0, 0};
  int curr[2];

  // measure RPM
  int pole = 36;
  static int rpm[2][2] = {0, 0};

  key_interrupt();

  if(prev_state != state)
  {
    Serial.print("State:");
    Serial.print(state);
    Serial.print(", DIR:");
    Serial.println(MOTORDIR);

    switch(state)
    {
      case 0:
        print_init();
        MOTORON = false;
        break;

      case 1:
        clear_disp(4,0,12);
        clear_disp(4,1,12);
        print_disp(4, 0, "MOTOR ON...",11);
        lcd_row = 0;
        start_time = millis();
        MOTORDIR = CW;
        MOTORON = true;
        break;

      case 2:
        MOTORON = false;
        print_result(0, curr_avg[CW], rpm[CW]);
        break;

      case 3:
        lcd_row = 1;
        print_disp(4, 1, "MOTOR ON...",11);
        start_time = millis();
        MOTORDIR = CCW;
        MOTORON = true;
        break;

      case 4:
        MOTORON = false;
        print_result(0, curr_avg[CW], rpm[CW]);
        print_result(1, curr_avg[CCW], rpm[CCW]);
        break;

      case 5:
        clear_disp(4,0,12);
        clear_disp(4,1,12);
        print_curr(curr_avg);
        break;

      case 6:
        clear_disp(4,0,12);
        clear_disp(4,1,12);
        print_rpm(rpm);
        break;
    }

    prev_state = state;
  }

  else
  {
    switch(state)
    {
      case 1:
        if(millis() - start_time > TESTLENGTH * 1000 + 3000)
        {
          curr_avg[CW][0] = curr_sum[0] / curr_cnt[0];
          curr_avg[CW][1] = curr_sum[1] / curr_cnt[1];
 
          rpm[CW][0] = hall_cnt[0]*60/(TESTLENGTH)/(pole/2);
          rpm[CW][1] = hall_cnt[1]*60/(TESTLENGTH)/(pole/2);

          state = 2;

          curr_cnt[0] = 0;
          curr_cnt[1] = 0;
          curr_sum[0] = 0;
          curr_sum[1] = 0;
          hall_cnt[0] = 0;
          hall_cnt[1] = 0;
          
          click_time = millis();
        }

        else if(millis() - start_time < 3000)
        {
          hall_cnt[0] = 0;
          hall_cnt[1] = 0;
        }

        else
        {
          curr[0] = 5000.0/1024.0*analogRead(A5) - 2500;
          curr[1] = 5000.0/1024.0*analogRead(A4) - 2500;

          print_disp(4, lcd_row, curr[0], 4);
          print_disp(8, lcd_row, "/", 1);
          print_disp(9, lcd_row, curr[1], 4);
          print_disp(14, lcd_row, (millis()-start_time)/1000-3,2);

          curr_cnt[0] += 1;
          curr_cnt[1] += 1;
          curr_sum[0] = curr_sum[0] + curr[0];
          curr_sum[1] = curr_sum[1] + curr[1];
        }

        break;

      case 2:
        if(!ISMOVE)
          state = 3;
        break;

      case 3:
        if(millis() - start_time > TESTLENGTH * 1000 + 3000)
        {
          curr_avg[CCW][0] = curr_sum[0] / curr_cnt[0];
          curr_avg[CCW][1] = curr_sum[1] / curr_cnt[1];

          rpm[CCW][0] = hall_cnt[0]*60/(TESTLENGTH)/(pole/2);
          rpm[CCW][1] = hall_cnt[1]*60/(TESTLENGTH)/(pole/2);

          state = 4;

          curr_cnt[0] = 0;
          curr_cnt[1] = 0;
          curr_sum[0] = 0;
          curr_sum[1] = 0;
          hall_cnt[0] = 0;
          hall_cnt[1] = 0;
          
          click_time = millis();
        }

        else if(millis() - start_time < 3000)
        {
          hall_cnt[0] = 0;
          hall_cnt[1] = 0;
        }

        else
        {
          curr[0] = 5000.0/1024.0*analogRead(A5) - 2500;
          curr[1] = 5000.0/1024.0*analogRead(A4) - 2500;

          print_disp(4, lcd_row, curr[0], 4);
          print_disp(8, lcd_row, "/", 1);
          print_disp(9, lcd_row, curr[1], 4);
          print_disp(14, lcd_row, (millis()-start_time)/1000-3,2);

          curr_cnt[0] += 1;
          curr_cnt[1] += 1;
          curr_sum[0] = curr_sum[0] + curr[0];
          curr_sum[1] = curr_sum[1] + curr[1];
        }

        break;

      case 4:

        break;
    }
  }

}


void proc_setmode()
{
  static int idx = 0;

  if(SET_IRQ)
  {
    print_setmode(idx);
    SET_IRQ = false;
  }

  setmode_ctrl(idx);
}


void motor_loop()
{
  static int speed = 0;
  static int prev_speed = 0;
  int step = 1;

  // speed auto-control 
  if(MOTORON)
  {
    if(speed < 255 - step)
      speed += step;
    else
      speed = 255;
  }

  else
  {
    if(speed < step)
      speed = 0;
    else
      speed -= step;
  }

  // motor operation
  if(speed != prev_speed)
  {
    analogWrite(PWM,speed);
    delay(5);
  }

  if(MOTORDIR == CW)
    digitalWrite(DIR,HIGH);
  else
    digitalWrite(DIR,LOW);

  if(speed != 0)
    ISMOVE = true;
  else
    ISMOVE = false;
}


void key_interrupt()
{
  // key interrupt
  lcd_key = read_LCD_buttons();

  if(lcd_key == btnNONE)
  {
    btnClicked = false;
    
    if(state > 4)
      state = 4;
  }

  else if(!btnClicked)
  {
    click_time = millis();
    btnClicked = true;
    
    if(lcd_key == btnDOWN) 
    {
      if(state == 0 || state == 4) 
        state = 1;

      else
        state = 0;
    }

    else if(lcd_key == btnSELECT && state != 1)
    {
      SETMODE = true;
      print_setmode(0);
    }

  }

  else
  {
    if(lcd_key == btnLEFT)
    {
      if(state == 4)
        state = 5;
    }

    if(lcd_key == btnRIGHT)
    {
      if(state == 4)
        state = 6;
    }
  }
}


void setmode_ctrl(int& idx)
{
  lcd_key = read_LCD_buttons();
  
  if(lcd_key == btnNONE)
    btnClicked = false;
  
  else if(!btnClicked)
  {
    click_time = millis();
    btnClicked = true;
    SET_IRQ = true;

    switch(lcd_key)
    {
      case btnSELECT:
        SETMODE = false;
        print_init();
        break;

      case btnLEFT:
        idx = (idx + 4) % 5;
        break;

      case btnRIGHT:
        idx = (idx + 1) % 5;
        break;

      case btnUP:
        if(idx == 0)    CURR_UPPER = (CURR_UPPER + 1) % 250;
        else if(idx == 1) CURR_LOWER = (CURR_LOWER + 1) % 250;
        else if(idx == 2) RPM_UPPER  = (RPM_UPPER  + 1) % 250;
        else if(idx == 3) RPM_LOWER  = (RPM_LOWER  + 1) % 250;
        else if(idx == 4) TESTLENGTH = (TESTLENGTH + 1) % 250;
        write_value();
        break;

      case btnDOWN:
        if(idx == 0)    CURR_UPPER = (CURR_UPPER + 249) % 250;
        else if(idx == 1) CURR_LOWER = (CURR_LOWER + 249) % 250;
        else if(idx == 2) RPM_UPPER  = (RPM_UPPER  + 249) % 250;
        else if(idx == 3) RPM_LOWER  = (RPM_LOWER  + 249) % 250;
        else if(idx == 4) TESTLENGTH = (TESTLENGTH + 249) % 250;
        write_value();
        break;
    }
  }
}


void backlight_check()
{
  if(millis() - click_time > 5000 && state == 0)
    backlight_en(false);
  else
    backlight_en(true);
}


void clear_disp(int col, int row, int len)
{
  int i;
  lcd.setCursor(col,row);
  
  for(i = 0 ; i < len ; i++) 
  {
    lcd.print(" ");
  }
}


void print_disp(int col, int row, int num, int len)
{
  int i;
  String temp = (String) num;
  int size = temp.length();
  lcd.setCursor(col,row);

  for (i = 0 ; i < len - size ; i++) 
  {
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

void print_setmode(int idx)
{
  lcd.clear();

  switch(idx)
  {
    case 0:
      print_disp(1,0,"<CURR UPPER>",13);
      print_disp(6,1,CURR_UPPER,3); break;

    case 1:
      print_disp(1,0,"<CURR LOWER>",13);
      print_disp(6,1,CURR_LOWER,3); break;

    case 2:
      print_disp(1,0,"<RPM UPPER>",13);
      print_disp(6,1,RPM_UPPER * 100,3); break;

    case 3:
      print_disp(1,0,"<RPM LOWER>",13);
      print_disp(6,1,RPM_LOWER * 100,3); break;

    case 4:
      print_disp(0,0,"<TEST TIME LENG>",16);
      print_disp(4,1,TESTLENGTH,3);
      print_disp(7,1,"(s)",3); break;
  } 
}

void print_init()
{
  lcd.clear();
  print_disp(0,0, "CW",   2);
  print_disp(0,1, "CCW",  3);
  print_disp(11,0,"READY",5);
}

void print_result(int row, int* avg, int* rpm_v)
{
  if(CURR_UPPER >= abs(avg[0]) && CURR_LOWER <= abs(avg[0]))
    print_disp(4, row, "OK/", 3);
  else
    print_disp(4, row, "NG/", 3);
  
  if(CURR_UPPER >= abs(avg[1]) && CURR_LOWER <= abs(avg[1]))
    print_disp(7, row, "OK||", 4);
  else
    print_disp(7, row, "NG||", 4);
  
  if(RPM_UPPER * 100 >= rpm_v[0] && RPM_LOWER * 100 <= rpm_v[0])
    print_disp(11, row, "OK/", 3);
  else
    print_disp(11, row, "NG/", 3);
  
  if(RPM_UPPER * 100 >= rpm_v[1] && RPM_LOWER * 100 <= rpm_v[1])
    print_disp(14, row, "OK", 2);
  else
    print_disp(14, row, "NG", 2);

  Serial.print(row);
  Serial.print(" ROW:");
  Serial.print(avg[0]);
  Serial.print(",");
  Serial.print(avg[1]);
  Serial.print("/");
  Serial.print(rpm_v[0]);
  Serial.print(",");
  Serial.println(rpm_v[1]);
}


void print_curr(int curr[][2])
{
  print_disp(4,  CW, curr[CW ][0], 4);
  print_disp(9,  CW, curr[CW ][1], 5);
  print_disp(4, CCW, curr[CCW][0], 4);
  print_disp(9, CCW, curr[CCW][1], 5);
}


void print_rpm(int rpm_v[][2])
{
  print_disp(4,  CW, rpm_v[CW ][0], 4);
  print_disp(9,  CW, rpm_v[CW ][1], 5);
  print_disp(4, CCW, rpm_v[CCW][0], 4);
  print_disp(9, CCW, rpm_v[CCW][1], 5);
}


void prog_view(int col,int row) {

  static int prog_seq = 0;

  lcd.setCursor(col,row);

  if(prog_seq == 0)
    lcd.print("-");
  else if(prog_seq == 1)
    lcd.print("/");
  else 
    lcd.print("|");

  prog_seq = (prog_seq + 1) % 3;
}

void backlight_en(boolean s)
{
  digitalWrite(BACKLIGHT,s);
}

void write_value()
{
  EEPROM.write(1, CURR_UPPER);
  EEPROM.write(2, CURR_LOWER);
  EEPROM.write(3,  RPM_UPPER);
  EEPROM.write(4,  RPM_LOWER);
  EEPROM.write(5, TESTLENGTH);
}


void hall_countup()
{
  hall_cnt[0]++;
}


void hall_countup2()
{
  hall_cnt[1]++;
}