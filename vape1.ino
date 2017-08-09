/*********************************************************************
This is an example for our Monochrome OLEDs based on SSD1306 drivers

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/category/63_98

This example is for a 128x32 size display using I2C to communicate
3 pins are required to interface (2 I2C and one reset)

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.  
BSD license, check license.txt for more information
All text above, and the splash screen must be included in any redistribution
*********************************************************************/
//-----------------------------------НАСТРОЙКИ------------------------------------
#define initial_calibration 0  // калибровка вольтметра 1 - включить, 0 - выключить
#define welcome 0              // приветствие (слова GYVER VAPE при включении), 1 - включить, 0 - выключить
#define battery_info 0         // отображение напряжения аккумулятора при запуске, 1 - включить, 0 - выключить
#define sleep_timer 10         // таймер сна в секундах
#define vape_threshold 4       // отсечка затяжки, в секундах
#define turbo_mode 0           // турбо режим 1 - включить, 0 - выключить
#define battery_percent 0      // отображать заряд в процентах, 1 - включить, 0 - выключить
#define battery_low 2.8        // нижний порог срабатывания защиты от переразрядки аккумулятора, в Вольтах!
//-----------------------------------НАСТРОЙКИ------------------------------------

//-----------флажки-----------
boolean up_state, down_state, set_state, vape_state;
boolean up_flag, down_flag, set_flag, set_flag_hold, set_hold, vape_btt, vape_btt_f;
volatile boolean wake_up_flag, vape_flag;
boolean change_v_flag, change_w_flag, change_o_flag;
volatile byte mode, mode_flag = 1;
boolean flag;          // флаг, разрешающий подать ток на койл (защита от КЗ, обрыва, разрядки)
//-----------флажки-----------
//-----------пины-------------
#define mosfet 10      // пин мосфета (нагрев спирали)
#define battery 1      // пин измерения напряжения акума
//-----------пины-------------
//-----------кнопки-----------
#define butt_up 5      // кнопка вверх
#define butt_down 6    // кнпока вниз
#define butt_set 4     // кнопка выбора
#define butt_vape 3    // кнопка "парить"
//-----------кнопки-----------

int bat_vol, bat_volt_f, bat_percent;   // хранит напряжение на акуме
int PWM, PWM_f;           // хранит PWM сигнал

//-------переменные и коэффициенты для фильтра-------
int bat_old, PWM_old = 800;
float filter_k = 0.04;
float PWM_filter_k = 0.1;
//-------переменные и коэффициенты для фильтра-------

unsigned long last_time, vape_press, set_press, last_vape, wake_timer; // таймеры
int volts, watts;    // храним вольты и ватты
float ohms;          // храним омы
float my_vcc_const = 1.1;  // константа вольтметра
volatile byte vape_mode, vape_release_count;

#include <EEPROMex.h>   // библиотека для работы со внутренней памятью ардуино
#include <LowPower.h>   // библиотека сна
#include <TimerOne.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2


#define LOGO16_GLCD_HEIGHT 16 
#define LOGO16_GLCD_WIDTH  16 
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
void setup()   {                
  Serial.begin(9600);
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  // init done
  
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  //display.display();
  //delay(2000);

  // Clear the buffer.
  display.clearDisplay();
  delay(100);
  if (initial_calibration) calibration();  // калибровка, если разрешена
   //----читаем из памяти-----
  volts = EEPROM.readInt(0);
  watts = EEPROM.readInt(2);
  ohms = EEPROM.readFloat(4);
  my_vcc_const = 1.1;
  //----читаем из памяти-----

  Timer1.initialize(1500);          // таймер

  //---настройка кнопок и выходов-----
  pinMode(butt_up , INPUT_PULLUP);
  pinMode(butt_down , INPUT_PULLUP);
  pinMode(butt_set , INPUT_PULLUP);
  pinMode(butt_vape , INPUT_PULLUP);
  pinMode(mosfet , OUTPUT);
  //pinMode(disp_vcc , OUTPUT);
  //digitalWrite(disp_vcc, HIGH);
  Timer1.disablePwm(mosfet);    // принудительно отключить койл
  digitalWrite(mosfet, LOW);    // принудительно отключить койл
  //---настройка кнопок и выходов-----
  
  //------приветствие-----
  if (welcome) { 
    display.setCursor(50,0);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println("IG");        
    display.display();
    display.startscrollleft(0x00, 0x0F);
    delay(500);
    display.stopscroll();
    display.clearDisplay();
    display.setCursor(32,0);
    display.setTextSize(1);
    display.setTextColor(BLACK,WHITE);
    display.println("IGNA");        
    display.display();
    display.startscrollright(0x00, 0x0F);
    delay(500);
    display.stopscroll();
    display.clearDisplay();
    display.setCursor(16,0);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println("IGNATI");        
    display.display();
    display.startscrollleft(0x00, 0x0F);
    delay(500);
    display.stopscroll();
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(2);
    display.setTextColor(BLACK,WHITE);
    display.println("IGNATIUS");        
    display.display();
    delay(100);
    display.startscrollright(0x00, 0x0F);
    delay(500);
    display.stopscroll();
    display.clearDisplay();
  }
  //------приветствие-----
  // измерить напряжение аккумулятора
  bat_vol = readVcc();                                 // измерить напряжение аккумулятора в миллиВольтах
  bat_old=bat_vol;
  bat_percent = map(bat_vol, battery_low * 1000, 4200, 0, 99);
  // проверка заряда акума, если разряжен то прекратить работу
  if (bat_vol < battery_low * 1000) {
    flag = 0;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE); 
    display.setCursor(0,0);
    display.println("Sorry,bro.");
    display.println("Low battery!");
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print(bat_vol);display.println("mVolts");
    display.setCursor(98,0);
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE); // 'inverted' text
    display.print(bat_percent);
    if (bat_percent==100){
      display.println("%");
    }
    else{
      display.println(" %");
    }
    display.setCursor(98,8);
    display.print(watts);
    if (watts>99){
      display.println("W");
    }
    else{
      display.println(" W");
    }
    display.display();
    delay(2000);
    display.clearDisplay();
    Timer1.disablePwm(mosfet);    // принудительно отключить койл
    digitalWrite(mosfet, LOW);    // принудительно отключить койл
  } else {
    flag = 1;
  }

  if (battery_info) {  // отобразить заряд аккумулятора при первом включении
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE); 
      display.setCursor(0,0);
      display.println("IGNATIUS");
      display.println("Battery voltage");
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.print(bat_volt_f);display.println("mVolts");
      display.setCursor(98,0);
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE); // 'inverted' text
      display.print(bat_percent);
      if (bat_percent==100){
        display.println("%");
      }
      else{
        display.println(" %");
      }
      display.setCursor(98,8);
      display.print(watts);
      if (watts>99){
        display.println("W");
      }
      else{
        display.println(" W");
      }
      display.display();
      delay(1000);
      display.clearDisplay();
  }

  // draw a single pixel
  //display.drawPixel(10, 10, WHITE);
  //testdrawline();
  //testdrawrect();
  //testfillrect();
  //testdrawcircle();
  //display.fillCircle(display.width()/2, display.height()/2, 10, WHITE);// draw a white circle, 10 pixel radius
  //testdrawroundrect();
  //testfillroundrect();
  //testdrawtriangle();
  //testfilltriangle();
  //testdrawchar(); // draw the first ~12 characters in the font
  //testscrolltext();  // draw scrolling text
  //testdrawbitmap(logo16_glcd_bmp, LOGO16_GLCD_HEIGHT, LOGO16_GLCD_WIDTH);// draw a bitmap icon and 'animate' movement
  //display.drawBitmap(30, 16,  logo16_glcd_bmp, 16, 16, 1);  // miniature bitmap display
  //display.display();
  //delay(2000);
  //display.clearDisplay();
  
  /* text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("IGNATIUS");
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println((float)bat_vol / 1000, 2);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print(bat_vol); display.println("VOLTS");
  display.drawPixel(124,0, WHITE);
  display.drawPixel(123,1, WHITE);display.drawPixel(125,1, WHITE);
  display.drawPixel(122,2, WHITE);display.drawPixel(126,2, WHITE);
  display.drawPixel(122,3, WHITE);display.drawPixel(123,3, WHITE);display.drawPixel(125,3,  WHITE);display.drawPixel( 126,3, WHITE);
  display.drawPixel(123,4, WHITE);display.drawPixel(125,4,  WHITE);
  display.drawPixel(123,5, WHITE);display.drawPixel(125,5,  WHITE);
  display.drawPixel(123,6, WHITE);display.drawPixel(125,6,  WHITE);
  display.drawPixel(123,7, WHITE);display.drawPixel(125,7,  WHITE);
  display.drawPixel(122,8, WHITE);display.drawPixel(123,8,  WHITE);display.drawPixel(125,8,  WHITE);display.drawPixel(126,8,  WHITE);
  display.drawPixel(121,9, WHITE);display.drawPixel(127,9,  WHITE);
  display.drawPixel(121,10, WHITE);display.drawPixel(124,10, WHITE);display.drawPixel(127,10,  WHITE);
  display.drawPixel(122,11, WHITE);display.drawPixel(123,11,  WHITE);display.drawPixel(125,11,  WHITE);display.drawPixel( 126,11, WHITE);
  display.display();
  delay(2000);
  display.clearDisplay();
*/
/*
  // invert the display
  display.invertDisplay(true);
  delay(1000); 
  display.invertDisplay(false);
  delay(1000); 
  display.clearDisplay();
*/
}


void loop() {
  if (millis() - last_time > 50) {                       // 20 раз в секунду измеряем напряжение
    last_time = millis();
    bat_vol = readVcc();                                 // измерить напряжение аккумулятора в миллиВольтах
    bat_volt_f = filter_k * bat_vol + (1 - filter_k) * bat_old;  // фильтруем
    bat_old = bat_volt_f;                                // фильтруем
    bat_percent = map(bat_volt_f, battery_low * 1000, 4200, 0, 99);
    if (bat_volt_f < battery_low * 1000) {               // если напряжение меньше минимального
      flag = 0;                                          // прекратить работу
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE); 
      display.setCursor(0,0);
      display.println("Sorry,bro.");
      display.println("Low battery!");
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.print(bat_volt_f);display.println("mVolts");
      display.setCursor(98,0);
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE); // 'inverted' text
      display.print(bat_percent);
      if (bat_percent==100){
        display.println("%");
      }
      else{
        display.println(" %");
      }
      display.setCursor(98,8);
      display.print(watts);
      if (watts>99){
        display.println("W");
      }
      else{
        display.println(" W");
      }
      display.display();
      //delay(2000);
      //display.clearDisplay();
      Timer1.disablePwm(mosfet);    // принудительно отключить койл
      digitalWrite(mosfet, LOW);    // принудительно отключить койл
    }
  }
  //-----------опрос кнопок-----------
  up_state = !digitalRead(butt_up);
  down_state = !digitalRead(butt_down);
  set_state = !digitalRead(butt_set);
  vape_state = !digitalRead(butt_vape);
   // если нажата любая кнопка, "продлить" таймер ухода в сон
  if (up_state || down_state || set_state || vape_state) wake_timer = millis();
  //-----------опрос кнопок-----------

  //service_mode();  // раскомментировать для отладки кнопок
  // показывает, какие кнопки нажаты или отпущены
  // использовать для проерки правильности подключения
   //---------------------отработка нажатия SET и изменение режимов---------------------
  if (flag) {                              // если акум заряжен
    if (set_state && !set_hold) {          // если кнпока нажата
      set_hold = 1;
      set_press = millis();                // начинаем отсчёт
      while (millis() - set_press < 300) {
        if (digitalRead(butt_set)) {       // если кнопка отпущена до 300 мс
          set_hold = 0;
          set_flag = 1;
          break;
        }
      }
    }
    if (set_hold && set_state) {           // если кнопка всё ещё удерживается
      if (!set_flag_hold) {
        display.clearDisplay();
        set_flag_hold = 1;
      }
      if (round(millis() / 150) % 2 == 0) {
        if (!battery_percent) {
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(WHITE); 
            display.setCursor(0,0);
            display.println("IGNATIUS");
            display.println("Current voltage");
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.print(bat_volt_f);display.println("mVolts");
            display.setCursor(98,0);
            display.setTextSize(1);
            display.setTextColor(BLACK, WHITE); // 'inverted' text
            display.print(bat_percent);
            if (bat_percent==100){
              display.println("%");
            }
            else{
              display.println(" %");
            }
            display.setCursor(98,8);
            display.print(watts);
            if (watts>99){
              display.println("W");
            }
            else{
              display.println(" W");
            }
            display.display();
          //delay(2000);
          //display.clearDisplay();
        }
      }
    }
    if (set_hold && !set_state && set_flag_hold) {  // если удерживалась и была отпущена
      set_hold = 0;
      set_flag_hold = 0;
      mode_flag = 1;
    }

    if (!set_state && set_flag) {  // если нажали-отпустили
      set_hold = 0;
      set_flag = 0;
      mode++;                      // сменить режим
      mode_flag = 1;
      if (mode > 2) mode = 0;      // ограничение на 3 режима
    }
    // ----------------------отработка нажатия SET и изменение режимов---------------------------

    // ------------------режим ВАРИВОЛЬТ-------------------
    if (mode == 0 && !vape_state && !set_hold) {
      if (mode_flag) {                     // приветствие
        mode_flag = 0;
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE); 
        display.setCursor(0,0);
        display.println("IGNATIUS");
        display.println("It's time to");
        display.setTextSize(2);
        display.println("VARIVOLT");
        display.setCursor(98,0);
        display.setTextSize(1);
        display.setTextColor(BLACK, WHITE); // 'inverted' text
        display.print(bat_percent);
        if (bat_percent==100){
          display.println("%");
        }
        else{
          display.println(" %");
        }
        display.setCursor(98,8);
        display.print(watts);
        if (watts>99){
          display.println("W");
        }
        else{
          display.println(" W");
        }
        display.display();
        delay(400);
        display.clearDisplay();
      }
      //---------кнопка ВВЕРХ--------
      if (up_state && !up_flag) {
        volts += 100;
        volts = min(volts, bat_volt_f);  // ограничение сверху на текущий заряд акума
        up_flag = 1;
        display.clearDisplay();
      }
      if (!up_state && up_flag) {
        up_flag = 0;
        change_v_flag = 1;
      }
      //---------кнопка ВВЕРХ--------

      //---------кнопка ВНИЗ--------
      if (down_state && !down_flag) {
        volts -= 100;
        volts = max(volts, 0);
        down_flag = 1;
        display.clearDisplay();
      }
      if (!down_state && down_flag) {
        down_flag = 0;
        change_v_flag = 1;
      }
      //---------кнопка ВНИЗ--------
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE); 
      display.setCursor(0,0);
      display.println("IGNATIUS");
      display.println("Current voltage:");
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.println((float)volts / 1000, 2);display.println("Volts");
      display.setCursor(98,0);
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE); // 'inverted' text
      display.print(bat_percent);
      if (bat_percent==100){
        display.println("%");
      }
      else{
        display.println(" %");
      }
      display.setCursor(98,8);
      display.print(watts);
      if (watts>99){
        display.println("W");
      }
      else{
        display.println(" W");
      }
      display.display();
    }
    // ------------------режим ВАРИВОЛЬТ-------------------


    // ------------------режим ВАРИВАТТ-------------------
    if (mode == 1 && !vape_state && !set_hold) {
      if (mode_flag) {                     // приветствие
        mode_flag = 0;
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE); 
        display.setCursor(0,0);
        display.println("IGNATIUS");
        display.println("It's time to");
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.println("VARIWATT");
        display.setCursor(98,0);
        display.setTextSize(1);
        display.setTextColor(BLACK, WHITE); // 'inverted' text
        display.print(bat_percent);
        if (bat_percent==100){
          display.println("%");
        }
        else{
          display.println(" %");
        }
        display.setCursor(98,8);
        display.print(watts);
        if (watts>99){
          display.println("W");
        }
        else{
          display.println(" W");
        }
        display.display();
        display.clearDisplay();
      }
      //---------кнопка ВВЕРХ--------
      if (up_state && !up_flag) {
        watts += 1;
        byte maxW = (sq((float)bat_volt_f / 1000)) / ohms;
        watts = min(watts, maxW);               // ограничение сверху на текущий заряд акума и сопротивление
        up_flag = 1;
        display.clearDisplay();
      }
      if (!up_state && up_flag) {
        up_flag = 0;
        change_w_flag = 1;
      }
      //---------кнопка ВВЕРХ--------

      //---------кнопка ВНИЗ--------
      if (down_state && !down_flag) {
        watts -= 1;
        watts = max(watts, 0);
        down_flag = 1;
        display.clearDisplay();
      }
      if (!down_state && down_flag) {
        down_flag = 0;
        change_w_flag = 1;
      }
      //---------кнопка ВНИЗ--------
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE); 
      display.setCursor(0,0);
      display.println("Current wattage");
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.print(watts);display.println("Watts");
      display.setCursor(98,0);
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE); // 'inverted' text
      display.print(bat_percent);
      if (bat_percent==100){
        display.println("%");
      }
      else{
        display.println(" %");
      }
      display.setCursor(98,8);
      display.print(watts);
      if (watts>99){
        display.println("W");
      }
      else{
        display.println(" W");
      }
      display.display();
    }
    // ------------------режим ВАРИВАТТ--------------

    // ----------режим установки сопротивления-----------
    if (mode == 2 && !vape_state && !set_hold) {
      if (mode_flag) {                     // приветствие
        mode_flag = 0;
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE); 
        display.setCursor(0,0);
        display.println("IGNATIUS");
        display.println("Enter resistance");
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.print(ohms);display.println("Ohms");
        display.setCursor(98,0);
        display.setTextSize(1);
        display.setTextColor(BLACK, WHITE); // 'inverted' text
        display.print(bat_percent);
        if (bat_percent==100){
          display.println("%");
        }
        else{
          display.println(" %");
        }
        display.setCursor(98,8);
        display.print(watts);
        if (watts>99){
          display.println("W");
        }
        else{
          display.println(" W");
        }
        display.display();
        delay(400);
        display.clearDisplay();
      }
      //---------кнопка ВВЕРХ--------
      if (up_state && !up_flag) {
        ohms += 0.05;
        ohms = min(ohms, 3);
        up_flag = 1;
        display.clearDisplay();
      }
      if (!up_state && up_flag) {
        up_flag = 0;
        change_o_flag = 1;
      }
      //---------кнопка ВВЕРХ--------

      //---------кнопка ВНИЗ--------
      if (down_state && !down_flag) {
        ohms -= 0.05;
        ohms = max(ohms, 0);
        down_flag = 1;
        display.clearDisplay();
      }
      if (!down_state && down_flag) {
        down_flag = 0;
        change_o_flag = 1;
      }
      //---------кнопка ВНИЗ--------
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE); 
        display.setCursor(0,0);
        display.println("IGNATIUS");
        display.println("Enter resistance");
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.print(ohms,2);display.println(".Ohms");
        display.setCursor(98,0);
        display.setTextSize(1);
        display.setTextColor(BLACK, WHITE); // 'inverted' text
        display.print(bat_percent);
        if (bat_percent==100){
          display.println("%");
        }
        else{
          display.println(" %");
        }
        display.setCursor(98,8);
        display.print(watts);
        if (watts>99){
          display.println("W");
        }
        else{
          display.println(" W");
        }
        display.display();
    }
    // ----------режим установки сопротивления-----------

    //---------отработка нажатия кнопки парения-----------
    if (vape_state && flag && !wake_up_flag) {

      if (!vape_flag) {
        vape_flag = 1;
        vape_mode = 1;            // первичное нажатие
        delay(20);                // анти дребезг (сделал по-тупому, лень)
        vape_press = millis();    // первичное нажатие
      }

      if (vape_release_count == 1) {
        vape_mode = 2;               // двойное нажатие
        delay(20);                   // анти дребезг (сделал по-тупому, лень)
      }
      if (vape_release_count == 2) {
        vape_mode = 3;               // тройное нажатие
      }

      if (millis() - vape_press > vape_threshold * 1000) {  // "таймер затяжки"
        vape_mode = 0;
        digitalWrite(mosfet, 0);
      }

      if (vape_mode == 1) {                                           // обычный режим парения
        if (round(millis() / 150) % 2 == 0){
          display.clearDisplay();
          display.setTextSize(2);
          display.setTextColor(WHITE);
          display.setCursor(0,0);
          display.print("VAPE");
          display.setCursor(64,0);display.print("THIS");
          display.setCursor(128,0);display.print("OUT,MDFK!");
          display.display();   
        }else{
          display.clearDisplay();
          display.setCursor(0,0);
          display.setTextSize(2);
          display.setTextColor(BLACK,WHITE);
          display.setCursor(0,0);
          display.print("VAPE");
          display.setCursor(64,0);display.print("THIS");
          display.setCursor(128,0);display.print("OUT,MDFK!");
          display.display();
        }                    // мигать медленно
        if (mode == 0) {                                              // если ВАРИВОЛЬТ
          PWM = (float)volts / bat_volt_f * 1024;                     // считаем значение для ШИМ сигнала
          if (PWM > 1023) PWM = 1023;                                 // ограничил PWM "по тупому", потому что constrain сука не работает!
          PWM_f = PWM_filter_k * PWM + (1 - PWM_filter_k) * PWM_old;  // фильтруем
          PWM_old = PWM_f;                                            // фильтруем
        }
        Timer1.pwm(mosfet, PWM_f);                                    // управление мосфетом
      }
      if (vape_mode == 2 && turbo_mode) {                             // турбо режим парения (если включен)
        if (round(millis() / 50) % 2 == 0){
          display.clearDisplay();
          display.setCursor(0,0);
          display.setTextSize(2);
          display.setTextColor(WHITE);
          display.setCursor(0,0);
          display.print("TURBO");
          display.setCursor(64,0);display.print("VAPE");
          display.setCursor(128,0);display.print("OUT,MDFK!");
          display.display();
        }else{
          display.clearDisplay();
          display.setCursor(0,0);
          display.setTextSize(2);
          display.setTextColor(BLACK,WHITE);
          display.setCursor(0,0);
          display.print("TURBO");
          display.setCursor(64,0);display.print("VAPE");
          display.setCursor(128,0);display.print("OUT,MDFK!");
          display.display();
        }                            // мигать быстро
        digitalWrite(mosfet, 1);                                      // херачить на полную мощность
      }
      if (vape_mode == 3) {                                           // тройное нажатие
        vape_release_count = 0;
        vape_mode = 1;
        vape_flag = 0;
        good_night();    // вызвать функцию сна
      }
      vape_btt = 1;
    }

    if (!vape_state && vape_btt) {  // если кнопка ПАРИТЬ отпущена
      if (millis() - vape_press > 180) {
        vape_release_count = 0;
        vape_mode = 0;
        vape_flag = 0;
      }
      vape_btt = 0;
      if (vape_mode == 1) {
        vape_release_count = 1;
        vape_press = millis();
      }
      if (vape_mode == 2) vape_release_count = 2;

      digitalWrite(mosfet, 0);
      display.clearDisplay();
      mode_flag = 0;

      // если есть изменения в настройках, записать в память
      if (change_v_flag) {
        EEPROM.writeInt(0, volts);
        change_v_flag = 0;
      }
      if (change_w_flag) {
        EEPROM.writeInt(2, watts);
        change_w_flag = 0;
      }
      if (change_o_flag) {
        EEPROM.writeFloat(4, ohms);
        change_o_flag = 0;
      }
      // если есть изменения в настройках, записать в память
    }
    if (vape_state && !flag) { // если акум сел, а мы хотим подымить
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE); 
      display.setCursor(0,0);
      display.println("Sorry,bro.");
      display.println("Low battery!");
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.println(bat_volt_f);display.println("mVolts");
      display.setCursor(98,0);
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE); // 'inverted' text
      display.print(bat_percent);
      if (bat_percent==100){
        display.println("%");
      }
      else{
        display.println(" %");
      }
      display.setCursor(98,8);
      display.print(watts);
      if (watts>99){
        display.println("W");
      }
      else{
        display.println(" W");
      }
      display.display();
      delay(1000);
      vape_flag = 1;
    }
    //---------отработка нажатия кнопки парения-----------
  }

  if (wake_up_flag) wake_puzzle();                   // вызвать функцию 5 нажатий для пробудки

  if (millis() - wake_timer > sleep_timer * 1000) {  // если кнопки не нажимались дольше чем sleep_timer секунд
    good_night();
  }
} // конец loop

//------функция, вызываемая при выходе из сна (прерывание)------
void wake_up() {
  //digitalWrite(disp_vcc, HIGH);  // включить дисплей
  Timer1.disablePwm(mosfet);    // принудительно отключить койл
  digitalWrite(mosfet, LOW);    // принудительно отключить койл
  wake_timer = millis();         // запомнить время пробуждения
  wake_up_flag = 1;
  vape_release_count = 0;
  vape_mode = 0;
  vape_flag = 0;
  mode_flag = 1;
}
//------функция, вызываемая при выходе из сна (прерывание)------

//------функция 5 нажатий для полного пробуждения------
void wake_puzzle() {
  detachInterrupt(1);    // отключить прерывание
  vape_btt_f = 0;
  boolean wake_status = 0;
  byte click_count = 0;
  while (1) {
    vape_state = !digitalRead(butt_vape);

    if (vape_state && !vape_btt_f) {
      vape_btt_f = 1;
      click_count++;
      switch (click_count) {
        case 1:
          display.setCursor(50,0);
          display.setTextSize(1);
          display.setTextColor(WHITE);
          display.println("IG");        
          display.display();
          display.clearDisplay();
          break;
        case 2: 
          display.setCursor(32,0);
          display.setTextSize(1);
          display.setTextColor(BLACK,WHITE);
          display.println("IGNA");        
          display.display();
          display.clearDisplay();
          break;
        case 3:
         display.setCursor(16,0);
          display.setTextSize(2);
          display.setTextColor(WHITE);
          display.println("IGNATI");        
          display.display();
          display.clearDisplay();
          break;
        case 4:
          display.setCursor(0,0);
          display.setTextSize(2);
          display.setTextColor(BLACK,WHITE);
          display.println("IGNATIUS");        
          display.display();
          display.clearDisplay();
          break;
      }
      if (click_count > 4) {               // если 5 нажатий сделаны за 3 секунды
        wake_status = 1;                   // флаг "проснуться"
        break;
      }
    }
    if (!vape_state && vape_btt_f) {
      vape_btt_f = 0;
      delay(70);
    }
    if (millis() - wake_timer > 3000) {    // если 5 нажатий не сделаны за 3 секунды
      wake_status = 0;                     // флаг "спать"
      break;
    }
  }
  if (wake_status) {
    wake_up_flag = 0;
    display.clearDisplay();
    delay(100);
  } else {
    display.clearDisplay();
    good_night();     // спать
  }
}
//------функция 5 нажатий для полного пробуждения------

//-------------функция ухода в сон----------------
void good_night() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("GOOD");
  display.setCursor(64,0);display.print("BYE,");
  display.setCursor(128,0);display.print("BRO!");
  display.display();
  delay(100);
 
  display.startscrollright(0x00, 0x0F);
  delay(4200);
  display.stopscroll();
  delay(500);
  display.clearDisplay();
  display.display();
  Timer1.disablePwm(mosfet);    // принудительно отключить койл
  digitalWrite(mosfet, LOW);    // принудительно отключить койл
  delay(50);

  // подать 0 на все пины питания дисплея
  //digitalWrite(disp_vcc, LOW);

  delay(50);
  attachInterrupt(1, wake_up, FALLING);                   // подключить прерывание для пробуждения
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);    // спать. mode POWER_OFF, АЦП выкл
}
//-------------функция ухода в сон----------------

//----------режим теста кнопок----------
void service_mode() {
  if (set_state && !set_flag) {
    set_flag = 1;
    Serial.println("SET pressed");
  }
  if (!set_state && set_flag) {
    set_flag = 0;
    Serial.println("SET released");
  }
  if (up_state && !up_flag) {
    up_flag = 1;
    Serial.println("UP pressed");
  }
  if (!up_state && up_flag) {
    up_flag = 0;
    Serial.println("UP released");
  }
  if (down_state && !down_flag) {
    down_flag = 1;
    Serial.println("DOWN pressed");
  }
  if (!down_state && down_flag) {
    down_flag = 0;
    Serial.println("DOWN released");
  }
  if (vape_state && !vape_flag) {
    vape_flag = 1;
    Serial.println("VAPE pressed");
  }
  if (!vape_state && vape_flag) {
    vape_flag = 0;
    Serial.println("VAPE released");
  }
}
//----------режим теста кнопок----------


void testdrawbitmap(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  uint8_t icons[NUMFLAKES][3];
 
  // initialize
  for (uint8_t f=0; f< NUMFLAKES; f++) {
    icons[f][XPOS] = random(display.width());
    icons[f][YPOS] = 0;
    icons[f][DELTAY] = random(5) + 1;
    
    Serial.print("x: ");
    Serial.print(icons[f][XPOS], DEC);
    Serial.print(" y: ");
    Serial.print(icons[f][YPOS], DEC);
    Serial.print(" dy: ");
    Serial.println(icons[f][DELTAY], DEC);
  }

  while (1) {
    // draw each icon
    for (uint8_t f=0; f< NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, WHITE);
    }
    display.display();
    delay(200);
    
    // then erase it + move it
    for (uint8_t f=0; f< NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, BLACK);
      // move it
      icons[f][YPOS] += icons[f][DELTAY];
      // if its gone, reinit
      if (icons[f][YPOS] > display.height()) {
        icons[f][XPOS] = random(display.width());
        icons[f][YPOS] = 0;
        icons[f][DELTAY] = random(5) + 1;
      }
    }
   }
}


void testdrawchar(void) {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  for (uint8_t i=0; i < 168; i++) {
    if (i == '\n') continue;
    display.write(i);
    if ((i > 0) && (i % 21 == 0))
      display.println();
  }    
  display.display();
  delay(1);
}

void testdrawcircle(void) {
  for (int16_t i=0; i<display.height(); i+=2) {
    display.drawCircle(display.width()/2, display.height()/2, i, WHITE);
    display.display();
    delay(1);
  }
}

void testfillrect(void) {
  uint8_t color = 1;
  for (int16_t i=0; i<display.height()/2; i+=3) {
    // alternate colors
    display.fillRect(i, i, display.width()-i*2, display.height()-i*2, color%2);
    display.display();
    delay(1);
    color++;
  }
}

void testdrawtriangle(void) {
  for (int16_t i=0; i<min(display.width(),display.height())/2; i+=5) {
    display.drawTriangle(display.width()/2, display.height()/2-i,
                     display.width()/2-i, display.height()/2+i,
                     display.width()/2+i, display.height()/2+i, WHITE);
    display.display();
    delay(1);
  }
}

void testfilltriangle(void) {
  uint8_t color = WHITE;
  for (int16_t i=min(display.width(),display.height())/2; i>0; i-=5) {
    display.fillTriangle(display.width()/2, display.height()/2-i,
                     display.width()/2-i, display.height()/2+i,
                     display.width()/2+i, display.height()/2+i, WHITE);
    if (color == WHITE) color = BLACK;
    else color = WHITE;
    display.display();
    delay(1);
  }
}

void testdrawroundrect(void) {
  for (int16_t i=0; i<display.height()/2-2; i+=2) {
    display.drawRoundRect(i, i, display.width()-2*i, display.height()-2*i, display.height()/4, WHITE);
    display.display();
    delay(1);
  }
}

void testfillroundrect(void) {
  uint8_t color = WHITE;
  for (int16_t i=0; i<display.height()/2-2; i+=2) {
    display.fillRoundRect(i, i, display.width()-2*i, display.height()-2*i, display.height()/4, color);
    if (color == WHITE) color = BLACK;
    else color = WHITE;
    display.display();
    delay(1);
  }
}
   
void testdrawrect(void) {
  for (int16_t i=0; i<display.height()/2; i+=2) {
    display.drawRect(i, i, display.width()-2*i, display.height()-2*i, WHITE);
    display.display();
    delay(1);
  }
}

void testdrawline() {  
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, WHITE);
    display.display();
    delay(1);
  }
  for (int16_t i=0; i<display.height(); i+=4) {
    display.drawLine(0, 0, display.width()-1, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);
  
  display.clearDisplay();
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(0, display.height()-1, i, 0, WHITE);
    display.display();
    delay(1);
  }
  for (int16_t i=display.height()-1; i>=0; i-=4) {
    display.drawLine(0, display.height()-1, display.width()-1, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);
  
  display.clearDisplay();
  for (int16_t i=display.width()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, i, 0, WHITE);
    display.display();
    delay(1);
  }
  for (int16_t i=display.height()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, 0, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, WHITE);
    display.display();
    delay(1);
  }
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(display.width()-1, 0, i, display.height()-1, WHITE); 
    display.display();
    delay(1);
  }
  delay(250);
}

void testscrolltext(void) {
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10,0);
  display.clearDisplay();
  display.println("IGNATIUS-xyu");
  display.display();
  delay(1);
 
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);    
  display.startscrolldiagright(0x00, 0x07);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x07);
  delay(2000);
  display.stopscroll();
}

void calibration() {
  //--------калибровка----------
  for (byte i = 0; i < 7; i++) EEPROM.writeInt(i, 0);          // чистим EEPROM для своих нужд
  my_vcc_const = 1.1;
  Serial.print("Real VCC is: "); Serial.println(readVcc());     // общаемся с пользователем
  Serial.println("Write your VCC (in millivolts)");
  while (Serial.available() == 0); int Vcc = Serial.parseInt(); // напряжение от пользователя
  float real_const = (float)1.1 * Vcc / readVcc();              // расчёт константы
  Serial.print("New voltage constant: "); Serial.println(real_const, 3);
  EEPROM.writeFloat(8, real_const);                             // запись в EEPROM
  while (1); // уйти в бесконечный цикл
  //------конец калибровки-------
}

long readVcc() { //функция чтения внутреннего опорного напряжения, универсальная (для всех ардуин)
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both
  long result = (high << 8) | low;

  result = my_vcc_const * 1023 * 1000 / result; // расчёт реального VCC
  return result; // возвращает VCC
}
