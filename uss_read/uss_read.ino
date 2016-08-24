#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

SoftwareSerial UssSerial(12,13);

boolean ENFLAG   = false;
boolean VALFLAG  = false;
boolean INITFLAG = true;
boolean CLICKED  = false;

boolean result[2][3] = {false,false,false,false,false,false};

int  SELMODE   = 0;
int  INFSCROLL = 0;
int  SETSCROLL = 0;
int  DIRFLAG   = 0;
int  index 	   = 0;
int  prog_seq  = 0;
int  tim_count = 0;
int  loop_cnt  = 0;
int  state 	   = 0;
int  errcount  = 0;
long sum[2][3] = {0,0,0,0,0,0};
long avg[2][3] = {0,0,0,0,0,0};
long cnt[2][3] = {0,0,0,0,0,0};


// PASS/FAIL COUNT
long pass_cnt    = 0;
long fail_cnt    = 0;
long part_err[2][3] = {0, 0, 0, 0, 0, 0};



byte str[20];

unsigned long stt_time = 0;

#define TX1 	   0
#define TX2 	   1


// EEPROM variable
int addr = 0; //EEPROM Start address 
int crit[3];


// LCD Module setting
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
  //if (adc_key_in < 650)  return btnLEFT;
  if (adc_key_in < 700)  return btnLEFT;
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
  // put your setup code here, to run once:

	lcd.begin(16, 2);               // 16x2 : LCD Size

	printLCD(0,0,"TX1:RX1/RX2/CEN",15);
	printLCD(0,1,"TX2:RX1/RX2/CEN",15);

	for(int i = 0 ; i < 3 ; i++)
	{
		crit[i] = EEPROM.read(i);
		//crit[i] = crit[i] - (crit[i] % 5);

		if(crit[i] < 100)
			crit[i] = 150;
	}

	pass_cnt = EEPROM.read(8);
	fail_cnt = EEPROM.read(9);

	for(int j = 0 ; j < 2 ; j++)
	{
		for(int k = 0 ; k < 3 ; k++) 
		{
			part_err[j][k] = EEPROM.read(j*3+k+10);
		}
	}

	//Serial.begin(115200);
	//Serial.println("Program started!");

	Serial.begin(115200);
	Serial.print(2);		//init start
	Serial.write(0xC3);		//stop
	Serial.write(0xFA);		//power setting for board jig
}

void loop() {
 	// put your main code here, to run repeatedly:
	procButton();


	if(SELMODE == 0)
	{
		if(ENFLAG)
			stateProcess();

		else if(!INITFLAG)
		{
			if(VALFLAG)
				printAvg();
			else 
			{
				printResult(TX1);
				printResult(TX2);
			}
		}
	}

	else if(SELMODE == 1)
	{
		printMenu(SETSCROLL);
	}

	else if(SELMODE == 2)
	{
		printInfo();
	}

	else if(SELMODE == 3)
	{
		printQclear();
	}

}


void stateProcess()
{
	byte buff;
	
	if((millis() - stt_time)/1000 > tim_count) 
		tim_count += 1;

	if(state == 1 || state == 3)
	{
		if(tim_count > 1) 
		{
			stt_time  = millis();
			tim_count = 0;
			DIRFLAG   = state/2;
			state += 1;
			setStart(DIRFLAG);
		}
	}

	if(state == 2 || state == 4)
	{
		prog_view(15,DIRFLAG);

		if(Serial.available())
		{
			buff  		= Serial.read();
			str[index]  = buff;
			index		= index + 1;
			//Serial.print(buff,HEX)
	
			if(buff == 0xFE || index > 13) {
				parseDist(str);
				index = 0;
			}
		}

		if(tim_count == 5)
		{
			if(state == 2)
			{
				setCancel();
				getAverage(TX1);
				printResult(TX1);
				setStart(TX2);
				printLCD(4,1,"---/---/---",11);
				state = 3;
			}

			else if(state == 4) 
			{
				setCancel();
				getAverage(TX2);
				printResult(TX2);
				writeCount();
				state = 0;
			}

		}

	}
}

void procButton()
{
	int get_btn = read_LCD_buttons();

	//printLCD(15,0,get_btn,1);

	if(CLICKED)
	{
		if(get_btn == btnNONE) {
			VALFLAG  = false;
			CLICKED  = false;
		}
	}

	else if(SELMODE == 0)
	{
		CLICKED = true;

		switch(get_btn) 
		{
			case btnSELECT:
				if(!ENFLAG)
				{
					SELMODE = 1;
					CLICKED = true;
					lcd.clear();
				}

				break;

			case btnLEFT:
				VALFLAG = true;
				break;

			case btnUP:
				state = 0;   INITFLAG = true;
				setCancel(); initLCD();
				Serial.write(0xCA); 
				Serial.print(2);
				Serial.write(0xC3);
				Serial.write(0xFA);
				break;

			case btnDOWN:
				if(state == 0)
				{
					state 	 = 1; 
					INITFLAG = false;
					initLCD(); 
					printLCD(4,0,"---/---/---",11);
					setStart(TX1);
				}

				else
				{
					state 	 = 0;
					INITFLAG = true;
					setCancel();
					initLCD();
				}

				break;

			case btnNONE:
				VALFLAG  = false;
				CLICKED  = false;
				break;

		}
	}

	else if(SELMODE == 1)
	{
		CLICKED = true;

		switch(get_btn) 
		{
			case btnSELECT:
				SELMODE = 2;
				CLICKED = true;
				lcd.clear();
				//initLCD();
				break;

			case btnLEFT:
				SETSCROLL = (SETSCROLL + 2) % 3;
				break;

			case btnRIGHT:
				SETSCROLL = (SETSCROLL + 1) % 3;
				break;

			case btnUP:		//incrase
				if(crit[SETSCROLL] < 255)
					crit[SETSCROLL] += 1;
				EEPROM.write(SETSCROLL,crit[SETSCROLL]);
				break;

			case btnDOWN:	//decrease
				if(crit[SETSCROLL] > 100)
					crit[SETSCROLL] -= 1;
				EEPROM.write(SETSCROLL,crit[SETSCROLL]);
				break;

			case btnNONE:
				CLICKED = false;
				break;
		}
	}

	else if(SELMODE == 2)
	{
		CLICKED = true;

		switch(get_btn) 
		{
			case btnSELECT:
				SELMODE   = 0;
				INFSCROLL = 0;
				lcd.clear();
				initLCD();
				break;

			case btnLEFT:
				INFSCROLL = (INFSCROLL + 7) % 8;
				printLCD(0,1," ",16);
				break;

			case btnRIGHT:
				INFSCROLL = (INFSCROLL + 1) % 8;
				printLCD(0,1," ",16);
				break;

			case btnUP:
				SELMODE = 3;
				CLICKED = true;
				lcd.clear();
				break;

			case btnNONE:
				CLICKED = false;
				break;
		}
	}

	else if(SELMODE == 3)
	{
		CLICKED = true;

		switch(get_btn)
		{
			case btnLEFT:
				SELMODE   = 0;
				INFSCROLL = 0;
				initCount();
				lcd.clear();
				initLCD();
				break;

			case btnRIGHT:
				lcd.clear();
				SELMODE = 2;
				break;

			case btnNONE:
				CLICKED = false;
				break;
		}
	}
}


void parseDist(byte data[])
{
	byte i, upbit, downbit;
	int  dist[3];

	if(str[0] == 0xD1 && str[1] == 0xE1 
		&& str[2] == 0xF1 && index == 14) 
	{
		if(DIRFLAG == data[12]) 
		{
			for(i = 1 ; i < 4 ; i++)
			{
				upbit 	  = data[3*i] 	& 0x0F;
				downbit   = data[3*i+1]	& 0x1F;
				dist[i-1] = upbit * 32 + downbit;

				sum[DIRFLAG][i-1] += dist[i-1];
				cnt[DIRFLAG][i-1] += 1;

				//Serial.print('/');
				//Serial.print(dist[i-1]);
				printLCD(4*i,DIRFLAG,dist[i-1],3);
			}

			//Serial.write('\n');

			if(errcount != 0)
				errcount = 0;
		}
	}

	else if(str[0] == 0xFF && str[1] == 0x56 && str[2] == 0x01)
	{
		//Serial.println("Need Shift");
		Serial.write(0xC5);
	}
	
	else
	{
		errcount++;

		if(errcount > 100)
		{
			//Serial.println("NO DATA");
			printLCD(0,DIRFLAG,"ER2:RX1/RX2/CEN",15);
		}
	}
	
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


void printLCD(int col, int row, String s, int len)
{
	int i;
	int size = s.length();

	lcd.setCursor(col,row);

	for(i = 0 ; i < len-size ; i++)
	{
		lcd.print(' ');
	}

	lcd.print(s);
}

void printLCD(int col, int row, int num, int len)
{
	int i;
	String temp = (String) num;
	int size = temp.length();

	lcd.setCursor(col,row);

	for(i = 0 ; i < len-size ; i++)
	{
		lcd.print(' ');
	}

	lcd.print(num);
}


void printMenu(int idx)
{
	printLCD(1,0,"<SET CRITERIA>",14);

	if(idx == 0)		printLCD(4,1,"RX1:",4);
	else if(idx == 1)	printLCD(4,1,"RX2:",4);
	else if(idx == 2)	printLCD(4,1,"CEN:",4);

	printLCD(8,1,crit[idx],3);
}


void printInfo()
{
	printLCD(2,0,"<CHECK INFO>",12);

	switch(INFSCROLL) 
	{
		case 0:	printLCD(0,1,"PASS CNT:",10);	break;
		case 1: printLCD(0,1,"FAIL CNT:",10);	break;
		case 2: printLCD(0,1,"TX1-RX1 ER:",12); break;
		case 3: printLCD(0,1,"TX1-RX2 ER:",12); break;
		case 4: printLCD(0,1,"TX1-CEN ER:",12); break;
		case 5: printLCD(0,1,"TX2-RX1 ER:",12); break;
		case 6: printLCD(0,1,"TX2-RX2 ER:",12); break;
		case 7: printLCD(0,1,"TX2-CEN ER:",12); break;
	}


	if(INFSCROLL == 0)
		printLCD(12,1,pass_cnt,3);
	else if(INFSCROLL == 1)
		printLCD(12,1,fail_cnt,3);
	else if(INFSCROLL > 1)
		printLCD(12,1,part_err[(INFSCROLL-2)/3][(INFSCROLL-2)%3],3);
}

void printQclear()
{
	printLCD(3,0,"Clear data?",11);
	printLCD(3,1,"YES:L, NO:R",11);
}


void printAvg()
{
	int i,j;

	for(i = 0 ; i < 2 ; i++)
	{
		for(j = 0 ; j < 3 ; j++)
		{
			printLCD(4*(j+1),i,avg[i][j],3);
		}
	}
}


void printResult(int flag)
{
	for(int i = 0 ; i < 3 ; i++)
	{
		if(result[flag][i])
			printLCD((i+1)*4,flag,"OK.",3);
		else
			printLCD((i+1)*4,flag,"NG.",3);
	}
}


void initLCD()
{
	printLCD(0,0,"TX1:RX1/RX2/CEN",15);
	printLCD(0,1,"TX2:RX1/RX2/CEN",15);
}


void setStart(int flag)
{
	ENFLAG 	 = true;
	stt_time = millis();
	cleanBuff();
	Serial.print(2);
	Serial.write(0xC6+flag);
	Serial.write(0xC4);
}


void setCancel()
{
	tim_count   = 0;
	ENFLAG  	= false;
	printLCD(15,0," ",1);
	printLCD(15,1," ",1);
	Serial.write(0xC3);
	Serial.write(0xC6);
}


void cleanBuff()
{
	int i,j;

	for(i = 0 ; i < 2 ; i++)
	{
		for(j = 0 ; j < 3 ; j++) {
			sum[i][j] = 0;
			cnt[i][j] = 0;
		}
	}
}


void getAverage(int flag)
{
	int i;

	//Serial.print('\n');

	for(i = 0 ; i < 3 ; i++)
	{
		avg[flag][i] = sum[flag][i]/cnt[flag][i];
		//Serial.print('/');
		//Serial.print(avg[i][DIRFLAG]);
	}

	//Serial.print('\n');

	for(i = 0 ; i < 3 ; i++)
	{
		if(i == 2) 
		{
			if((avg[flag][i] > crit[i] - 7)
					&& (avg[flag][i] < crit[i] + 7))
				result[flag][i]= true;
			else
				result[flag][i] = false;
		}

		else 
		{
			if((avg[flag][i] > crit[i] - 5)
					&& (avg[flag][i] < crit[i] + 5))
				result[flag][i] = true;
			else
				result[flag][i] = false;
		}
	}

	//for(i = 1 ; i < 4 ; i++)
	//	printLCD(4*i,DIRFLAG,avg[DIRFLAG][i-1],3);	
}

void writeCount()
{
	boolean fail_flag = false;

	for(int i = 0 ; i < 2 ; i++)
	{
		for(int j = 0; j < 3 ; j++)
		{
			if(!result[i][j])
			{
				fail_flag = true;
				part_err[i][j] += 1;
				EEPROM.write(10+i*3+j,part_err[i][j]); 
			}
		}
	}

	if(fail_flag)
	{
		fail_cnt += 1;
		EEPROM.write(9,fail_cnt);
	}

	else
	{
		pass_cnt += 1;
		EEPROM.write(8,pass_cnt);
	}

}


void initCount()
{
	pass_cnt  = 0;
	fail_cnt  = 0;

	EEPROM.write(8,pass_cnt);
	EEPROM.write(9,fail_cnt);

	for(int i = 0 ; i < 2 ; i++) 
	{
		for(int j = 0 ; j < 3 ; j++)
		{
			part_err[i][j] = 0;
			EEPROM.write(10+i*3+j,part_err[i][j]);
		}
	}
}
