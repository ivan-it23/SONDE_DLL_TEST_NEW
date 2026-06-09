# Документация: Симметризация фаз в структуре GP_DATA

## 1. Общее описание

Данный документ описывает логику вычисления и симметризации фаз для индукционных зондов типа **LWD_4Tx** и **AUTONOM_5Tx** (включая SDR-версию). Документация применима к структуре данных `GP_DATA` и форматам файлов `.DEV` и `.biv`.

**Исключение:** Картографические зонды (CARTOGRAPH_) в данном документе не рассматриваются, так как используют структуру `ALL_DATA` с более сложной обработкой сырых секторных данных.

---

## 2. Структура GP_DATA

```cpp
struct GP_DATA {
    uint32_t signature;        // Сигнатура зонда (идентификатор типа)
    uint32_t condition;        // Условия измерения (битовая маска активных Tx)
    uint32_t frame;            // Номер кадра (временная метка)
    float temperature;         // Температура зонда
    
    // Данные 400 кГц ([0]) и 2000 кГц ([1]) для 5 передатчиков
    float rho_smt[2][5];       // Удельное сопротивление, Ом·м
    float phase_smt[2][5];     // Фаза в градусах (симметризованная)
    float AM_RX_1[2][5];       // Амплитуда приёмника 1
    float AM_RX_2[2][5];       // Амплитуда приёмника 2
    
    // Нулевые значения (калибровка на воздухе)
    float ZERO_AM_RX_1[2];     // Нулевая амплитуда Rx1 [400, 2000]
    float ZERO_AM_RX_2[2];     // Нулевая амплитуда Rx2 [400, 2000]
    
    // Разность фаз между приёмниками (ключевой параметр)
    float DELTA_PH[2][5];       // Δφ = φ(Rx2) - φ(Rx1) в градусах
    float ZERO_dPH[2];         // Нулевой сдвиг разности фаз [400, 2000]
};
```

**Общий размер структуры:** 240 байт (для 5-передатчикового зонда)

---

## 3. Чтение данных из файлов

### 3.1 Формат .biv (бинарный внутренний)

Данные сохраняются в виде последовательных записей `_5Tx_DATA` или `_4Tx_DATA` без заголовков.

**Структура для AUTONOM_5Tx:**
```cpp
struct _5Tx_DATA {
    uint32_t signature;
    uint32_t condition;
    uint32_t frame;
    float temperature;
    float rho_smt[2][5];
    float phase_smt[2][5];
    float AM_RX_1[2][5];
    float ZERO_AM_RX_1[2];
    float AM_RX_2[2][5];
    float ZERO_AM_RX_2[2];
    float DELTA_PH[2][5];
    float ZERO_dPH[2];
};
```

**Чтение из файла:**
```cpp
_5Tx_DATA _5Tx_data;
param->fin->seekg(n * sizeof(_5Tx_DATA));
param->fin->read((char*)&_5Tx_data, sizeof(_5Tx_DATA));
```

**Размер записи:** 240 байт

### 3.2 Формат .DEV (устройство)

Используется при прямом чтении с прибора. Структура данных та же, но перед каждой записью добавляется 11-байтный преамбула (служебные данные протокола передачи).

```cpp
uint8_t *buff = new uint8_t[12];
param->fin->seekg(n * (sizeof(_5Tx_DATA) + 11));
param->fin->read((char*)buff, 11);        // Преамбула
param->fin->read((char*)&_5Tx_data, sizeof(_5Tx_DATA));
```

**Размер записи в файле:** 240 + 11 = 251 байт

---

## 4. Преобразование данных (transform_data)

Функция `transform_data()` конвертирует типизированные данные зонда в универсальную структуру `GP_DATA`.

### 4.1 Для AUTONOM_5Tx / AUTONOM_5Tx_SDR

Данные копируются напрямую, так как структуры идентичны:

```cpp
if constexpr (is_same_v<T, _5Tx_DATA>) {
    gp_data.signature = current.signature;
    gp_data.condition = current.condition;
    gp_data.frame = current.frame;
    gp_data.temperature = current.temperature;
    
    for (int freq = 0; freq < 2; freq++) {      // 0=400кГц, 1=2000кГц
        gp_data.ZERO_AM_RX_1[freq] = current.ZERO_AM_RX_1[freq];
        gp_data.ZERO_AM_RX_2[freq] = current.ZERO_AM_RX_2[freq];
        gp_data.ZERO_dPH[freq] = current.ZERO_dPH[freq];
        
        for (int Tx = 0; Tx < 5; Tx++) {        // Tx1-Tx5
            gp_data.phase_smt[freq][Tx] = current.phase_smt[freq][Tx];
            gp_data.rho_smt[freq][Tx] = current.rho_smt[freq][Tx];
            gp_data.AM_RX_1[freq][Tx] = current.AM_RX_1[freq][Tx];
            gp_data.AM_RX_2[freq][Tx] = current.AM_RX_2[freq][Tx];
            gp_data.DELTA_PH[freq][Tx] = current.DELTA_PH[freq][Tx];
        }
    }
}
```

### 4.2 Для LWD_4Tx

Вычисление разности фаз происходит в момент преобразования:

```cpp
if constexpr (is_same_v<T, _4Tx_DATA>) {
    for (int freq = 0; freq < 2; freq++) {
        for (int Tx = 0; Tx < 4; Tx++) {        // Только Tx1-Tx4
            // Вычисление разности фаз между приёмниками
            float signal = current.PH_RX_2[freq][Tx] - current.PH_RX_1[freq][Tx];
            
            // Нормализация в диапазон [-180°, +180°]
            if (fabs(signal) < 40) {            // Проверка на выбросы
                while (signal >= PI) signal -= 2 * PI;
                while (signal <= -PI) signal += 2 * PI;
            } else { 
                signal = 0.0;                   // Отбраковка некорректных данных
            }
            
            gp_data.DELTA_PH[freq][Tx] = signal;
        }
    }
}
```

---

## 5. Симметризация фаз (функция simmetry)

### 5.1 Принцип работы

Симметризация корректирует фазы с учётом взаимного влияния передатчиков. Используется матричный метод с весовыми коэффициентами.

**Математическая модель:**
```
PH_sym[Tx] = Σ(K[Tx][k] × PH_raw[k])  для k = 0..3
RO_sym[Tx] = 10^(Σ(K[Tx][k] × log10(RO_raw[k])))
```

### 5.2 Матрицы коэффициентов

Матрица `K[4][4]` выбирается в зависимости от condition (какие передатчики активны):

#### Случай 1: Все 4 передатчика активны (condition = 0b00001111)
```
        Tx1   Tx2   Tx3   Tx4
Tx1   +0.75 +0.50 -0.25 +0.00
Tx2   +0.25 +0.50 +0.25 +0.00
Tx3   +0.00 +0.25 +0.50 +0.25
Tx4   +0.00 -0.25 +0.50 +0.75
```

#### Случай 2: Активны Tx2, Tx3, Tx4 (condition = 0b00000111)
```
        Tx1   Tx2   Tx3   Tx4
Tx1   +0.00 +1.25 +0.50 -0.75
Tx2   +0.00 +0.75 +0.50 -0.25
Tx3   +0.00 +0.25 +0.50 +0.25
Tx4   +0.00 -0.25 +0.50 +0.75
```

#### Случай 3: Активны Tx1, Tx2, Tx3 (condition = 0b00001110)
```
        Tx1   Tx2   Tx3   Tx4
Tx1   +0.75 +0.50 -0.25 +0.00
Tx2   +0.25 +0.50 +0.25 +0.00
Tx3   -0.25 +0.50 +0.75 +0.00
Tx4   -0.75 +0.50 +1.25 +0.00
```

#### Случай 4: Активны Tx1, Tx3, Tx4 (condition = 0b00001011)
```
        Tx1   Tx2   Tx3   Tx4
Tx1   +1.25 +0.00 -0.75 +0.50
Tx2   +0.75 +0.00 -0.25 +0.50
Tx3   +0.25 +0.00 +0.25 +0.50
Tx4   -0.25 +0.00 +0.75 +0.50
```

#### Случай 5: Активны Tx1, Tx2, Tx4 (condition = 0b00001101)
```
        Tx1   Tx2   Tx3   Tx4
Tx1   +0.50 +0.75 +0.00 -0.25
Tx2   +0.50 +0.25 +0.00 +0.25
Tx3   +0.50 -0.25 +0.00 +0.75
Tx4   +0.50 -0.75 +0.00 +1.25
```

### 5.3 Реализация симметризации

```cpp
uint16_t simmetry(struct GP_DATA *Data, struct METROLOGY_GP *metro, 
                  uint16_t freq, uint16_t condition) {
    float K[4][4];              // Матрица коэффициентов
    uint16_t formula;         // Возвращаемый код формулы
    
    // Выбор матрицы по condition
    formula = formula_simmetry(K, condition);
    
    // Буферы для симметризованных значений
    float ph[4] = {0, 0, 0, 0};
    float ro[4] = {0, 0, 0, 0};
    
    // Матричное умножение для каждого передатчика
    for (int Tx = 0; Tx < 4; Tx++) {
        for (int k = 0; k < 4; k++) {
            // Симметризация фазы (линейная)
            ph[Tx] += K[Tx][k] * Data->phase_smt[freq][k];
            
            // Симметризация сопротивления (в логарифмической шкале)
            ro[Tx] += K[Tx][k] * log10(Data->rho_smt[freq][k]);
        }
        
        // Запись результатов
        Data->phase_smt[freq][Tx] = ph[Tx];
        Data->rho_smt[freq][Tx] = pow(10, ro[Tx]);
    }
    
    return formula;
}
```

---

## 6. Влияние метрологии на вычисление фаз

### 6.1 Структура метрологических данных

```cpp
struct METROLOGY_GP {
    uint32_t signature;        // Должен совпадать с GP_DATA.signature
    uint32_t serial;           // Серийный номер зонда
    uint32_t D_sonde_mm;       // Диаметр зонда
    
    // Геометрические параметры передатчиков (мм)
    float L1[5];               // Расстояние Tx->ближний приёмник
    float L2[5];               // Расстояние Tx->дальний приёмник
    
    // Частоты (Гц)
    float F[2];                // [400000, 2000000]
    
    // Воздушные значения (калибровка)
    int32_t Air_zz[2][5];      // Воздушная фаза [400/2000][Tx]
    int32_t Air_zz_amt[2][5];  // Порог воздуха по амплитуде
};
```

### 6.2 Проверка condition (функция get_condition)

Метрология определяет, какие передатчики считаются "в воздухе":

```cpp
uint16_t get_condition(struct GP_DATA *Data, struct METROLOGY_GP *metro, 
                       uint16_t freq, float mV) {
    uint16_t condition = 0;
    float Tx_amp_val;
    
    // Tx1 (измеряется на приёмнике 2)
    Tx_amp_val = Data->AM_RX_2[freq][T1] * mV;
    if (Tx_amp_val > metro->Air_zz_amt[freq][T1])
        condition |= (1 << (3 - T1));        // Бит 3
    
    // Tx2 (измеряется на приёмнике 1)
    Tx_amp_val = Data->AM_RX_1[freq][T2] * mV;
    if (Tx_amp_val > metro->Air_zz_amt[freq][T2])
        condition |= (1 << (3 - T2));        // Бит 2
    
    // Tx3 (измеряется на приёмнике 2)
    Tx_amp_val = Data->AM_RX_2[freq][T3] * mV;
    if (Tx_amp_val > metro->Air_zz_amt[freq][T3])
        condition |= (1 << (3 - T3));        // Бит 1
    
    // Tx4 (измеряется на приёмнике 1)
    Tx_amp_val = Data->AM_RX_1[freq][T4] * mV;
    if (Tx_amp_val > metro->Air_zz_amt[freq][T4])
        condition |= (1 << (3 - T4));        // Бит 0
    
    return condition;
}
```

**Логика:** Если амплитуда сигнала от передатчика превышает порог `Air_zz_amt`, передатчик считается "в воздухе" (не в породе).

### 6.3 Вычисление теоретической фазы (SIGNAL)

Метрологические параметры L1, L2 и F используются для расчёта теоретической фазы:

```cpp
float SIGNAL(struct METROLOGY_GP *metrology, uint16_t n, uint16_t freq, float ro) {
    // Геометрические параметры в метрах
    float L1 = metrology->L1[n] / 1000.0;
    float L2 = metrology->L2[n] / 1000.0;
    
    // Угловая частота × магнитная проницаемость × проводимость
    float omegamu0sigma = (0.0078957 * metrology->F[freq]) / ro;
    
    // Волновое число
    complex<float> ik = 1if * sqrt(1if * omegamu0sigma);
    
    // Теоретический сигнал
    complex<float> SGN = exp(ik*(L2 - L1)) * ((1.0f - ik*L2) / (1.0f - ik*L1));
    
    return arg(SGN);  // Возвращает фазу в радианах
}
```

### 6.4 Расчёт сопротивления по фазе (RO_ARG)

Используется метод золотого сечения для подбора Ro, при котором теоретическая фаза совпадает с измеренной:

```cpp
int32_t RO_ARG(struct METROLOGY_GP *metrology, struct GP_DATA *Data, 
               uint16_t n, uint16_t freq) {
    const float epsilon_ARG = 0.0000005;    // Точность по фазе
    float ro_0 = 0.01;                      // Минимальное Ro
    float ro_max = 7000.0;                  // Максимальное Ro
    
    do {
        // Точки деления (метод золотого сечения)
        float X1 = ro_0 + 0.382*(ro_max - ro_0);
        float X2 = ro_max - 0.382*(ro_max - ro_0);
        
        // Разность между измеренной и теоретической фазой
        float A = Data->phase_smt[freq][n] - SIGNAL(metrology, n, freq, X1);
        float B = Data->phase_smt[freq][n] - SIGNAL(metrology, n, freq, X2);
        
        // Сужение интервала
        if (fabs(A) > fabs(B)) ro_0 = X1;
        else ro_max = X2;
        
    } while (fabs(A) > epsilon_ARG && fabs(B) > epsilon_ARG);
    
    Data->rho_smt[freq][n] = (ro_0 + ro_max) / 2.0;
    return 0;
}
```

---

## 7. Обработка по частотам

### 7.1 Двухчастотная структура массивов

Все измеренные параметры хранятся как `[частота][передатчик]`:

| Индекс | Частота | Назначение |
|--------|---------|------------|
| [0][n] | 400 кГц | Глубинное зондирование |
| [1][n] | 2000 кГц | Поверхностное зондирование |

### 7.2 Независимая обработка

```cpp
for (int freq = 0; freq < 2; freq++) {          // Обе частоты
    for (int Tx = 0; Tx < N_Tx; Tx++) {         // Все передатчики
        // Симметризация выполняется независимо для каждой частоты
        Data->phase_smt[freq][Tx] = ...;
        Data->DELTA_PH[freq][Tx] = ...;
    }
}
```

### 7.3 Калибровка нулевых значений

Нулевые значения (воздушная калибровка) применяются одинаково для всех передатчиков данной частоты:

```cpp
// Вычитание нулевой фазы для каждой частоты
for (int freq = 0; freq < 2; freq++) {
    for (int Tx = 0; Tx < 5; Tx++) {
        Data->DELTA_PH[freq][Tx] -= Data->ZERO_dPH[freq];
    }
}
```

---

## 8. Последовательность обработки данных

### 8.1 Поток для AUTONOM_5Tx

```
1. Чтение файла .biv или .DEV
   ├─ .biv: прямое чтение 240 байт
   └─ .DEV: чтение 251 байт (11 байт преамбула + 240 данных)

2. Преобразование в GP_DATA (transform_data)
   ├─ Прямое копирование phase_smt
   ├─ Прямое копирование DELTA_PH
   └─ Проверка нормализации фаз

3. Определение condition (get_condition)
   ├─ Сравнение AM_RX_1/2 с Air_zz_amt из метрологии
   └─ Формирование битовой маски активных Tx

4. Симметризация (simmetry)
   ├─ Выбор матрицы K[4][4] по condition
   ├─ Матричная коррекция phase_smt
   └─ Логарифмическая коррекция rho_smt

5. Расчёт сопротивления (RO_ARG, опционально)
   ├─ Метод золотого сечения
   ├─ Сравнение с теоретической SIGNAL()
   └─ Обновление rho_smt

6. Визуализация в UI
```

### 8.2 Поток для LWD_4Tx

```
1. Чтение файла .biv или .DEV
   ├─ .biv: sizeof(_4Tx_DATA) = ~180 байт
   └─ .DEV: +11 байт преамбула

2. Преобразование в GP_DATA
   ├─ Копирование PH_RX_1 и PH_RX_2
   ├─ Вычисление DELTA_PH = PH_RX_2 - PH_RX_1
   └─ Нормализация в [-180°, +180°]

3-6. Аналогично AUTONOM_5Tx (но только для Tx1-Tx4)
```

---

## 9. Ключевые константы и коэффициенты

### 9.1 Физические константы
```cpp
const float PI = 3.141592741f;
const float mV = 0.00023842;        // Коэффициент перевода АЦП в Вольты
const float mG = 1000.0f * 180.0f / PI;  // Перевод радиан в миллиградусы
const float Grad = 180.0f / PI;     // Перевод радиан в градусы
```

### 9.2 Параметры точности
```cpp
const float epsilon_ARG = 0.0000005;      // Точность фазы (рад)
const float epsilon_INF = 0.000000005;    // Точность сопротивления
const float Ro_0 = 0.01;                  // Минимальное Ro
const float Ro_max = 7000.0;              // Максимальное Ro
```

### 9.3 Коэффициенты метода золотого сечения
```cpp
float X1 = ro_0 + 0.382*(ro_max - ro_0);  // Левая точка
float X2 = ro_max - 0.382*(ro_max - ro_0); // Правая точка
```

---

## 10. Примеры использования

### 10.1 Чтение и обработка одного кадра

```cpp
// Инициализация параметров файла
DATA_FILE_PARAM param;
param.fin = &filestream;
param.Data_file_name = "data.DEV";
param.extension = ".DEV";
param.sonde_type = AUTONOM_5Tx_;

// Определение типа и размера
define_sonde_type(&param);

// Чтение кадра №10
variant<_4Tx_DATA, _5Tx_DATA, ALL_DATA> sonde_data = read_blok(&param, 10);

// Преобразование в GP_DATA
ALL_DATA all_data;
GP_DATA gp_data = transform_data(sonde_data, &all_data);

// Загрузка метрологии
METROLOGY_GP metro = metrology_GP_struct;

// Обработка для 400 кГц
uint16_t condition = get_condition(&gp_data, &metro, 0, mV);
simmetry(&gp_data, &metro, 0, condition);

// Обработка для 2000 кГц
condition = get_condition(&gp_data, &metro, 1, mV);
simmetry(&gp_data, &metro, 1, condition);

// Результат доступен в gp_data.phase_smt[2][5]
```

### 10.2 Проверка нормализации фазы

```cpp
// Пример нормализации разности фаз
float signal = PH_RX_2 - PH_RX_1;  // Может быть -400°...+400°

if (fabs(signal) < 40) {            // Проверка на адекватность
    while (signal >= PI)            // +190° → -170°
        signal -= 2 * PI;
    while (signal <= -PI)           // -190° → +170°
        signal += 2 * PI;
} else {
    signal = 0.0;                   // Отбраковка выброса
}

// Результат: signal в диапазоне [-180°, +180°]
```

---

## 11. Особенности и ограничения

### 11.1 Только для LWD_4Tx и AUTONOM_5Tx
- Документация не применима к картографическим зондам
- Картограф использует `ALL_DATA` с сырыми секторными данными

### 11.2 Форматы файлов
- `.biv`: Чистые бинарные данные без заголовков
- `.DEV`: Данные с преамбулой 11 байт (протокол устройства)

### 11.3 Ограничения симметризации
- Матрица 4×4 рассчитана только для 4 передатчиков
- Для AUTONOM_5Tx пятый передатчик (Tx5) обрабатывается отдельно
- Симметризация не применяется, если активен только 1 передатчик

### 11.4 Требования к метрологии
- `signature` в метрологии должен совпадать с `GP_DATA.signature`
- Параметры L1, L2 влияют на точность расчёта `RO_ARG`
- `Air_zz_amt` определяет порог "воздух/порода"

---

## 12. Заключение

Система симметризации фаз в `GP_DATA` обеспечивает:

1. **Коррекцию взаимного влияния** передатчиков через матричный метод
2. **Двухчастотность** — независимую обработку 400 и 2000 кГц
3. **Калибровку** — учёт нулевых значений и метрологических параметров
4. **Устойчивость** — фильтрацию выбросов и нормализацию фаз
5. **Гибкость** — различные матрицы для разных комбинаций активных передатчиков

Метрология является **обязательным** компонентом для:
- Определения condition (воздух/порода)
- Расчёта теоретической фазы (функция SIGNAL)
- Определения удельного сопротивления (функция RO_ARG)

---

*Документация составлена на основе анализа исходного кода variable.cpp и variable.h*
*Версия документа: 1.0*
*Дата создания: 08.06.2026*
