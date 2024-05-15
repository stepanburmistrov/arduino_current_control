#include <Servo.h> // Подключаем библиотеку Servo для управления сервоприводами
#include "Wire.h"
#include "SDL_Arduino_INA3221.h"

SDL_Arduino_INA3221 ina3221; // создаём экземпляр класса датчика
// Три канала измерения датчика INA3221
#define CHANNEL_1 1
#define CHANNEL_2 2
#define CHANNEL_3 3

Servo part0; // Создаем объекты сервоприводов
Servo part1;
Servo part2;

Servo servos[] = { part0, part1, part2}; // Массив объектов сервоприводов для удобства управления
int servosCurrentPos[] = {135, 40, 90}; // Массив текущих позиций сервоприводов
int servosTargetPos[] = {135, 40, 90}; // Массив целевых позиций сервоприводов
uint32_t servosTimer[] = {0, 0, 0}; // Таймеры для контроля времени обновления положения каждого сервопривода
int sDelay = 40; // Задержка между обновлениями положения
uint32_t servosDelay[] = {sDelay, sDelay, sDelay}; // Массив задержек для каждого сервопривода
float maxCurrent = 200;
uint32_t printTimer = 0;
uint8_t gripperIsMoving = 0;
uint8_t lastGripperValue = 70;

const int bufferSize = 40; // Размер буфера для скользящего среднего
float currentMeasurements[bufferSize]; // Массив для хранения измерений
int measurementIndex = 0; // Индекс для текущего измерения
float currentAverage = 0; // Глобальная переменная для хранения текущего скользящего среднего
bool bufferFilled = false; // Флаг, указывающий, заполнен ли буфер


void parseSerialInput() {
  // Функция обработки данных, получаемых через Serial Monitor
  String inputString = ""; // Строка для хранения входных данных
  int inputArray[3]; // Массив для хранения распарсенных значений углов
  int inputIndex = 0; // Индекс для перебора элементов массива

  // Читаем данные из Serial пока они доступны
  while (Serial.available() > 0) {
    delay(2); // Небольшая задержка для стабилизации данных
    char inChar = Serial.read(); // Читаем символ из Serial

    // Проверяем на символ конца строки
    if (inChar == '|') {
      // Печатаем полученную строку для отладки
      Serial.print("Received String: ");
      Serial.println(inputString);

      // Преобразуем String в массив char для разбора
      char tempStr[inputString.length() + 1];
      inputString.toCharArray(tempStr, sizeof(tempStr));

      // Разбираем строку на отдельные числа
      char* ptr = strtok(tempStr, ";");
      while (ptr != NULL && inputIndex < 3) {
        inputArray[inputIndex] = atoi(ptr);
        // Выводим целевое положение для каждого сервопривода
        Serial.print("Servo ");
        Serial.print(inputIndex);
        Serial.print(" Target: ");
        Serial.println(inputArray[inputIndex]);
        inputIndex++;
        ptr = strtok(NULL, ";");
      }

      // Обновляем целевые позиции сервоприводов
      // for (int i = 0; i <= 2; i++) {
      //   servosTargetPos[i] = inputArray[i];
      // }
      for (int i = 0; i <= 2; i++) {
        servosDelay[i] = inputArray[1];
      }
      maxCurrent = inputArray[2];
      servosTargetPos[0] = 85 + inputArray[0];
      servosTargetPos[1] = 90 - inputArray[0];
      if (inputArray[0] < lastGripperValue) gripperIsMoving = 1;
      else gripperIsMoving = 0;
      lastGripperValue = inputArray[0];
      
      // Сброс переменных для следующего чтения
      inputString = "";
      inputIndex = 0;
    } else if (isdigit(inChar) || inChar == ';') {
      // Если символ является числом или разделителем, добавляем его к строке
      inputString += inChar;
    }
  }
}

void servoPosControl() {
  // Функция для плавного управления положением сервоприводов
  for (int i = 0; i <= 2; i++) {
    // Проверяем, прошло ли достаточно времени с последнего обновления положения
    if (millis() - servosTimer[i] > servosDelay[i]) {
      // Вычисляем разницу между текущим и целевым положением
      int delta = servosCurrentPos[i] == servosTargetPos[i] ? 0 : (servosCurrentPos[i] < servosTargetPos[i] ? 1 : -1);
      // Обновляем текущее положение
      servosCurrentPos[i] += delta;
      // Обновляем таймер
      servosTimer[i] = millis();
      // Устанавливаем новое положение сервопривода
      float current = getCurrent();
      
      if (current>maxCurrent and gripperIsMoving){
        Serial.println("AHTUNG!!!!");
        servosTargetPos[0] = servosCurrentPos[0] - delta*4;
        servosTargetPos[1] = servosCurrentPos[1] - delta*4;
        gripperIsMoving = 0;
        lastGripperValue = 0;
        break;
      }
      servos[i].write(servosCurrentPos[i]);
      
    }
  }
}

float getCurrent() {
  float current = ina3221.getCurrent_mA(CHANNEL_1);
  return current;
}

void updateCurrentMeasurement(float newMeasurement) {
    // Добавляем новое измерение в массив
    currentMeasurements[measurementIndex] = newMeasurement;
    
    // Перемещаем индекс на следующую позицию
    measurementIndex++;
    if (measurementIndex >= bufferSize) {
        measurementIndex = 0; // Возвращаемся к началу, если достигли конца массива
        bufferFilled = true; // Массив заполнен
    }
    
    // Вычисляем скользящее среднее
    float sum = 0;
    int count = bufferFilled ? bufferSize : measurementIndex; // Если буфер не заполнен, учитываем только добавленные измерения
    for (int i = 0; i < count; i++) {
        sum += currentMeasurements[i];
    }
    currentAverage = sum / count;
}

void setup() {
  Serial.begin(9600);
  part0.attach(11);
  part1.attach(12);
 ina3221.begin();

  Serial.print("ID=0x");
  int id = ina3221.getManufID();
  Serial.println(id, HEX);

  Serial.println("Measuring voltage and current with ina3221 ...");

}
void loop() {
  parseSerialInput(); // Обрабатываем входные данные из Serial Monitor
  servoPosControl(); // Управляем положением сервоприводов
  
    if (millis()-printTimer>20) {
      float currentMeasurement = getCurrent(); // Замените это на вашу функцию измерения тока
      updateCurrentMeasurement(currentMeasurement);
      Serial.println(currentAverage);
      printTimer = millis();
  }
}

//Откройте Serial Monitor
//Отправьте строку в формате "90;120;45;|" для установки углов поворота сервоприводов.