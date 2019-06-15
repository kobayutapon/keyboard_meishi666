/* 
 *  Meishi666 Keyboard Input sample
 *  
 */

// for Keyscan
#include "Keyboard.h"
#include <MsTimer2.h>
#include <TimerOne.h>

// for LED Control
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// for OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// #include <misakiUTF16.h>


#define KEY_NUM_MAX   9

#define KEY_NUM_COL   3
#define KEY_NUM_ROW   3

#define KEY_INPUT_ACTIVE    1
#define KEY_INPUT_DEACTIVE  0


#define LED_PIN   1
#define LED_NUM   7


unsigned char key_status_current[KEY_NUM_MAX];
unsigned char key_status_before[KEY_NUM_MAX];

// ---------------------------------------------------------------------------------
//
// KEY MAP table
//  1キーにつき4バイト(=32bit)割り当てる。同時に4キー分を割り当てることが出来る。
//  bit31-bit24     KEY1
//  bit23-bit16     KEY2
//  bit15-biit8     KEY3
//  bit7 - bit0     KEY4
// 
//  未割当の場合は0を割り当てる。とりあえず今はキー一つだけ。
//
// ---------------------------------------------------------------------------------
#define KEYTABLE_PRESET_NUM   4
const unsigned long key_code_preset[KEYTABLE_PRESET_NUM][KEY_NUM_MAX] = {
  {
    // Preset1 for VisualStudio 2017
    KEY_F5,                 // SW1 : Run
    KEY_F10,                // SW2 : Step over
    KEY_F11,                // SW3 : Step in
    KEY_F4,                 // not use 
    0x008180B3,             // SW4 : KEY_LEFT_SHIFT << 16 |  KEY_LEFT_ALT << 8 |  KEY_TAB,  
    KEY_F6,                 // not use
    0x000082B3,             // SW5 : KEY_LEFT_CTRL << 8 |  KEY_TAB,  
    0x000080B3,             // SW6 : KEY_LEFT_ALT  << 8 |  KEY_TAB,  
    0x008182B3,             // SW7 : KEY_LEFT_SHIFT << 16 |  KEY_LEFT_CTRL  << 8 |  KEY_TAB,  
  },
  {
    // Preset 2:Android Studio
    0x00818200 | KEY_F9,    // SW1 : Debug... : KEY_LEFT_SHIFT |  KEY_LEFT_ALT | KEY_F9,  
    KEY_F7,                 // SW2 : Step over
    KEY_F8,                 // SW3 : Step in
    KEY_F4,                 // not use 
    0x008180B3,             // SW4 : KEY_LEFT_SHIFT << 16 |  KEY_LEFT_ALT << 8 |  KEY_TAB,  
    KEY_F6,                 // not use
    0x000082B3,             // SW5 : KEY_LEFT_CTRL << 8 |  KEY_TAB,  
    0x000080B3,             // SW6 : KEY_LEFT_ALT  << 8 |  KEY_TAB,  
    0x008182B3,             // SW7 : KEY_LEFT_SHIFT << 16 |  KEY_LEFT_CTRL  << 8 |  KEY_TAB,  
  },
  {
    // Preset 3
    KEY_F1, 
    KEY_F2, 
    KEY_F3, 
    KEY_F4, 
    KEY_F5, 
    KEY_F6, 
    KEY_F7, 
    KEY_F8, 
    KEY_F9,
  },
  {
    // Preset 4
    KEY_F1, 
    KEY_F2, 
    KEY_F3, 
    KEY_F4, 
    KEY_F5, 
    KEY_F6, 
    KEY_F7, 
    KEY_F8, 
    KEY_F9,
  }

};

// カレントキーテーブル
unsigned long key_code[KEY_NUM_MAX];

// チャタリング除去用のバッファ
volatile unsigned char key_buffer[KEY_NUM_MAX] = {0};

// LED制御用クラス

#define MAX_ANIMATION_TABLE_NUM   8
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);
uint32_t    g_ledState[LED_NUM];

unsigned long g_nTimerCount = 1;
char g_nAnimationCount = 0;

uint32_t    g_ledAnimationTable[MAX_ANIMATION_TABLE_NUM][LED_NUM] = {
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x0000FF00, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000FF00 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000FF00, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000FF00, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x0000FF00, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x0000FF00, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x0000FF00, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
};

// OLED制御用
Adafruit_SSD1306 OLED(128, 32, &Wire, -1); // OLEDドライバ作成
uint8_t font[8];                           // フォント格納バッファ


// プロトタイプ宣言
void setKeyCol(int nLine);
void KeyScanHandler();

// Keytableをセットする
void SetKeyTable(int nPreset)
{
  if ( nPreset < 0 || nPreset >= KEYTABLE_PRESET_NUM ) return;
  
  for( int i=0; i < KEY_NUM_MAX; i++) {
    key_code[i] = key_code_preset[nPreset][i];
  }
}

//
//  10ms タイマーハンドラ
//  キーのスキャン及びチャタリング除去処理を行う
//
void KeyScanHandler()
{
  int i, n = 0;
  for( i=1, n=0; i<=3;i++, n+=3) {
    setKeyCol(i);
    key_buffer[n+0] <<= 1; key_buffer[n+0] |= digitalRead(A3);
    key_buffer[n+1] <<= 1; key_buffer[n+1] |= digitalRead(A2);
    key_buffer[n+2] <<= 1; key_buffer[n+2] |= digitalRead(A1);
    setKeyCol(0);
  }
}

// LED制御ハンドラ(100ms)

void LedCtrlHandler()
{
  if ( g_nAnimationCount >= 0 ) {
    for( int i=0; i < LED_NUM; i++) {
      pixels.setPixelColor(i, g_ledAnimationTable[g_nAnimationCount][i]);
    }
    pixels.show();
    g_nAnimationCount--;
  }

}

void LedUpdateState()
{
  for( int i=0; i < LED_NUM; i++) {
    pixels.setPixelColor(i, g_ledState[i]);
  }
  pixels.show();
}

//
// Keyのカラム制御を行う
//
void setKeyCol(int nLine)
{
  switch(nLine) {
    case 0:
      digitalWrite(4,HIGH);
      digitalWrite(5,HIGH);
      digitalWrite(6,HIGH);
      break;

    case 1:
      digitalWrite(4,LOW);
      digitalWrite(5,HIGH);
      digitalWrite(6,HIGH);
      break;

    case 2:
      digitalWrite(4,HIGH);
      digitalWrite(5,LOW);
      digitalWrite(6,HIGH);
      break;

    case 3:
      digitalWrite(4,HIGH);
      digitalWrite(5,HIGH);
      digitalWrite(6,LOW);
      break;

  }
}

// Put strings to OLED 
void putString(String str)
{
  OLED.clearDisplay();  
  OLED.setRotation(2);
  OLED.setCursor(0,0);
  OLED.println(str);
  OLED.display();
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  // Key Matrix
  pinMode(A3,INPUT_PULLUP);
  pinMode(A2,INPUT_PULLUP);
  pinMode(A1,INPUT_PULLUP);

  pinMode(4,OUTPUT);
  pinMode(5,OUTPUT);
  pinMode(6,OUTPUT);

  // Initialize LED Matrix
  pixels.begin();
  for( int i=0; i<LED_NUM;i++) {
    g_ledState[i] = pixels.Color(0,255,0);
  }
  LedUpdateState();

  // Initialize OLED
  if(!OLED.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Cannot work."));
    for(;;); 
  }
  OLED.clearDisplay();  
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);
  putString("meishi666");

  // Initialize Key State
  setKeyCol(0);
  for( int i=0; i<KEY_NUM_MAX; i++ ) {
    key_status_current[i] = KEY_INPUT_DEACTIVE;
    key_status_before[i] = KEY_INPUT_DEACTIVE;
  }

  SetKeyTable(0);

  Keyboard.begin();

  // キー監視割込み(10msec)
  MsTimer2::set(10, KeyScanHandler);
  MsTimer2::start();
  Timer1.initialize(50000);
  Timer1.attachInterrupt(LedCtrlHandler);
  Timer1.start();


}

void loop() {  
  // Key input check
  unsigned long keycode;

  // チャタリング除去
  for( int i=0; i < KEY_NUM_MAX; i++) {
    if (( key_buffer[i] & 0x0f) == 0x0f ){
      key_status_current[i] = KEY_INPUT_DEACTIVE;
    } else if ( (key_buffer[i] & 0x0f) == 0x00 ) {
      key_status_current[i] = KEY_INPUT_ACTIVE;
    }
  }

  // キーイベントの発生
  for( int i=0; i<KEY_NUM_MAX; i++) {
    if ( key_status_before[i] != key_status_current[i] ){
      if ( key_status_current[i] == KEY_INPUT_ACTIVE) {

        keycode = (key_code[i] >> 24) & 0xff;   Keyboard.press(keycode);
        keycode = (key_code[i] >> 16) & 0xff;   Keyboard.press(keycode);
        keycode = (key_code[i] >> 8) & 0xff;    Keyboard.press(keycode);
        keycode = (key_code[i] >> 0) & 0xff;    Keyboard.press(keycode);
        
      } else {
        keycode = (key_code[i] >> 24) & 0xff;   Keyboard.release(keycode);
        keycode = (key_code[i] >> 16) & 0xff;   Keyboard.release(keycode);
        keycode = (key_code[i] >> 8) & 0xff;    Keyboard.release(keycode);
        keycode = (key_code[i] >> 0) & 0xff;    Keyboard.release(keycode);
        g_nAnimationCount = MAX_ANIMATION_TABLE_NUM - 1;

      }
    }
  }

  // Update status
  for( int i=0; i<KEY_NUM_MAX; i++) {
    key_status_before[i] = key_status_current[i];
  }

  // キーコードの動的変更処理
  int rcvcmd;
  if ( Serial.available() > 0 ) {
    rcvcmd = Serial.read();
    if ( rcvcmd == '0' ) {
      Keyboard.releaseAll();
      SetKeyTable(0);
      putString("Preset 1");
    } else if ( rcvcmd == '1' ) {
      Keyboard.releaseAll();
      SetKeyTable(1);
      putString("Preset 2");
    } else if ( rcvcmd == '2' ) {
      SetKeyTable(2);
      putString("Preset 3");
    }else if ( rcvcmd == '3' ) {
      SetKeyTable(3);
      putString("Preset 4");
    }
  } 
}
