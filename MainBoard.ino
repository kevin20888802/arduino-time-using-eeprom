/* 
 * TimeSerial.pde
 * example code illustrating Time library set through serial port messages.
 *
 * Messages consist of the letter T followed by ten digit time (as seconds since Jan 1 1970)
 * you can send the text on the next line using Serial Monitor to set the clock to noon Jan 1 2013
 T1357041600  //預設時間是2013年 1月 1日 
 T135707000
 *
 * A Processing example sketch to automatically send the messages is included in the download
 * On Linux, you can use "date +T%s\n > /dev/ttyACM0" (UTC time zone)
 */ 

// 以上為原始範例程式說明原文
#include <Keypad.h>    // 引用Keypad程式庫
#include <TimeLib.h> // 匯入時間的函式庫
#include <EEPROM.h> //儲存時間的EEPROM記憶體函式庫
#include <Wire.h> // 匯入LCD螢幕函式庫 1
#include <LiquidCrystal_I2C.h> // 匯入LCD螢幕函式庫 2
#include <Adafruit_Sensor.h> // 匯入溫溼度感測器函式庫 1
#include <DHT.h> // 匯入溫溼度感測器函式庫 2
#include <DHT_U.h> // 匯入溫溼度感測器函式庫 3

//---------------------螢幕參數-------------------

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

//--------------------感測器參數-----------------
#define DHTPIN            4         // 接溫溼度感測器(DHT33)接角 
#define DHTTYPE           DHT22     // DHT 22 (AM2302)

DHT_Unified dht(DHTPIN, DHTTYPE);

uint32_t delayMS;

String Temperature = "0";
String Humidity = "0";

//---------------------鍵盤參數-------------------

#define KEY_ROWS 4 // 按鍵模組的列數
#define KEY_COLS 4 // 按鍵模組的行數

// 依照行、列排列的按鍵字元（二維陣列）
char keymap[KEY_ROWS][KEY_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte colPins[KEY_COLS] = {9, 8, 7, 6};     // 按鍵模組，行1~4接腳。
byte rowPins[KEY_ROWS] = {13, 12, 11, 10}; // 按鍵模組，列1~4接腳。

// 初始化Keypad物件
// 語法：Keypad(makeKeymap(按鍵字元的二維陣列), 模組列接腳, 模組行接腳, 模組列數, 模組行數)
Keypad myKeypad = Keypad(makeKeymap(keymap), colPins, rowPins, KEY_ROWS, KEY_COLS);

//---------------------時間參數-------------------
#define TIME_HEADER  "T"   // Header tag for serial time sync message // 在序列埠前面第一個能觸發時間校正的文字
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 
#define TIMESAVEADDR 0 //儲存時間的記憶體位址
#define STARTTIMEADDR 4
#define ENDTIMEADDR 6


unsigned long SAVEDADD = 0;

// 校正時間 = 序列埠輸入 T + 自從1970年1月1日到現在所經過的毫秒數(ms)

//處理開關燈時間
unsigned int OpenStart = 17; //開燈時間 (從OpenStart往24數)
unsigned int OpenEnd = 18;  //關燈時間 (從0往OpenEnd數)
// 不在範圍內則燈不開
// 在範圍內則燈開    

//---------------------輸出參數-------------------
#define LIGHTOUTPIN 2 // 燈輸出腳位
                    // 請改成接燈(或繼電器控制)的腳位
                    

void setup()  {
  Serial1.begin(9600); // 雲端用port
  Serial.begin(9600);
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;
  SetupLCD();
  if((EEPROM.read(STARTTIMEADDR) != 0))
  {
    EEPROM.get(STARTTIMEADDR , OpenStart);
  }
  if((EEPROM.read(ENDTIMEADDR) != 0))
  {
    EEPROM.get(ENDTIMEADDR , OpenEnd);
  }
     pinMode(LIGHTOUTPIN, OUTPUT); 
  //while (!Serial) ; // Needed for Leonardo only

  // setSyncProvider( requestSync);  //set function to call when sync required
  // Serial.println("Waiting for sync message");
}

//有沒有處理過EEPROM的儲存值
//設定了就變TRUE代表處理過了
bool SetDefaulted = false;
String LastSec = "0";
void loop(){    
  if (Serial.available()) {
    // 處理序列埠輸入
    processSyncMessage(); 
  }

  // 如果有EEPROM有儲存時間，就設定儲存的時間。
  if((EEPROM.read(TIMESAVEADDR) != 0)) 
  { 
    if(!SetDefaulted)
    {     
      EEPROM.get(TIMESAVEADDR,SAVEDADD);
      setTime(SAVEDADD);
      SAVEDADD += 1;
      delay(50);
      SetDefaulted = true;
    }
  }

  // 序列埠顯示時間傳給電腦校正程式用
  if (timeStatus()!= timeNotSet) { 
    digitalClockDisplay();  
  }
  LightControl();
  ScreenDisplay();
  KeyInputs();
  SAVEDADD = now();
  EEPROM.put(TIMESAVEADDR,SAVEDADD);
  if(LastSec.toInt() <= int(second()) + 1)
  {
      GetTempAndHumid();
      LastSec = String(second());
  }
  SendDataToCloud();
}

void GetTempAndHumid()
{
  sensors_event_t event;  
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) 
  {
      //Ln[0] = "Temp: Error";
  }
  else 
  {
      Temperature = String(event.temperature);
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) 
  {
     //Ln[1] = "Humid: Error";
  }
  else 
  {
     Humidity = String(event.relative_humidity);
  }
}

void SendDataToCloud()
{
  String OutData = "";
  OutData = Temperature + "," + Humidity + "," + (String(year()) + "/" + String(month()) + "/" + String(day()) + "/" + String(hour()) + ":" + String(minute()) + ":" + String(second())) +"\n";
  Serial1.print(OutData);
  delay(50);
  Serial1.flush();
}

int NowMenuCount = 4;
int MenuCount = 4;
String Menus[] = {"System Time", "Adjust Time" , "Set OpenTime" , "TempAndHumid" , ""};


int NowMenu = -1;
int NowBtn = 0;

int STT[]= {0,0,0,0,0,0}; // SetTimeTable
int OTT[]= {0,0,0,0,0,0}; // OldTimeTable


void ScreenDisplay()
{
  lcd.clear();

   String TimeTable[] = {String(year()),String(month()), String(day()) , String(hour()) , String(minute()) , String(second())};
        String Ln[] = {"" , ""};
  switch(NowMenu)
  {
    case -1: // 主目錄
    for(int i = NowBtn; i <= NowBtn + 1; i++)
    {
    lcd.setCursor(0,i - NowBtn);
    if(i == NowBtn)
    {
       lcd.print("> " + Menus[i]);   
    }
    else
    {
      lcd.print(Menus[i]);  
    }
    }
    break;
    case 0: // 顯示系統時間
     lcd.setCursor(0,0);
     lcd.print(String(year()) + "/" + String(month()) + "/" + String(day()) );
     lcd.setCursor(0,1);
     lcd.print(String(hour()) + ":" + String(minute()) + ":" + String(second()));
     break;
    case 1: // 調整時間
    

     for(int i = 0 ; i < 6; i++)
     {
      int Lin = 0;
      String SplitChar = "";
      if(i > 2)
      {
         Lin = 1;
         SplitChar = ":";
      }
      else
      {
         Lin = 0;
         SplitChar = "/";
      }
      if(i == NowBtn)
      {
        Ln[Lin] = Ln[Lin] + ">" + TimeTable[i] + "<";
      }
      else
      {
        Ln[Lin] = Ln[Lin] + TimeTable[i];
      }
      if((i != 2) & (i != 5))
      {
        Ln[Lin] = Ln[Lin] + SplitChar;
      }
     }
     for(int i = 0 ; i < 2; i++)
     {
        lcd.setCursor(0,i);
        lcd.print(Ln[i]);
     }
     break;
    case -2:        
     for(int i = 0 ; i < 6; i++)
     {
      int Lin = 0;
      String SplitChar = "";
      if(i > 2)
      {
         Lin = 1;
         SplitChar = ":";
      }
      else
      {
         Lin = 0;
         SplitChar = "/";
      }
      if(i == NowBtn)
      {
        Ln[Lin] = Ln[Lin] + ">" + STT[i] + "<";
      }
      else
      {
        Ln[Lin] = Ln[Lin] + STT[i];
      }
      if((i != 2) & (i != 5))
      {
        Ln[Lin] = Ln[Lin] + SplitChar;
      }
     }
     for(int i = 0 ; i < 2; i++)
     {
        lcd.setCursor(0,i);
        lcd.print(Ln[i]);
     }
     break;
     case 2:
        Ln[0] = "StartHour:"  + String(OpenStart);
        Ln[1] = "  EndHour:"  + String(OpenEnd);
        for(int i = 0; i < 2; i++)
        {
          if(i == NowBtn)
          {
            Ln[i] = Ln[i] + "  <<<";
          }
          lcd.setCursor(0,i);
          lcd.print(Ln[i]);
        }
        break;
    case 3:
          Ln[0] = "Temp: " + String(Temperature) + " *C";
          Ln[1] = "Humid: " + String(Humidity) + " %";
        for(int i = 0 ; i < 2; i++)
        {
        lcd.setCursor(0,i);
        lcd.print(Ln[i]);
        }
        break;
    default:
    break;
  }
  delay(50); 
}

void SetTimeDone()
{
  //設定新的時間
  setTime(STT[3] , STT[4] , STT[5] ,STT[2] , STT[1] , STT[0]);
  SAVEDADD = now();
  EEPROM.put(TIMESAVEADDR,SAVEDADD);
  //Serial.println(TIME_HEADER + String(SAVEDADD));
}

void KeyInputs()
{
  char key = myKeypad.getKey();
  switch(key)
  {
    case 'D': // 確認鍵
        switch(NowMenu)
        {
          case 1: // 進入調整時間
            NowMenu = -2;
            STT[0] = int(year()); STT[1] = int(month()); STT[2] = int(day()) ; STT[3] = int(hour()) ; STT[4] = int(minute()) ; STT[5] = int(second());
            //OTT[0] = int(year()); OTT[1] = int(month()); OTT[2] = int(day()) ; OTT[3] = int(hour()) ; OTT[4] = int(minute()) ; OTT[5] = int(second());
            //EEPROM.get(TIMESAVEADDR,oldpctime);
            break;
          case 2:
            NowMenu = 2;
            EEPROM.put(STARTTIMEADDR , OpenStart);
            EEPROM.put(ENDTIMEADDR , OpenEnd);
            break;
          case -2: // 確認調整時間
            SetTimeDone();
            NowMenu = 1;
            break;
          default: // 其他確認
            NowMenu = NowBtn;
            if(NowMenu == 1)
            {
              NowMenuCount = 6;
            }
            else if(NowMenu == 2)
            {
              NowMenuCount = 2;
            }
            else
            {
              NowBtn = 0;     
            }
            break;
        }
        break;
    case 'A': // 回主目錄鍵
        NowBtn = 0;
        NowMenu = -1;
        NowMenuCount = MenuCount;
        EEPROM.write(STARTTIMEADDR , OpenStart);
        EEPROM.write(ENDTIMEADDR , OpenEnd);
        break;
    case 'C': // 上選鍵
        if(NowMenu != -2)
        {
              if(NowBtn < NowMenuCount - 1)
              {
               NowBtn += 1;
              } 
              else
              {
               NowBtn = 0;
              }
       }
        break;
    case 'B': // 下選鍵
        if(NowMenu != -2)
        {
          if(NowBtn > 0)
          {
            NowBtn -= 1;
          }
          else
          {
            NowBtn = NowMenuCount - 1;
          }
        }
        break;
    default:
        if(key)
        {
          if(NowMenu == -2) // 調整時間時輸入數字
          {
             if(key == '#') // 清除一位數
             {
               STT[NowBtn] /= 10;
             }
             else if(key == '*') // 清除全部
             {
               STT[NowBtn] = 1;
             }
             else
             {
               STT[NowBtn] *= 10;
               STT[NowBtn] = STT[NowBtn] + (int(key - '0'));
             }
          }
          if(NowMenu == 2)
          {
             if(key == '#') // 清除一位數
             {
              if(NowBtn == 0)
              {
                 OpenStart /= 10;
              }
              else
              {
                 OpenEnd /= 10;
              }
             }
             else if(key == '*') // 清除全部
             {
               if(NowBtn == 0)
               {
                 OpenStart = 0;
               }
               else
               {
                 OpenEnd = 0;
               }
             }
             else
             {
               if(NowBtn == 0)
               {
                 OpenStart *= 10;
                 OpenStart = OpenStart + (int(key - '0'));
               }
               else
               {
                 OpenEnd *= 10;
                 OpenEnd = OpenEnd + (int(key - '0'));
               }
             }
             EEPROM.write(STARTTIMEADDR , OpenStart);
             EEPROM.write(ENDTIMEADDR , OpenEnd);
          }
        }

        break;
  }
}

void SetupLCD()
{
  lcd.begin(16, 2);
  for(int j=0;j<3;j++) 
  {
    lcd.backlight(); // 開啟背光
    delay(300);
    lcd.noBacklight(); // 關閉背光
    delay(300);
  }
  lcd.backlight();
  lcd.setCursor(0,0); // 設定游標位置在第一行行首
  lcd.print("IT FUCKING STARTS!!");
  delay(300);
  lcd.clear();
  lcd.setCursor(0,1); // 設定游標位置在第二行行首
  lcd.print("Plant Factory.");
  delay(500);
  lcd.clear();
  lcd.setCursor(0,1); // 設定游標位置在第二行行首
  lcd.print("Plant Factory..");
  delay(500);
  lcd.clear();
  lcd.setCursor(0,1); // 設定游標位置在第二行行首
  lcd.print("Plant Factory...");
  delay(500);
  //lcd.clear(); //顯示清除
  
}

// 序列埠顯示時間傳給電腦校正程式用
void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.print(" ");
 /* unsigned long SAVEDADD = 0;
  EEPROM.get(TIMESAVEADDR,SAVEDADD);
  Serial.print(SAVEDADD);*/
  Serial.println(); 
}
              
void LightControl()
{
  int NowHour = 0; // 現在幾點
  NowHour = int(hour()); // 取得現在幾點
  int RealStart = OpenStart;
  int RealEnd = OpenEnd;
  if(OpenStart > OpenEnd)
  {
    RealEnd += 24;
  }
  if((NowHour >= RealStart) & (NowHour < RealEnd))
  {
    digitalWrite(LIGHTOUTPIN , 1);
  }
  else
  {
    digitalWrite(LIGHTOUTPIN ,0); 
  }
}

//範例的分秒處理
void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}


//範例的處理輸入
void processSyncMessage() {
  unsigned long pctime;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     if( pctime >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013) 
          EEPROM.put(TIMESAVEADDR,pctime); // 如果有得到時間校正就儲存
       setTime(pctime); // Sync Arduino clock to the time received on the serial port
     }
  }
}


//範例的時間請求
time_t requestSync()
{
  Serial.write(TIME_REQUEST);  
  return 0; // the time will be sent later in response to serial mesg
}
