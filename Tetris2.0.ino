/*
 * Arduino Nano Tetris Oyunu - SH1106 OLED Ekran - İYİLEŞTİRİLMİŞ VERSİYON
 * U8g2 Kütüphanesi ile
 * 
 * Ekran: 1.3" SH1106 I2C OLED (128x64)
 * Pin Sırası: GND-VCC-SCL-SDA
 * 
 * Bağlantılar:
 * - OLED GND -> Arduino GND
 * - OLED VCC -> Arduino 5V (veya 3.3V)
 * - OLED SCL -> Arduino A5
 * - OLED SDA -> Arduino A4
 * - Yukarı Buton -> D2
 * - Aşağı Buton -> D6
 * - Sol Buton -> D5
 * - Sağ Buton -> D3
 * - OK Buton -> D4
 * - Piezo Buzzer (+) -> D10
 * - Tüm butonlar ve buzzer (-) -> GND
 * 
 * YENİ ÖZELLİKLER:
 * - Geliştirilmiş zorlaşma sistemi (daha hızlı zorlaşır)
 * - Bag Randomizer (şekiller tekrar etmez)
 * - Tetris tema müziği (açılışta)
 * - Game Over melodisi
 */

#include <U8g2lib.h>
#include <Wire.h>
#include <EEPROM.h>

// U8g2 Constructor - SH1106 128x64 I2C (Page Buffer Mode - DİKEY EKRAN)
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R1, U8X8_PIN_NONE);

// Buton Pinleri
#define BTN_UP 2
#define BTN_DOWN 6
#define BTN_LEFT 5
#define BTN_RIGHT 3
#define BTN_OK 4

// Piezo Buzzer - PWM pin
#define BUZZER_PIN 10

// EEPROM Adresleri
#define EEPROM_HIGHSCORE_ADDR 0
#define EEPROM_MAGIC_ADDR 4
#define EEPROM_MAGIC_VALUE 0xAB
#define EEPROM_SOUND_ENABLED_ADDR 8
#define EEPROM_SOUND_VOLUME_ADDR 9

// ============================================
// OYUN AYARLARI - GELİŞTİRİLMİŞ ZORLAŞMA
// ============================================

// AÇILIŞ EKRANI
#define INTRO_DURATION 4000
#define INTRO_FRAME_DELAY 100

// GERİ SAYIM
#define COUNTDOWN_START 3
#define COUNTDOWN_INTERVAL 1000

// KONTROL AYARLARI
#define MOVE_REPEAT_DELAY 70
#define ROTATE_REPEAT_DELAY 150
#define PAUSE_HOLD_TIME 500

// OYUN HIZI - GELİŞTİRİLMİŞ ZORLAŞMA SİSTEMİ
#define FALL_SPEED_LEVEL_1 600      // Seviye 1 - Biraz daha yavaş başla
#define FALL_SPEED_LEVEL_2 500      // Seviye 2
#define FALL_SPEED_LEVEL_3 400      // Seviye 3
#define FALL_SPEED_LEVEL_4 300      // Seviye 4
#define FALL_SPEED_LEVEL_5 220      // Seviye 5
#define FALL_SPEED_LEVEL_6 160      // Seviye 6
#define FALL_SPEED_LEVEL_7 120      // Seviye 7
#define FALL_SPEED_LEVEL_8 90       // Seviye 8
#define FALL_SPEED_LEVEL_9 70       // Seviye 9
#define FALL_SPEED_LEVEL_10 50      // Seviye 10+ - ÇOK HIZLI!

#define FAST_DROP_DIVIDER 10

// SEVİYE SİSTEMİ
#define LINES_PER_LEVEL 10

// SKOR SİSTEMİ
#define POINTS_PER_PIECE 1
#define POINTS_PER_LINE 10

// EKRAN AYARLARI
#define GAME_WIDTH 10
#define GAME_HEIGHT 20
#define BLOCK_SIZE 6
#define GAME_OFFSET_X 2
#define GAME_OFFSET_Y 5

// SES EFEKTLERİ
#define BEEP_DURATION 30
#define MOVE_BEEP_FREQ 600
#define ROTATE_BEEP_FREQ 800
#define DROP_BEEP_FREQ 400

// AYARLAR MENÜSÜ
#define SETTINGS_MENU_ITEMS 3
#define MIN_VOLUME 1
#define MAX_VOLUME 5

// ============================================
// BAG RANDOMIZER SİSTEMİ (Şekil Tekrarını Önler)
// ============================================
int pieceBag[7];           // 7 şekilli çanta
int bagIndex = 7;          // Çanta boşsa yeniden doldur

// Oyun durumları
enum GameState {
  STATE_MENU,
  STATE_SETTINGS,
  STATE_COUNTDOWN,
  STATE_PLAYING,
  STATE_PAUSED,
  STATE_GAMEOVER
};

GameState gameState = STATE_MENU;

// Oyun alanı
bool gameField[GAME_HEIGHT][GAME_WIDTH] = {0};

// Tetromino şekilleri
const byte TETROMINOS[7][4][4] = {
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, // I
  {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}}, // O
  {{0,0,0,0},{0,1,1,1},{0,0,1,0},{0,0,0,0}}, // T
  {{0,0,0,0},{0,0,1,1},{0,1,1,0},{0,0,0,0}}, // S
  {{0,0,0,0},{0,1,1,0},{0,0,1,1},{0,0,0,0}}, // Z
  {{0,0,0,0},{0,1,1,1},{0,0,0,1},{0,0,0,0}}, // J
  {{0,0,0,0},{0,1,1,1},{0,1,0,0},{0,0,0,0}}  // L
};

// Oyun değişkenleri
int currentPiece = 0;
int nextPiece = 0;
int currentRotation = 0;
int pieceX = 3;
int pieceY = 0;
unsigned long lastFallTime = 0;
unsigned long fallDelay = FALL_SPEED_LEVEL_1;
int score = 0;
int highScore = 0;
int prevHighScore = 0;
int linesCleared = 0;
int level = 1;
unsigned long lastLeftMove = 0;
unsigned long lastRightMove = 0;
unsigned long buttonPressStartTime = 0;
int countdownTimer = COUNTDOWN_START;
bool fastDropActive = false;
bool okButtonPressed = false;

// Ses ayarları
bool soundEnabled = true;
int soundVolume = 3;

// Ayarlar menüsü
int settingsMenuIndex = 0;
bool editingVolume = false;

// Forward declarations
void loadHighScore();
void loadSoundSettings();
void saveSoundSettings();
void showIntro();
void playTetrisTheme();
void showMenu();
void showSettings();
void handleSettings();
void shuffleBag();
int getNextPieceFromBag();

void setup() {
  Serial.begin(9600);
  
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  
  u8g2.begin();
  u8g2.setContrast(255);
  
  randomSeed(analogRead(0));
  
  // Bag randomizer'ı başlat
  shuffleBag();
  
  loadHighScore();
  loadSoundSettings();
  prevHighScore = highScore;
  
  showIntro();
  playTetrisTheme();  // 🎵 TETRİS TEMASI
  
  showMenu();
}

void loop() {
  switch(gameState) {
    case STATE_MENU:
      handleMenu();
      break;
    case STATE_SETTINGS:
      handleSettings();
      break;
    case STATE_COUNTDOWN:
      handleCountdown();
      break;
    case STATE_PLAYING:
      handleGame();
      break;
    case STATE_PAUSED:
      handlePause();
      break;
    case STATE_GAMEOVER:
      handleGameOver();
      break;
  }
}

// ============================================
// BAG RANDOMIZER - ŞEKİL TEKRARI YOK!
// ============================================
void shuffleBag() {
  // 7 şekli çantaya koy
  for(int i = 0; i < 7; i++) {
    pieceBag[i] = i;
  }
  
  // Fisher-Yates shuffle - karıştır
  for(int i = 6; i > 0; i--) {
    int j = random(i + 1);
    int temp = pieceBag[i];
    pieceBag[i] = pieceBag[j];
    pieceBag[j] = temp;
  }
  
  bagIndex = 0;
}

int getNextPieceFromBag() {
  if(bagIndex >= 7) {
    shuffleBag();  // Çanta bitti, yeniden karıştır
  }
  return pieceBag[bagIndex++];
}

// ============================================
// TETRİS TEMA MÜZİĞİ (Korobeiniki)
// ============================================
void playTetrisTheme() {
  if(!soundEnabled) return;
  
  int pwmValue = map(soundVolume, MIN_VOLUME, MAX_VOLUME, 50, 255);
  analogWrite(BUZZER_PIN, pwmValue);
  
  // Tetris Theme A (ilk 8 nota)
  // E5 B4 C5 D5 C5 B4 A4 A4
  int melody[] = {659, 494, 523, 587, 523, 494, 440, 440};
  int durations[] = {400, 200, 200, 400, 200, 200, 400, 200};
  
  for(int i = 0; i < 8; i++) {
    tone(BUZZER_PIN, melody[i], durations[i]);
    delay(durations[i] + 50);
    noTone(BUZZER_PIN);
  }
  
  analogWrite(BUZZER_PIN, 0);
}

// ============================================
// AÇILIŞ ANIMASYONU
// ============================================
void showIntro() {
  int totalFrames = INTRO_DURATION / INTRO_FRAME_DELAY;
  
  for(int frame = 0; frame < totalFrames; frame++) {
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(12, 12, "TETRIS");
      
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.drawStr(10, 24, "Taha Acar");
      
      u8g2.drawLine(5, 28, 59, 28);
      
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.drawStr(12, 40, "Loading");
      
      int dots = (frame / 8) % 4;
      for(int i = 0; i < dots; i++) {
        u8g2.drawStr(46 + (i * 3), 40, ".");
      }
      
      int blockY = 55;
      int blockSize = 4;
      int rotation = (frame / 5) % 4;
      
      drawMiniTetromino(5, rotation, 10, blockY, blockSize);
      drawMiniTetromino(2, rotation, 26, blockY, blockSize);
      drawMiniTetromino(0, rotation, 42, blockY, blockSize);
      
      int barY = 95;
      u8g2.drawFrame(10, barY, 44, 6);
      int progress = (frame * 42) / totalFrames;
      u8g2.drawBox(11, barY + 1, progress, 4);
      
    } while (u8g2.nextPage());
    
    delay(INTRO_FRAME_DELAY);
  }
}

void drawMiniTetromino(int piece, int rotation, int x, int y, int blockSize) {
  for(int py = 0; py < 4; py++) {
    for(int px = 0; px < 4; px++) {
      if(getPieceBlock(piece, rotation, px, py)) {
        u8g2.drawBox(
          x + px * blockSize,
          y + py * blockSize,
          blockSize - 1,
          blockSize - 1
        );
      }
    }
  }
}

// ============================================
// MENÜ EKRANI
// ============================================
void showMenu() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(2, 15, "TETRIS");
    
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 28, "Taha Acar");
    
    u8g2.drawLine(0, 32, 64, 32);
    
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(2, 48, "Rekor:");
    
    char scoreStr[10];
    itoa(highScore, scoreStr, 10);
    u8g2.drawStr(2, 60, scoreStr);
    
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 80, "Oyun icin");
    u8g2.drawStr(2, 90, "OK basin");
    
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 110, "Ayar icin");
    u8g2.drawStr(2, 120, "SAG basin");
    
  } while (u8g2.nextPage());
}

void handleMenu() {
  if(digitalRead(BTN_UP) == LOW && digitalRead(BTN_OK) == LOW) {
    delay(300);
    highScore = 0;
    saveHighScore();
    playBeep(400, 100);
    playBeep(600, 100);
    playBeep(800, 100);
    showMenu();
    delay(500);
    return;
  }
  
  if(digitalRead(BTN_OK) == LOW) {
    delay(200);
    playBeep(1000, 100);
    gameState = STATE_COUNTDOWN;
    countdownTimer = COUNTDOWN_START;
    lastFallTime = millis();
    return;
  }
  
  if(digitalRead(BTN_RIGHT) == LOW) {
    delay(200);
    playBeep(800, 80);
    slideToSettings();
    gameState = STATE_SETTINGS;
    settingsMenuIndex = 0;
    editingVolume = false;
  }
}

// ============================================
// AYARLAR MENÜSÜ
// ============================================
void slideToSettings() {
  for(int offset = 0; offset <= 64; offset += 8) {
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.drawStr(2 - offset, 15, "TETRIS");
      
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(64 - offset + 10, 15, "AYARLAR");
    } while (u8g2.nextPage());
    delay(30);
  }
}

void slideToMenu() {
  for(int offset = 0; offset <= 64; offset += 8) {
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(10 - offset, 15, "AYARLAR");
      
      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.drawStr(64 - offset + 2, 15, "TETRIS");
    } while (u8g2.nextPage());
    delay(30);
  }
}

void showSettings() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(8, 12, "AYARLAR");
    u8g2.drawLine(0, 15, 64, 15);
    
    u8g2.setFont(u8g2_font_6x10_tr);
    
    if(settingsMenuIndex == 0) {
      u8g2.drawStr(2, 30, ">");
    }
    u8g2.drawStr(10, 30, "Ses:");
    u8g2.drawStr(35, 30, soundEnabled ? "(ON)" : "(OFF)");
    
    if(settingsMenuIndex == 1) {
      u8g2.drawStr(2, 50, ">");
    }
    u8g2.drawStr(10, 50, "Seviye:");
    
    char volStr[5];
    sprintf(volStr, "(%d)", soundVolume);
    u8g2.drawStr(42, 50, volStr);
    
    if(editingVolume && settingsMenuIndex == 1) {
      u8g2.drawStr(36, 50, "<");
      u8g2.drawStr(52, 50, ">");
    }
    
    if(settingsMenuIndex == 2) {
      u8g2.drawStr(2, 70, ">");
    }
    u8g2.drawStr(10, 70, "Cikis");
    
    u8g2.drawLine(0, 78, 64, 78);
    
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 92, "OK: Sec/Kaydet");
    
    if(soundEnabled) {
      int barY = 105;
      u8g2.drawFrame(5, barY, 54, 8);
      int barWidth = (soundVolume * 50) / MAX_VOLUME;
      u8g2.drawBox(7, barY + 2, barWidth, 4);
    }
    
  } while (u8g2.nextPage());
}

void handleSettings() {
  showSettings();
  
  static unsigned long lastButtonPress = 0;
  unsigned long currentTime = millis();
  
  if(currentTime - lastButtonPress < 200) {
    return;
  }
  
  if(!editingVolume) {
    if(digitalRead(BTN_UP) == LOW) {
      playBeep(600, 50);
      settingsMenuIndex--;
      if(settingsMenuIndex < 0) settingsMenuIndex = SETTINGS_MENU_ITEMS - 1;
      lastButtonPress = currentTime;
      return;
    }
    
    if(digitalRead(BTN_DOWN) == LOW) {
      playBeep(600, 50);
      settingsMenuIndex++;
      if(settingsMenuIndex >= SETTINGS_MENU_ITEMS) settingsMenuIndex = 0;
      lastButtonPress = currentTime;
      return;
    }
    
    if(digitalRead(BTN_OK) == LOW) {
      playBeep(1000, 80);
      
      if(settingsMenuIndex == 0) {
        soundEnabled = !soundEnabled;
        saveSoundSettings();
      } else if(settingsMenuIndex == 1) {
        editingVolume = true;
      } else if(settingsMenuIndex == 2) {
        saveSoundSettings();
        slideToMenu();
        gameState = STATE_MENU;
        showMenu();
      }
      
      lastButtonPress = currentTime;
      return;
    }
  } 
  else {
    if(digitalRead(BTN_UP) == LOW || digitalRead(BTN_RIGHT) == LOW) {
      if(soundVolume < MAX_VOLUME) {
        soundVolume++;
        playBeep(800 + (soundVolume * 100), 100);
      }
      lastButtonPress = currentTime;
      return;
    }
    
    if(digitalRead(BTN_DOWN) == LOW || digitalRead(BTN_LEFT) == LOW) {
      if(soundVolume > MIN_VOLUME) {
        soundVolume--;
        playBeep(800 + (soundVolume * 100), 100);
      }
      lastButtonPress = currentTime;
      return;
    }
    
    if(digitalRead(BTN_OK) == LOW) {
      playBeep(1200, 100);
      editingVolume = false;
      saveSoundSettings();
      lastButtonPress = currentTime;
      return;
    }
  }
}

void handleCountdown() {
  unsigned long currentTime = millis();
  
  if(currentTime - lastFallTime >= COUNTDOWN_INTERVAL) {
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_ncenB24_tr);
      
      if(countdownTimer > 0) {
        char numStr[2];
        itoa(countdownTimer, numStr, 10);
        u8g2.drawStr(20, 70, numStr);
      }
    } while (u8g2.nextPage());
    
    if(countdownTimer > 0) {
      playBeep(800, 150);
      countdownTimer--;
      lastFallTime = currentTime;
    } else {
      playBeep(1200, 200);
      gameState = STATE_PLAYING;
      resetGame();
    }
  }
}

void resetGame() {
  for(int y = 0; y < GAME_HEIGHT; y++) {
    for(int x = 0; x < GAME_WIDTH; x++) {
      gameField[y][x] = false;
    }
  }
  score = 0;
  linesCleared = 0;
  level = 1;
  fallDelay = FALL_SPEED_LEVEL_1;
  fastDropActive = false;
  okButtonPressed = false;
  
  // Bag randomizer reset
  shuffleBag();
  nextPiece = getNextPieceFromBag();
  
  spawnNewPiece();
}

void handleGame() {
  unsigned long currentTime = millis();
  
  if(digitalRead(BTN_OK) == LOW) {
    if(!okButtonPressed) {
      buttonPressStartTime = currentTime;
      okButtonPressed = true;
    } else if(currentTime - buttonPressStartTime > PAUSE_HOLD_TIME) {
      playBeep(1000, 100);
      gameState = STATE_PAUSED;
      okButtonPressed = false;
      return;
    }
  } else {
    okButtonPressed = false;
  }
  
  if(digitalRead(BTN_LEFT) == LOW && (currentTime - lastLeftMove > MOVE_REPEAT_DELAY)) {
    movePiece(-1, 0);
    playBeep(MOVE_BEEP_FREQ, BEEP_DURATION);
    lastLeftMove = currentTime;
  }
  
  if(digitalRead(BTN_RIGHT) == LOW && (currentTime - lastRightMove > MOVE_REPEAT_DELAY)) {
    movePiece(1, 0);
    playBeep(MOVE_BEEP_FREQ, BEEP_DURATION);
    lastRightMove = currentTime;
  }
  
  static unsigned long lastRotateTime = 0;
  if(currentTime - lastRotateTime > ROTATE_REPEAT_DELAY) {
    if(digitalRead(BTN_UP) == LOW) {
      rotatePiece();
      playBeep(ROTATE_BEEP_FREQ, 40);
      lastRotateTime = currentTime;
    }
  }
  
  if(digitalRead(BTN_DOWN) == LOW) {
    fastDropActive = true;
  } else {
    fastDropActive = false;
  }
  
  unsigned long currentFallDelay = fastDropActive ? (fallDelay / FAST_DROP_DIVIDER) : fallDelay;
  
  if(currentTime - lastFallTime >= currentFallDelay) {
    if(!movePiece(0, 1)) {
      playDropSound();
      lockPiece();
      score += POINTS_PER_PIECE;
      clearLines();
      spawnNewPiece();
      
      if(checkCollision(pieceX, pieceY, currentRotation)) {
        playGameOverMelody();
        
        if(score > prevHighScore) {
          highScore = score;
          saveHighScore();
          prevHighScore = highScore;
        }
        
        gameState = STATE_GAMEOVER;
      }
    }
    lastFallTime = currentTime;
  }
  
  drawGame();
}

void spawnNewPiece() {
  currentPiece = nextPiece;
  nextPiece = getNextPieceFromBag();  // 🎲 BAG RANDOMIZER
  currentRotation = 0;
  pieceX = GAME_WIDTH / 2 - 2;
  pieceY = 0;
}

bool movePiece(int dx, int dy) {
  int newX = pieceX + dx;
  int newY = pieceY + dy;
  
  if(!checkCollision(newX, newY, currentRotation)) {
    pieceX = newX;
    pieceY = newY;
    return true;
  }
  return false;
}

void rotatePiece() {
  int newRotation = (currentRotation + 1) % 4;
  if(!checkCollision(pieceX, pieceY, newRotation)) {
    currentRotation = newRotation;
  }
}

bool checkCollision(int x, int y, int rotation) {
  for(int py = 0; py < 4; py++) {
    for(int px = 0; px < 4; px++) {
      if(getPieceBlock(currentPiece, rotation, px, py)) {
        int fieldX = x + px;
        int fieldY = y + py;
        
        if(fieldX < 0 || fieldX >= GAME_WIDTH || fieldY >= GAME_HEIGHT) {
          return true;
        }
        
        if(fieldY >= 0 && gameField[fieldY][fieldX]) {
          return true;
        }
      }
    }
  }
  return false;
}

byte getPieceBlock(int piece, int rotation, int x, int y) {
  int px = x, py = y;
  for(int r = 0; r < rotation; r++) {
    int temp = px;
    px = 3 - py;
    py = temp;
  }
  return TETROMINOS[piece][py][px];
}

void lockPiece() {
  for(int py = 0; py < 4; py++) {
    for(int px = 0; px < 4; px++) {
      if(getPieceBlock(currentPiece, currentRotation, px, py)) {
        int fieldX = pieceX + px;
        int fieldY = pieceY + py;
        if(fieldY >= 0 && fieldY < GAME_HEIGHT && fieldX >= 0 && fieldX < GAME_WIDTH) {
          gameField[fieldY][fieldX] = true;
        }
      }
    }
  }
}

void clearLines() {
  int cleared = 0;
  
  for(int y = GAME_HEIGHT - 1; y >= 0; y--) {
    bool full = true;
    for(int x = 0; x < GAME_WIDTH; x++) {
      if(!gameField[y][x]) {
        full = false;
        break;
      }
    }
    
    if(full) {
      cleared++;
      
      for(int yy = y; yy > 0; yy--) {
        for(int x = 0; x < GAME_WIDTH; x++) {
          gameField[yy][x] = gameField[yy-1][x];
        }
      }
      
      for(int x = 0; x < GAME_WIDTH; x++) {
        gameField[0][x] = false;
      }
      
      y++;
    }
  }
  
  if(cleared > 0) {
    playClearLineSound(cleared);
    linesCleared += cleared;
    score += cleared * POINTS_PER_LINE;
    
    level = (linesCleared / LINES_PER_LEVEL) + 1;
    
    // 🚀 GELİŞTİRİLMİŞ ZORLAŞMA SİSTEMİ
    updateDifficulty();
  }
}

// ============================================
// ZORLAŞMA SİSTEMİ - GİTTİKÇE HIZLANIYOR!
// ============================================
void updateDifficulty() {
  switch(level) {
    case 1:
      fallDelay = FALL_SPEED_LEVEL_1;  // 600ms
      break;
    case 2:
      fallDelay = FALL_SPEED_LEVEL_2;  // 500ms
      break;
    case 3:
      fallDelay = FALL_SPEED_LEVEL_3;  // 400ms
      break;
    case 4:
      fallDelay = FALL_SPEED_LEVEL_4;  // 300ms
      break;
    case 5:
      fallDelay = FALL_SPEED_LEVEL_5;  // 220ms
      break;
    case 6:
      fallDelay = FALL_SPEED_LEVEL_6;  // 160ms
      break;
    case 7:
      fallDelay = FALL_SPEED_LEVEL_7;  // 120ms
      break;
    case 8:
      fallDelay = FALL_SPEED_LEVEL_8;  // 90ms
      break;
    case 9:
      fallDelay = FALL_SPEED_LEVEL_9;  // 70ms
      break;
    default:
      fallDelay = FALL_SPEED_LEVEL_10; // 50ms - ÇOK HIZLI!
      break;
  }
}

void drawGame() {
  int roofOffset = 6;
  
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_4x6_tr);
    char scoreStr[20];
    sprintf(scoreStr, "S:%d Lv:%d", score, level);
    u8g2.drawStr(2, 6, scoreStr);
    
    int nextX = 42;
    int nextY = 1;
    int miniBlockSize = 2;
    
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(54, 6, "Nxt");
    
    for(int py = 0; py < 4; py++) {
      for(int px = 0; px < 4; px++) {
        if(getPieceBlock(nextPiece, 0, px, py)) {
          u8g2.drawBox(
            nextX + px * miniBlockSize,
            nextY + py * miniBlockSize,
            miniBlockSize - 1,
            miniBlockSize - 1
          );
        }
      }
    }
    
    for(int y = 1; y < GAME_HEIGHT; y++) {
      for(int x = 0; x < GAME_WIDTH; x++) {
        if(gameField[y][x]) {
          u8g2.drawBox(
            GAME_OFFSET_X + x * BLOCK_SIZE,
            GAME_OFFSET_Y + y * BLOCK_SIZE,
            BLOCK_SIZE - 1,
            BLOCK_SIZE - 1
          );
        }
      }
    }
    
    for(int py = 0; py < 4; py++) {
      for(int px = 0; px < 4; px++) {
        if(getPieceBlock(currentPiece, currentRotation, px, py)) {
          int drawX = pieceX + px;
          int drawY = pieceY + py;
          if(drawY >= 1) {
            u8g2.drawBox(
              GAME_OFFSET_X + drawX * BLOCK_SIZE,
              GAME_OFFSET_Y + drawY * BLOCK_SIZE,
              BLOCK_SIZE - 1,
              BLOCK_SIZE - 1
            );
          }
        }
      }
    }
    
    u8g2.drawFrame(
      GAME_OFFSET_X - 1, 
      GAME_OFFSET_Y + roofOffset - 1,
      GAME_WIDTH * BLOCK_SIZE + 1, 
      (GAME_HEIGHT - 1) * BLOCK_SIZE + 1
    );
    
  } while (u8g2.nextPage());
}

void handleGameOver() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(5, 20, "OYUN");
    u8g2.drawStr(5, 35, "BITTI!");
    
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(5, 60, "Skor:");
    char scoreStr[10];
    itoa(score, scoreStr, 10);
    u8g2.drawStr(5, 72, scoreStr);
    
    u8g2.drawStr(5, 90, "Rekor:");
    char highStr[10];
    itoa(highScore, highStr, 10);
    u8g2.drawStr(5, 102, highStr);
    
    u8g2.drawStr(5, 120, "OK: Tekrar");
  } while (u8g2.nextPage());
  
  if(digitalRead(BTN_OK) == LOW) {
    delay(200);
    playBeep(1000, 100);
    if(score > prevHighScore) {
      highScore = score;
      saveHighScore();
      prevHighScore = highScore;
    }
    gameState = STATE_MENU;
    showMenu();
  }
}

void handlePause() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 40, "OYUN");
    u8g2.drawStr(5, 55, "DURDU");
    
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(5, 80, "OK: Devam");
    
    if(millis() / 500 % 2 == 0) {
      u8g2.drawDisc(30, 100, 3);
    }
  } while (u8g2.nextPage());
  
  static bool pauseButtonReleased = true;
  
  if(digitalRead(BTN_OK) == HIGH) {
    pauseButtonReleased = true;
  }
  
  if(digitalRead(BTN_OK) == LOW && pauseButtonReleased) {
    pauseButtonReleased = false;
    delay(200);
    playBeep(1000, 100);
    gameState = STATE_PLAYING;
    lastFallTime = millis();
  }
}

// ============================================
// SES EFEKTLERİ
// ============================================
void playBeep(int frequency, int duration) {
  if(!soundEnabled) return;
  
  int pwmValue = map(soundVolume, MIN_VOLUME, MAX_VOLUME, 50, 255);
  int adjustedFreq = frequency + (level * 15);
  
  analogWrite(BUZZER_PIN, pwmValue);
  tone(BUZZER_PIN, adjustedFreq, duration);
  delay(duration);
  noTone(BUZZER_PIN);
  analogWrite(BUZZER_PIN, 0);
}

void playDropSound() {
  if(!soundEnabled) return;
  
  int baseTone = DROP_BEEP_FREQ + (level * 20);
  int pwmValue = map(soundVolume, MIN_VOLUME, MAX_VOLUME, 50, 255);
  
  analogWrite(BUZZER_PIN, pwmValue);
  tone(BUZZER_PIN, baseTone);
  delay(50);
  tone(BUZZER_PIN, baseTone - 100);
  delay(50);
  noTone(BUZZER_PIN);
  analogWrite(BUZZER_PIN, 0);
}

void playClearLineSound(int lines) {
  if(!soundEnabled) return;
  
  int freq = 1000 + (lines * 200);
  int pwmValue = map(soundVolume, MIN_VOLUME, MAX_VOLUME, 50, 255);
  
  analogWrite(BUZZER_PIN, pwmValue);
  tone(BUZZER_PIN, freq, 150);
  delay(150);
  noTone(BUZZER_PIN);
  analogWrite(BUZZER_PIN, 0);
}

// ============================================
// GAME OVER MÜZİĞİ - Üzgün melodi
// ============================================
void playGameOverMelody() {
  if(!soundEnabled) return;
  
  int pwmValue = map(soundVolume, MIN_VOLUME, MAX_VOLUME, 50, 255);
  analogWrite(BUZZER_PIN, pwmValue);
  
  // Düşen tonlar - üzgün melodi
  tone(BUZZER_PIN, 523, 200);  // C
  delay(220);
  tone(BUZZER_PIN, 494, 200);  // B
  delay(220);
  tone(BUZZER_PIN, 440, 200);  // A
  delay(220);
  tone(BUZZER_PIN, 392, 400);  // G - uzun
  delay(450);
  
  noTone(BUZZER_PIN);
  analogWrite(BUZZER_PIN, 0);
}

// ============================================
// EEPROM FONKSİYONLARI
// ============================================
void loadHighScore() {
  byte magicByte = EEPROM.read(EEPROM_MAGIC_ADDR);
  
  if(magicByte != EEPROM_MAGIC_VALUE) {
    highScore = 0;
    EEPROM.put(EEPROM_HIGHSCORE_ADDR, highScore);
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
  } else {
    EEPROM.get(EEPROM_HIGHSCORE_ADDR, highScore);
  }
}

void saveHighScore() {
  EEPROM.put(EEPROM_HIGHSCORE_ADDR, highScore);
}

void loadSoundSettings() {
  byte magicByte = EEPROM.read(EEPROM_MAGIC_ADDR);
  
  if(magicByte != EEPROM_MAGIC_VALUE) {
    soundEnabled = true;
    soundVolume = 3;
    saveSoundSettings();
  } else {
    soundEnabled = EEPROM.read(EEPROM_SOUND_ENABLED_ADDR);
    soundVolume = EEPROM.read(EEPROM_SOUND_VOLUME_ADDR);
    
    if(soundVolume < MIN_VOLUME || soundVolume > MAX_VOLUME) {
      soundVolume = 3;
    }
  }
}

void saveSoundSettings() {
  EEPROM.write(EEPROM_SOUND_ENABLED_ADDR, soundEnabled);
  EEPROM.write(EEPROM_SOUND_VOLUME_ADDR, soundVolume);
}
