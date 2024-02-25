#include <M5Core2.h>
#include "MFRC522_I2C.h"
#include <Adafruit_NeoPixel.h>

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>

/**RFID**/
MFRC522_I2C mfrc522(0x28, -1); // Create MFRC522 instance.

/**LED**/
#define PIN 26
#define NUMPIXELS 21                                            // LEDの数を指定
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800); // 800kHzでNeoPixelを駆動

int ledBrightness = 0; // LEDの明るさカウンタ
int ledPosition = 0;   // LEDの位置カウンタ

// LED番号再割り当て
int thumbLine[5] = {2, 1, 0};
int indexFingerLine[5] = {7, 6, 5, 4, 3};
int middleFingerLine[5] = {8, 9, 10, 11, 12};
int ringFingerLine[5] = {17, 16, 15, 14, 13};
int littleFingerLine[5] = {18, 19, 20};

// 演出制御
bool isCard = false; // カード読み取りフラグ
#define ace "155 43 9 12"
int CardID = 0;
float TotalTime = 5; // 演出合計時間
unsigned long startMillis = 0;
unsigned long currentMillis = 0;
unsigned long previousLEDTime = 0;
unsigned long previousLCDTime = 0;

// 待機画面
static LGFX lcd;                      // LGFXのインスタンスを作成。
LGFX_Sprite spriteBase(&lcd);         // スプライトを使う場合はLGFX_Spriteのインスタンスを作成。
LGFX_Sprite spriteImage(&spriteBase); // スプライトを使う場合はLGFX_Spriteのインスタンスを作成。
LGFX_Sprite spriteWord1(&spriteBase); // スプライトを使う場合はLGFX_Spriteのインスタンスを作成。
LGFX_Sprite spriteWord2(&spriteBase); // スプライトを使う場合はLGFX_Spriteのインスタンスを作成。
LGFX_Sprite spriteWord3(&spriteBase); // スプライトを使う場合はLGFX_Spriteのインスタンスを作成。
LGFX_Sprite spriteWord4(&spriteBase); // スプライトを使う場合はLGFX_Spriteのインスタンスを作成。
void wait_display_setup();
void wait_display();

// 回転演出
#define ace "1d f4 df 06"
#define jack "1d 75 01 55"
#define queen "1d fd 6e 58"
#define king "1d e1 27 55"
void rotate_display(float diff);

// 加速度リセット
float baseAccX, baseAccY, baseAccZ = 0; // 基準加速度格納用
float accX, accY, accZ;                 // 加速度格納用
float diffAccX, diffAccY, diffAccZ = 0; // 現在値格納用
float maxAccX, maxAccY, maxAccZ = 0;
bool reset_flg = false;
void check_acc();
void zeroSet();
void shakeReset();

void Fingertip2Wrist(int ledPosition, int ledBrightness);
bool isNewCard();
int identifyCard();
void LEDcontrol(int B, unsigned long C, unsigned long D);
void LCDcontrol(int B, unsigned long C, unsigned long D);
void uid_display_proc();

// ---------------------------------------------------------------
void setup()
{
  pixels.begin(); // NeoPixelの初期化
  pixels.clear(); // NeoPixelのリセット

  Serial.begin(19200); // シリアル通信の開始

  M5.begin();
  Wire.begin();

  wait_display_setup(); // 待機画面SCANのsetup
}

// ---------------------------------------------------------------
void loop()
{
  currentMillis = millis(); // 現在のミリ秒を取得

  if (!isCard)
  {
    pixels.clear(); // NeoPixelのリセット
    pixels.show();  // NeoPixelのリセット
    wait_display(); // 待機画面
    if (isNewCard())
    {
      CardID = identifyCard();
      // CardID = 2;
      isCard = true;
      startMillis = currentMillis;
    }
  }
  else
  {
    LEDcontrol(CardID, startMillis, currentMillis);
    LCDcontrol(2, startMillis, currentMillis);
    if ((currentMillis - startMillis) > 5000)
    {
      // zeroSet();
      // while (!reset_flg)
      // {
      //   shakeReset();
      // }
      isCard = false; // isCardをbooleanで更新
      reset_flg = false;
    }
  }
}

// ---------------------------------------------------------------
bool isNewCard()
{ // カード有無を判定
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
  { // カード読み取り関数
    return false;
  }
  else
  {
    // uid_display_proc();
    return true;
  }
}
// ---------------------------------------------------------------
int identifyCard()
{
  String strBuf[mfrc522.uid.size];
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    strBuf[i] = String(mfrc522.uid.uidByte[i], HEX);

    if (strBuf[i].length() == 1)
    {
      strBuf[i] = "0" + strBuf[i];
    }
  }
  String strUID = strBuf[0] + " " + strBuf[1] + " " + strBuf[2] + " " + strBuf[3];
  if (strUID.equalsIgnoreCase(ace))
  {
    // spriteImage.drawJpgFile(SD, "/trump/card_spade_01.jpg", 0, 0);
    //  spriteImage.pushSprite(&lcd, x, y);
    return 1;
  }
}
// ---------------------------------------------------------------
void LEDcontrol(int ID, unsigned long StartTime, unsigned long CurrentTime)
{
  if (StartTime == CurrentTime)
  {
    ledBrightness = 100;
    ledPosition = 0;
  }
  switch (ID)
  {
  case 1:
    if ((CurrentTime - previousLEDTime) > 100)
    { // 100ms間隔で更新
      Fingertip2Wrist(ledPosition, ledBrightness);
      previousLEDTime = CurrentTime;
      // M5.Lcd.println(previousLEDTime);

      ledBrightness += 10;
      ledPosition += 1;
      if (ledPosition >= 5)
      {
        ledPosition = 0;
      }
      if (ledBrightness >= 250)
      {
        ledBrightness = 250;
      }
      break;

    default:
      break;
    }
  }
}
// ---------------------------------------------------------------
void LCDcontrol(int ID, unsigned long StartTime, unsigned long CurrentTime)
{
  if (StartTime == CurrentTime)
  {
    // ledBrightness =100;
    // ledPosition =0;
  }
  switch (ID)
  {
  case 1:
    if ((CurrentTime - previousLCDTime) > 100)
    { // 100ms間隔で更新
      previousLCDTime = CurrentTime;
    }
    // M5.Lcd.println(previousLEDTime);

    /*
      ledBrightness +=10;
      ledPosition +=1;
    if(ledPosition >= 5){
      ledPosition = 0;
    }
    if(ledBrightness >= 250){
      ledBrightness = 250;
    }
    */
    break;

  case 2:
    if ((CurrentTime - previousLCDTime) > 100)
    { // 100ms間隔で更新
      float diff = currentMillis - startMillis;
      previousLCDTime = CurrentTime;
      rotate_display(diff);
    }
  default:
    break;
  }
}

// ---------------------------------------------------------------
void Fingertip2Wrist(int ledPosition_a, int ledBrightness_a)
{
  pixels.clear(); // NeoPixelのリセット
  if (ledPosition_a < 2)
  {
    pixels.setPixelColor(indexFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));  // i番目の色を設定
    pixels.setPixelColor(middleFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a)); // i番目の色を設定
    pixels.setPixelColor(ringFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));   // i番目の色を設定
  }
  else
  {
    pixels.setPixelColor(thumbLine[ledPosition_a - 2], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));        // i番目の色を設定
    pixels.setPixelColor(indexFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));      // i番目の色を設定
    pixels.setPixelColor(middleFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));     // i番目の色を設定
    pixels.setPixelColor(ringFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));       // i番目の色を設定
    pixels.setPixelColor(littleFingerLine[ledPosition_a - 2], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a)); // i番目の色を設定
  }
  pixels.setBrightness(60); // 0~255の範囲で明るさを設定
  pixels.show();            // LEDに色を反映

  // delay(100);
}
// ---------------------------------------------------------------
void uid_display_proc()
{
  short aa[] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  char buf[60];
  sprintf(buf, "mfrc522.uid.size = %d", mfrc522.uid.size);
  Serial.println(buf);
  for (short it = 0; it < mfrc522.uid.size; it++)
  {
    aa[it] = mfrc522.uid.uidByte[it];
  }

  sprintf(buf, "%02x %02x %02x %02x", aa[0], aa[1], aa[2], aa[3]);
  M5.Lcd.println(buf);
  M5.Lcd.println("");
  Serial.println(buf);
}

// ---------------------------------------------------------------
// wait_display_setup
void wait_display_setup()
{
  lcd.init();                   // LCD初期化
  spriteImage.setColorDepth(8); // カラーモード設定
  spriteBase.setColorDepth(8);  // カラーモード設定
  spriteWord1.setColorDepth(8); // カラーモード設定
  spriteWord2.setColorDepth(8); // カラーモード設定
  spriteWord3.setColorDepth(8); // カラーモード設定
  spriteWord4.setColorDepth(8); // カラーモード設定

  spriteImage.createSprite(320, 218);
  spriteBase.createSprite(320, 240);
  spriteWord1.createSprite(80, 80);
  spriteWord2.createSprite(80, 80);
  spriteWord3.createSprite(80, 80);
  spriteWord4.createSprite(80, 80);

  spriteWord1.setTextSize(6); // 文字サイズ42px
  spriteWord2.setTextSize(6); // 文字サイズ42px
  spriteWord3.setTextSize(6); // 文字サイズ42px
  spriteWord4.setTextSize(6); // 文字サイズ42px

  spriteWord1.setCursor(24, 24);
  spriteWord2.setCursor(24, 24);
  spriteWord3.setCursor(24, 24);
  spriteWord4.setCursor(24, 24);

  spriteWord1.printf("S");
  spriteWord2.printf("C");
  spriteWord3.printf("A");
  spriteWord4.printf("N");
}
// ---------------------------------------------------------------
// 待機画面の表示
void wait_display()
{
  spriteBase.fillScreen(BLACK); // 画面の塗りつぶし
  spriteWord1.pushRotateZoom(280, 120, 90, 1, 1);
  spriteWord2.pushRotateZoom(200, 120, 90, 1, 1);
  spriteWord3.pushRotateZoom(120, 120, 90, 1, 1);
  spriteWord4.pushRotateZoom(40, 120, 90, 1, 1);
  spriteBase.pushSprite(&lcd, 0, 0);
}
// ---------------------------------------------------------------
// 回転演出
int i = 0;
int mode = 1;
void rotate_display(float diff)
{
  // String strBuf[mfrc522.uid.size];
  // for (byte i = 0; i < mfrc522.uid.size; i++)
  // {
  //   strBuf[i] = String(mfrc522.uid.uidByte[i], HEX);
  //   if (strBuf[i].length() == 1)
  //   {
  //     strBuf[i] = "0" + strBuf[i];
  //   }
  // }

  // String strUID = strBuf[0] + " " + strBuf[1] + " " + strBuf[2] + " " + strBuf[3];

  // if (strUID.equalsIgnoreCase(ace))
  // {
  //   spriteImage.drawJpgFile(SD, "/trump/card_spade_01.jpg", 0, 0);
  // }
  // else if (strUID.equalsIgnoreCase(jack))
  // {
  //   spriteImage.drawJpgFile(SD, "/trump/card_spade_11.jpg", 0, 0);
  // }
  // else if (strUID.equalsIgnoreCase(queen))
  // {
  //   spriteImage.drawJpgFile(SD, "/trump/card_spade_12.jpg", 0, 0);
  // }
  // else if (strUID.equalsIgnoreCase(king))
  // {
  //   spriteImage.drawJpgFile(SD, "/trump/card_spade_13.jpg", 0, 0);
  // }
  // else
  // {
  //   Serial.println(strUID);
  // }

  // if (mode == 1)
  // {
  //   spriteImage.drawJpgFile(SD, "/trump/card_spade_01.jpg", 0, 0);
  // }

  // for (float i = 0; i <= 360; i += 10)
  // {
  //   spriteBase.fillScreen(BLACK); // 画面の塗りつぶし
  //   spriteImage.pushRotateZoom(160, 120, i, i / 360, i / 360);
  //   spriteBase.pushSprite(0, 0);
  //   delay(100);
  // }
  spriteImage.drawJpgFile(SD, "/trump/card_spade_01.jpg", 0, 0);
  spriteBase.fillScreen(BLACK); // 画面の塗りつぶし
  spriteImage.pushRotateZoom(160, 120, diff * 360 / 5000, diff * 1 / 5000, diff * 1 / 5000);
  // spriteImage.pushRotateZoom(160, 120, 0, 1, 1);
  spriteBase.pushSprite(0, 0);
  // i += 10;
  // if (i > 360)
  // {
  //   i = 0;
  // }
  // delay(100);
}
// ---------------------------------------------------------------
void shakeReset()
{
  M5.update();
  M5.IMU.getAccelData(&accX, &accY, &accZ); // 加速度データ取得

  diffAccX = accX - baseAccX; // 補正後の数値を表示値としてセット
  diffAccY = accY - baseAccY;
  diffAccZ = accZ - baseAccZ;

  if (diffAccY > 0.5)
  {
    reset_flg = true;
  }
}
// ---------------------------------------------------------------
// 加速度リセット用
void zeroSet()
{
  M5.IMU.getAccelData(&accX, &accY, &accZ); // 加速度データ取得
  baseAccX = accX;                          // 取得値を補正値としてセット
  baseAccY = accY;
  baseAccZ = accZ;
}
// ---------------------------------------------------------------
// 加速度取得用
void check_acc()
{
  zeroSet();
  M5.update();                              // ボタン状態更新
  M5.IMU.getAccelData(&accX, &accY, &accZ); // 加速度データ取得
}
// ---------------------------------------------------------------