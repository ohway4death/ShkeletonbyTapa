#include <M5Core2.h>
#include "MFRC522_I2C.h"
#include <Adafruit_NeoPixel.h>

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>

#include <driver/i2s.h>
#include <WiFi.h>

#include "aura.h"
#include "beamcharge.h"
#include "charge.h"
#include "drumroll.h"
#include "wafu.h"

/**RFID**/
MFRC522_I2C mfrc522(0x28, -1); // Create MFRC522 instance.

#define ace   "04:28:C2:9A"
#define jack  "04:28:BF:9A"
#define queen "04:28:C0:9A"
#define king  "04:28:C1:9A"
#define ten   "04:28:BE:9A"
#define joker "04:28:BD:9A"

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
int CardID = 0;
float TotalTime = 4000; // 演出合計時間(ms)
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
void rotate_display(float diff, int cardId);

// 加速度リセット
float baseAccX, baseAccY, baseAccZ = 0; // 基準加速度格納用
float accX, accY, accZ;                 // 加速度格納用
float diffAccX, diffAccY, diffAccZ = 0; // 現在値格納用
float maxAccX, maxAccY, maxAccZ = 0;
bool reset_flg = false;
void check_acc();
void zeroSet();
boolean shakeReset();

// 効果音
#define SPEAKER_I2S_NUMBER I2S_NUM_0
const unsigned char *wavList[] = {aura, beamcharge, charge, drumroll, wafu};
const size_t wavSize[] = {sizeof(aura), sizeof(beamcharge), sizeof(charge), sizeof(drumroll), sizeof(wafu)};

// マルチタスク
TaskHandle_t task1Handle;
QueueHandle_t xQueue1;
#define QUEUE1_LENGTH 5

// プロトタイプ宣言
void Fingertip2Wrist(int ledPosition, int ledBrightness);
bool isNewCard();
int identifyCard();
void LEDcontrol(int B, unsigned long C, unsigned long D);
void LCDcontrol(int B, unsigned long C, unsigned long D);
void uid_display_proc();
void acc_setup();
void sound_effect_setup();
void SEcontrol();
void InitI2SSpeakerOrMic(int mode);
void multi_task_setup();
// ---------------------------------------------------------------
void setup()
{
  pixels.begin(); // NeoPixelの初期化
  pixels.clear(); // NeoPixelのリセット

  Serial.begin(115200); // シリアル通信の開始

  M5.begin();
  Wire.begin();
  mfrc522.PCD_Init();

  wait_display_setup(); // 待機画面SCANのsetup
  acc_setup();          // shake_resetのためのsetup
  sound_effect_setup(); // 効果音のためのsetup

  xQueue1 = xQueueCreate(QUEUE1_LENGTH, sizeof(boolean));
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
      static bool data = true;
      xQueueSend(xQueue1, &data, 0);
      startMillis = currentMillis;
    }
  }
  else
  {
    LEDcontrol(CardID, startMillis, currentMillis);
    LCDcontrol(CardID, startMillis, currentMillis);
    // SEcontrol();
    multi_task_setup();
    Serial.printf("startMillis = %d, currentMillis = %d, diff = %d\n", startMillis, currentMillis, (currentMillis - startMillis));

    if ((currentMillis - startMillis) > TotalTime)
    {
      zeroSet();
      while (!shakeReset())
        isCard = false; // isCardをbooleanで更新
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
  Serial.printf("%s",strUID);

  // カードを増やしたい場合はdefine増やしてからここに追加
  // switch文でstringの比較ができない
  if (strUID.equalsIgnoreCase(ace))
  {
    return 1;
  }else if (strUID.equalsIgnoreCase(ten))
  {
    return 10;
  }else if (strUID.equalsIgnoreCase(jack))
  {
    return 11;
  }else if (strUID.equalsIgnoreCase(queen))
  {
    return 12;
  }else if (strUID.equalsIgnoreCase(king))
  {
    return 13;
  }else if (strUID.equalsIgnoreCase(joker))
  {
    return 0;
  }else{
    return 1; // test用
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

  int action = 2; // 演出を指定

  switch (action)
  {
  case 1:
    if ((CurrentTime - previousLCDTime) > 100)
    { // 100ms間隔で更新
      previousLCDTime = CurrentTime;
    }
    // M5.Lcd.println(previousLEDTime);
    break;

  case 2:
    if ((CurrentTime - previousLCDTime) > 100)
    { // 100ms間隔で更新
      float diff = currentMillis - startMillis;
      previousLCDTime = CurrentTime;
      rotate_display(diff, ID);
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
void rotate_display(float diff, int cardId)
{
  switch (cardId)
  {
  case 1:
    spriteImage.drawJpgFile(SD, "/trump/card_spade_01.jpg", 0, 0);
    break;
  case 10:
    spriteImage.drawJpgFile(SD, "/trump/card_spade_10.jpg", 0, 0);
    break;
  case 11:
    spriteImage.drawJpgFile(SD, "/trump/card_spade_11.jpg", 0, 0);
    break;
  case 12:
    spriteImage.drawJpgFile(SD, "/trump/card_spade_12.jpg", 0, 0);
    break;
  case 13:
    spriteImage.drawJpgFile(SD, "/trump/card_spade_13.jpg", 0, 0);
    break;
  case  0:
    spriteImage.drawJpgFile(SD, "/trump/card_spade_05.jpg", 0, 0);
    break;
  }

  float round_diff = round(diff / 100) * 100; // きっちり5000で回転が終わるように調整
  // Serial.printf("--------------------------- \n");
  // Serial.printf("%lf \n", round_diff);

  spriteBase.fillScreen(BLACK); // 画面の塗りつぶし
  spriteImage.pushRotateZoom(160, 120, round_diff * 360 / TotalTime, round_diff * 1 / TotalTime, round_diff * 1 / TotalTime);
  spriteBase.pushSprite(0, 0);
}
// ---------------------------------------------------------------
// 加速度のセットアップ
void acc_setup()
{
  M5.IMU.Init();                     // 6軸センサ初期化
  M5.IMU.SetAccelFsr(M5.IMU.AFS_2G); // 加速度センサースケール初期値 ±2G(2,4,8,16)
}
// ---------------------------------------------------------------
// 加速度リセット用
void zeroSet()
{
  M5.update();
  M5.IMU.getAccelData(&accX, &accY, &accZ); // 加速度データ取得
  baseAccX = accX;                          // 取得値を補正値としてセット
  baseAccY = accY;
  baseAccZ = accZ;
}
// ---------------------------------------------------------------
// 端末を振ってリセット画面へ
boolean shakeReset()
{
  M5.update();
  M5.IMU.getAccelData(&accX, &accY, &accZ); // 加速度データ取得

  diffAccX = accX - baseAccX; // 補正後の数値を表示値としてセット
  diffAccY = accY - baseAccY;
  diffAccZ = accZ - baseAccZ;

  if (diffAccY > 0.5)
  {
    return true;
  }
  else
  {
    return false;
  }
}
// ---------------------------------------------------------------
// 効果音のセットアップ
void sound_effect_setup()
{
  M5.Axp.SetSpkEnable(true);
  InitI2SSpeakerOrMic(MODE_SPK);
}
// ---------------------------------------------------------------
// 効果音コントロール
void SEcontrol()
{
  int rand_int = random(5);
  size_t bytes_written;

  // rand_int = 0; // test用

  i2s_write(SPEAKER_I2S_NUMBER, wavList[rand_int], wavSize[rand_int], &bytes_written, portMAX_DELAY);
  i2s_zero_dma_buffer(SPEAKER_I2S_NUMBER);
}
// ---------------------------------------------------------------
void InitI2SSpeakerOrMic(int mode)
{
  esp_err_t err = ESP_OK;
  i2s_driver_uninstall(SPEAKER_I2S_NUMBER);
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 6,
      .dma_buf_len = 60,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0};
  if (mode == MODE_MIC)
  {
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  }
  else
  {
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  }
  err += i2s_driver_install(SPEAKER_I2S_NUMBER, &i2s_config, 0, NULL);
  i2s_pin_config_t tx_pin_config = {
      .bck_io_num = CONFIG_I2S_BCK_PIN,
      .ws_io_num = CONFIG_I2S_LRCK_PIN,
      .data_out_num = CONFIG_I2S_DATA_PIN,
      .data_in_num = CONFIG_I2S_DATA_IN_PIN,
  };
  err += i2s_set_pin(SPEAKER_I2S_NUMBER, &tx_pin_config);
  if (mode != MODE_MIC)
  {
    err += i2s_set_clk(SPEAKER_I2S_NUMBER, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  }
  i2s_zero_dma_buffer(SPEAKER_I2S_NUMBER);
}
// ---------------------------------------------------------------
void task1(void *pvParameters)
{
  bool receivedData;
  if (xQueueReceive(xQueue1, &receivedData, 0) == pdTRUE)
  {
    if (receivedData == 1)
    {
      SEcontrol();
    }
  }

  task1Handle = NULL;
  vTaskDelete(NULL);

  // while (1)
  // {
  //   SEcontrol();
  //   // Wait(1ミリ秒以上を推奨)
  //   delay(1);
  // }
}
// ---------------------------------------------------------------
void multi_task_setup()
{
  xTaskCreateUniversal(
      task1,        // 作成するタスク関数
      "SE_task",    // 表示用タスク名
      8192,         // スタックメモリ量
      NULL,         // 起動パラメータ
      0,            // 優先度
      &task1Handle, // タスクハンドル
      0             // 実行するコア
  );
}
// ---------------------------------------------------------------