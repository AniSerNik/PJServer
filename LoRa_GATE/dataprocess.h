#ifndef dataprocess_h
#define dataprocess_h

#include <Arduino.h>

void processPackageTask(void *pvParameters);
// Функции
String decodeJsonFromBytes(uint8_t *recv_buf, uint8_t dev_id);
String getKeyFromId(uint8_t idKey);
int64_t quick_pow10(int n);

// Функции отправки на сервер и печати
void registationKeys(uint8_t from, uint8_t *recv_buf);
void deleteInfo(uint8_t from);

// Очередь для отправки данных на сервер
extern QueueHandle_t processQueue;

#endif