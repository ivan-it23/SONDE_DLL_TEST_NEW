# Логика отображения симметризованных фаз (`PHASE_SMT`)

Версия приложения: **2.22.0**  
Ключевые файлы: `CurveSelectionModule.cpp`, `SchematicModule.cpp/.h`, `ToolDataTypes.h`, `SerialModule.h`

---

## 1. Два режима источника данных

Для пресетов **«Симм. фазы»** и **«УЭС»** приложение поддерживает два независимых источника
вычисленных значений. Режим выбирается пользователем через checkbox в модальном окне схемы прибора.

```cpp
// SchematicModule.h
enum class SmtComputationMode {
    FROM_SONDE,    // значения берутся из поля GP_TOOL_DATA.phase_smt (рассчитаны на приборе)
    FROM_COMPUTER  // значения вычисляются в приложении по DELTA_PH + метрологии
};
```

По умолчанию оба пресета инициализируются в `FROM_SONDE`:

```cpp
// SchematicModule.cpp — SchematicSelection::SchematicSelection()
rhoSmtMode   = SmtComputationMode::FROM_SONDE;
phaseSmtMode = SmtComputationMode::FROM_SONDE;
```

---

## 2. UI: пресеты и чекбоксы в модальном окне

Два пресета управляются через единую таблицу привязок `SmtUiBinding`:

| Пресет | Кнопка | Checkbox «на приборе» | Checkbox «на компьютере» | Поле режима |
|---|---|---|---|---|
| УЭС | `IDC_PRESET_RHO_SMT` | `IDC_RHO_SMT_FROM_SONDE` | `IDC_RHO_SMT_FROM_COMPUTER` | `rhoSmtMode` |
| Симм. фазы | `IDC_PRESET_PHASE_SMT` | `IDC_PHASE_SMT_FROM_SONDE` | `IDC_PHASE_SMT_FROM_COMPUTER` | `phaseSmtMode` |

### Название пресета в легенде

Название автоматически формируется с суффиксом режима:

```cpp
// SchematicModule.cpp
static std::wstring BuildSmtPresetDisplayName(const wchar_t* baseName, SmtComputationMode mode) {
    return std::wstring(baseName) +
           (mode == SmtComputationMode::FROM_COMPUTER ? L" (на компьютере)" : L" (на приборе)");
}
```

Примеры: **«Симм. фазы (на приборе)»**, **«Симм. фазы (на компьютере)»**.

### Защита от отсутствия метрологии

При попытке переключить checkbox в `FROM_COMPUTER` без загруженной метрологии:

```cpp
// SchematicModule.cpp — HandleSmtModeCheckbox()
if (mode == SmtComputationMode::FROM_COMPUTER &&
    metrologyModule && !metrologyModule->HasData()) {
    MessageBox(hDlg,
        L"Метрология не была считана.\n...",
        L"Метрология недоступна", MB_OK | MB_ICONWARNING);
    // Автоматически откатывается в FROM_SONDE:
    ApplySmtModeCheckboxSelection(hDlg, *binding, currentSelection,
                                  SmtComputationMode::FROM_SONDE);
    return;
}
```

---

## 3. Режим `FROM_SONDE` — чтение готового поля

Приложение просто читает значение из поля `GP_TOOL_DATA.phase_smt`, которое было заполнено
прибором или внешней DLL ещё до записи в файл:

```cpp
// ToolDataAccess.h — TryGetPhaseSmt()
bool TryGetPhaseSmt(int freqIdx, int transmitterIndex, float& outValue) const {
    if (!record || !SupportsTransmitter(transmitterIndex)
        || !capabilities.hasSondePhaseSmt
        || freqIdx < 0 || freqIdx > 1) {
        return false;
    }
    outValue = record->phase_smt[freqIdx][transmitterIndex];
    return true;
}
```

После чтения значение нормализуется и переводится в миллиградусы для графика:

```cpp
// DataModule.cpp — CalculatePhaseData()
phase = NormalizePhase(phase);           // нормализация в (-π, π]
value = phase * 57295.7795f;             // радианы → миллиградусы
```

---

## 4. Режим `FROM_COMPUTER` — вычисление в приложении

Реализован в `CurveSelectionModule.cpp`, функция `ComputePhaseSmtFromComputer()`.
Воспроизводит логику симметризации прибора (`formula_simmetry/simmetry`).

### 4.1. Условие активации

Вычисление на компьютере применяется только если:
1. Режим кривой — `SmtComputationMode::FROM_COMPUTER`
2. Метрология загружена (`metrologyModule->HasData() == true`)
3. Сигнатура кадра данных совпадает с сигнатурой метрологии:

```cpp
// CurveSelectionModule.cpp
bool ShouldUseLegacyComputedSmt(const MeasurementView& measurement,
                                const METROLOGY_DATA* metro) {
    return metro && measurement.IsValid()
        && measurement.GetSignature() == metro->signature;
}
```

Если любое из условий не выполнено — автоматически используется `TryGetPhaseSmt()` (т.е.
поведение эквивалентно `FROM_SONDE`).

### 4.2. Алгоритм `ComputePhaseSmtFromComputer()`

#### Шаг 1. Извлечь маску рабочих передатчиков (`conditionByte`)

Поле `GP_TOOL_DATA::condition` — 32-битное число. Каждый байт кодирует маску рабочих
передатчиков для своей частоты. Внутри байта: `bit0 = T1, bit1 = T2, ..., bit4 = T5`.

```
Байт 0 (биты 7..0)  — 2000 кГц (freqIdx = 1)
Байт 1 (биты 15..8) — 400 кГц  (freqIdx = 0)
```

```cpp
uint8_t ExtractConditionByte(uint32_t rawCondition, int freqIdx) {
    if (freqIdx == 0) return static_cast<uint8_t>((rawCondition >> 8) & 0xFF); // 400 кГц
    return static_cast<uint8_t>(rawCondition & 0xFF);                           // 2000 кГц
}
```

#### Шаг 2. Нормализация `DELTA_PH` к воздушному нулю

Для каждого передатчика `t` из `GP_TOOL_DATA::DELTA_PH[freqIdx][t]` вычитается воздушный
ноль `Air_zz[freqIdx][t]` из метрологии, и применяется чередование знаков:

```
T1: sign = +1
T2: sign = −1
T3: sign = +1
T4: sign = −1
T5: sign = +1
```

```cpp
const float airOffset = metrology.Air_zz[freqIdx][t] / 57297.0f;  // int16 → радианы
deltas[t] = kAlternatingSigns[t] * (rawDelta - airOffset);
```

Делитель `57297.0f` — legacy-константа перевода из единиц хранения `Air_zz` (deg×100) в радианы.

#### Шаг 3. Выбор матрицы симметризации K

Матрица `K` размером 5×5 выбирается диспетчером `BuildSymmetryMatrix()` по паре
`(transmitterCount, conditionByte)`. Строки матрицы — выходные каналы (T1..T5),
столбцы — входные `deltas[j]`. Применение: `phase_smt[t] = sum_j K[t][j] * deltas[j]`.

Для нераспознанной комбинации `conditionByte` используется **identity по маске** —
`FillIdentitySymmetryMatrix()`: рабочий передатчик отображается на самого себя (K[t][t]=1),
нерабочий даёт ноль. Это гарантирует, что граф никогда не упадёт.

Специальный случай `condition = 0b00000000` — прибор передаёт **несимметризованные**
значения. Матрица заменяется единичной (диагональ = 1.0f), данные выводятся как есть.

---

### 3.1. Прибор 5Tx (N\_Tx = 5)

Столбцы входа: `d[T1] d[T2] d[T3] d[T4] d[T5]`

**condition = `0b00011111` — все 5 работают** *(также `0b11111111`)*

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.75  +0.50  -0.25  +0.00  +0.00 ]
ph[T2] [ +0.25  +0.50  +0.25  +0.00  +0.00 ]
ph[T3] [ +0.00  +0.25  +0.50  +0.25  +0.00 ]
ph[T4] [ +0.00  +0.00  +0.25  +0.50  +0.25 ]
ph[T5] [ +0.00  +0.00  -0.25  +0.50  +0.75 ]
```

Матрица симметрична относительно диагонали «T1↔T5», веса убывают с удалением от
соседних передатчиков. T3 (центральный) даёт взвешенное среднее ближайших пар.

**condition = `0b00011110` — не работает T1**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.00  +1.25  +0.50  -0.75  +0.00 ]
ph[T2] [ +0.00  +0.75  +0.50  -0.25  +0.00 ]
ph[T3] [ +0.00  +0.25  +0.50  +0.25  +0.00 ]
ph[T4] [ +0.00  +0.00  +0.25  +0.50  +0.25 ]
ph[T5] [ +0.00  +0.00  -0.25  +0.50  +0.75 ]
```

Первый столбец `d[T1]` занулён. Выход `ph[T1]` восстанавливается экстраполяцией
из T2, T3, T4 с повышенным весом T2 (`+1.25`).

**condition = `0b00011101` — не работает T2**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +1.25  +0.00  -0.75  +0.50  +0.00 ]
ph[T2] [ +0.75  +0.00  -0.25  +0.50  +0.00 ]
ph[T3] [ +0.00  +0.00  +0.75  +0.50  -0.25 ]
ph[T4] [ +0.00  +0.00  +0.25  +0.50  +0.25 ]
ph[T5] [ +0.00  +0.00  -0.25  +0.50  +0.75 ]
```

Столбец `d[T2]` занулён. Выход `ph[T2]` восстанавливается из T1, T3, T4.

**condition = `0b00011011` — не работает T3**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.50  +0.75  +0.00  -0.25  +0.00 ]
ph[T2] [ +0.00  +1.25  +0.00  -0.75  +0.50 ]
ph[T3] [ +0.00  +0.75  +0.00  -0.25  +0.50 ]
ph[T4] [ +0.00  +0.25  +0.00  +0.25  +0.05 ]
ph[T5] [ +0.00  -0.25  +0.00  +0.75  +0.05 ]
```

Столбец `d[T3]` занулён. Центральный выход `ph[T3]` восстанавливается из T2 и T4.
Малые коэффициенты `+0.05` для ph[T4] и ph[T5] — артефакт исходной матрицы.

**condition = `0b00010111` — не работает T4**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.75  +0.50  -0.25  +0.00  +0.00 ]
ph[T2] [ +0.25  +0.50  +0.25  +0.00  +0.00 ]
ph[T3] [ -0.25  +0.50  +0.75  +0.00  +0.00 ]
ph[T4] [ +0.00  +0.50  -0.25  +0.00  +0.75 ]
ph[T5] [ +0.00  +0.50  -0.75  +0.00  +1.25 ]
```

Столбец `d[T4]` занулён. Выход `ph[T4]` восстанавливается из T2, T3, T5.

**condition = `0b00001111` — не работает T5**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.75  +0.50  -0.25  +0.00  +0.00 ]
ph[T2] [ +0.25  +0.50  +0.25  +0.00  +0.00 ]
ph[T3] [ +0.05  +0.25  +0.50  +0.25  +0.00 ]
ph[T4] [ +0.00  -0.25  +0.50  +0.75  +0.00 ]
ph[T5] [ +0.00  -0.75  +0.50  +1.25  +0.00 ]
```

Столбец `d[T5]` занулён. Выход `ph[T5]` восстанавливается из T2, T3, T4 с повышенным
весом T4 (`+1.25`). Малый коэффициент `+0.05` для ph[T3] — артефакт исходной матрицы.

**condition = `0b00000000` — несимметризованные (identity 5×5)**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +1.00  +0.00  +0.00  +0.00  +0.00 ]
ph[T2] [ +0.00  +1.00  +0.00  +0.00  +0.00 ]
ph[T3] [ +0.00  +0.00  +1.00  +0.00  +0.00 ]
ph[T4] [ +0.00  +0.00  +0.00  +1.00  +0.00 ]
ph[T5] [ +0.00  +0.00  +0.00  +0.00  +1.00 ]
```

---

### 3.2. Прибор 4Tx (N\_Tx = 4)

T5 физически отсутствует. Строка и столбец T5 в матрице всегда нулевые.

**condition = `0b00001111` — все 4 работают** *(также `0b11111111`)*

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.75  +0.50  -0.25  +0.00  +0.00 ]
ph[T2] [ +0.25  +0.50  +0.25  +0.00  +0.00 ]
ph[T3] [ +0.00  +0.25  +0.50  +0.25  +0.00 ]
ph[T4] [ +0.00  -0.25  +0.50  +0.75  +0.00 ]
ph[T5] [ +0.00  +0.00  +0.00  +0.00  +0.00 ]
```

Структура коэффициентов T1–T4 аналогична 5Tx при всех рабочих, но T5 всегда равен нулю.
Обратите внимание: строка T2 и T3 симметрична, T4 — зеркальная к T1.

**condition = `0b00001110` — не работает T1**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.00  +1.25  +0.50  -0.75  +0.00 ]
ph[T2] [ +0.00  +0.75  +0.50  -0.25  +0.00 ]
ph[T3] [ +0.00  +0.25  +0.50  +0.25  +0.00 ]
ph[T4] [ +0.00  -0.25  +0.50  +0.75  +0.00 ]
ph[T5] [ +0.00  +0.00  +0.00  +0.00  +0.00 ]
```

**condition = `0b00001101` — не работает T2**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +1.25  +0.00  -0.75  +0.50  +0.00 ]
ph[T2] [ +0.75  +0.00  -0.25  +0.50  +0.00 ]
ph[T3] [ +0.25  +0.00  +0.25  +0.50  +0.00 ]
ph[T4] [ -0.25  +0.00  +0.75  +0.50  +0.00 ]
ph[T5] [ +0.00  +0.00  +0.00  +0.00  +0.00 ]
```

**condition = `0b00001011` — не работает T3**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.50  +0.75  +0.00  -0.25  +0.00 ]
ph[T2] [ +0.50  +0.25  +0.00  +0.25  +0.00 ]
ph[T3] [ +0.50  -0.25  +0.00  +0.75  +0.00 ]
ph[T4] [ -0.50  -0.75  +0.00  +1.25  +0.00 ]
ph[T5] [ +0.00  +0.00  +0.00  +0.00  +0.00 ]
```

**condition = `0b00000111` — не работает T4**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.75  +0.50  -0.25  +0.00  +0.00 ]
ph[T2] [ +0.25  +0.50  +0.25  +0.00  +0.00 ]
ph[T3] [ -0.25  +0.50  +0.75  +0.00  +0.00 ]
ph[T4] [ -0.75  +0.50  +1.25  +0.00  +0.00 ]
ph[T5] [ +0.00  +0.00  +0.00  +0.00  +0.00 ]
```

**condition = `0b00000000` — несимметризованные (identity 4×4)**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +1.00  +0.00  +0.00  +0.00  +0.00 ]
ph[T2] [ +0.00  +1.00  +0.00  +0.00  +0.00 ]
ph[T3] [ +0.00  +0.00  +1.00  +0.00  +0.00 ]
ph[T4] [ +0.00  +0.00  +0.00  +1.00  +0.00 ]
ph[T5] [ +0.00  +0.00  +0.00  +0.00  +0.00 ]
```

---

### 3.3. Прибор 3Tx (N\_Tx = 3)

T4 и T5 физически отсутствуют. Столбцы и строки T4, T5 всегда нулевые.
Для 3Tx в коде определены только два случая `condition`.

**condition = `0b00000111` — все 3 работают**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +0.75  +0.50  -0.25  +0.00  +0.00 ]
ph[T2] [ +0.25  +0.50  +0.25  +0.00  +0.00 ]
ph[T3] [ -0.25  +0.50  +0.75  +0.00  +0.00 ]
ph[T4] [ -0.75  +0.50  +1.25  +0.00  +0.00 ]
ph[T5] [ +0.00  +0.00  +0.00  +0.00  +0.00 ]
```

> Строка `ph[T4]` здесь ненулевая — это теоретический «экстраполированный» виртуальный
> выход T4 через матрицу, однако в практическом расчёте он отбрасывается, так как
> `transmitterCount = 3` и цикл применения матрицы идёт только до `t < 3`.

**condition = `0b00000000` — несимметризованные (identity 3×3)**

```
         d[T1]  d[T2]  d[T3]  d[T4]  d[T5]
ph[T1] [ +1.00  +0.00  +0.00  +0.00  +0.00 ]
ph[T2] [ +0.00  +1.00  +0.00  +0.00  +0.00 ]
ph[T3] [ +0.00  +0.00  +1.00  +0.00  +0.00 ]
ph[T4] [ +0.00  +0.00  +0.00  +0.00  +0.00 ]
ph[T5] [ +0.00  +0.00  +0.00  +0.00  +0.00 ]
```

---

### 3.4. Fallback — нераспознанная комбинация `condition`

Для любого `conditionByte`, которого нет в таблицах выше (например, два нерабочих
передатчика одновременно), вызывается `FillIdentitySymmetryMatrix()`:

```cpp
for (int t = 0; t < transmitterCount; ++t) {
    const bool working = ((condition >> t) & 0x1u) != 0;
    K[t][t] = working ? 1.0f : 0.0f;  // рабочий → себя, нерабочий → 0
}
```

Каждый рабочий передатчик отображается на себя без вклада от соседей. Нерабочие
дают нулевое значение. Симметризации не происходит, но данные не теряются и не
вызывают ошибку.

#### Шаг 4. Применить матрицу

```cpp
for (int t = 0; t < transmitterCount; ++t) {
    float sum = 0.0f;
    for (int j = 0; j < kSmtMaxTransmitterCount; ++j) {
        sum += K[t][j] * deltas[j];
    }
    outPhase[t] = sum;
}
```

Результат `outPhase[t]` — симметризованная фаза в радианах для передатчика `t`.
Затем нормализуется и переводится в миллиградусы аналогично режиму `FROM_SONDE`.

---

## 5. Быстрый путь (batched rendering)

Для пресетов-таблиц с большим числом кривых приложение использует оптимизированный
batch-рендеринг через `DataModule::CalculateChartData()`. Однако batch-путь доступен
**только для `FROM_SONDE`**:

```cpp
// CurveSelectionModule.cpp — TryBuildBatchedPresetCurveData()
if ((batchedDataType == DataType::PHASE_SMT || batchedDataType == DataType::RHO_SMT) &&
    curve.smtMode != SmtComputationMode::FROM_SONDE) {
    return false;  // batch-путь недоступен, переходим к поштучному расчёту
}
```

В режиме `FROM_COMPUTER` каждый кадр и каждый передатчик обрабатываются индивидуально
через `CalculatePhaseSmtData()` → `TryGetSelectedPhaseSmt()` → `TryGetLegacyComputedPhaseSmt()`.

---

## 6. Режим `FROM_COMPUTER` для УЭС (`RHO_SMT`)

УЭС в режиме `FROM_COMPUTER` вычисляется через инверсию симметризованной фазы:

```cpp
// CurveSelectionModule.cpp — TryGetSelectedRhoSmt()
float phase = 0.0f;
TryGetLegacyComputedPhaseSmt(measurement, *metro, freqIdx, transmitterIndex, phase);
outRho = InvertPhaseToRho(*metro, phase, transmitterIndex, freqIdx);
```

`InvertPhaseToRho()` использует расстояния `L1[txIdx]`, `L2[txIdx]` из метрологии и
решает обратную задачу: фаза (рад) → удельное электрическое сопротивление (Ом·м).

---

## 7. Полная блок-схема принятия решений

```
Пользователь нажимает кнопку пресета «Симм. фазы» в модальном окне
    │
    ├─ Checkbox «на приборе» (FROM_SONDE)
    │       │
    │       └─ curve.smtMode = FROM_SONDE для всех кривых пресета
    │               │
    │               ├─ [batch-путь] DataModule::CalculateChartData()
    │               │       → TryGetPhaseSmt() → GP_TOOL_DATA.phase_smt[f][t]
    │               │
    │               └─ [поштучный путь] TryGetSelectedPhaseSmt()
    │                       → ShouldUseLegacyComputedSmt() == false
    │                       → TryGetPhaseSmt() → GP_TOOL_DATA.phase_smt[f][t]
    │
    └─ Checkbox «на компьютере» (FROM_COMPUTER)
            │
            ├─ [нет метрологии] → предупреждение, откат в FROM_SONDE
            │
            └─ curve.smtMode = FROM_COMPUTER для всех кривых пресета
                    │
                    └─ [поштучный путь] TryGetSelectedPhaseSmt()
                            │
                            ├─ ShouldUseLegacyComputedSmt()
                            │   signature кадра == signature метрологии? ──нет──→ FROM_SONDE
                            │
                            └─ да → ComputePhaseSmtFromComputer()
                                    │
                                    1. ExtractConditionByte(condition, freqIdx)
                                    2. deltas[t] = sign[t] * (DELTA_PH[f][t] - Air_zz[f][t]/57297)
                                    3. BuildSymmetryMatrix(K, transmitterCount, conditionByte)
                                    4. phase[t] = sum_j K[t][j] * deltas[j]
                                    │
                                    └─ NormalizePhase() → × 57295.7795 → миллиградусы на графике
```

---

## 8. Ключевые константы

| Константа | Значение | Смысл |
|---|---|---|
| `kMdegPerRad` | `57295.7795f` | Перевод радиан в миллиградусы |
| `kAirZzPhaseDivisor` | `57297.0f` | Нормализация `Air_zz` (int16, единицы deg×100) в радианы |
| `kSmtMaxTransmitterCount` | `5` | Максимальный размер матрицы симметризации |

---

## 9. Ключевые файлы исходного кода

| Файл | Роль в логике |
|---|---|
| `SchematicModule.h` | `SmtComputationMode`, `SmtUiBinding`, `SchematicSelection` — объявления |
| `SchematicModule.cpp` | Обработка UI-событий (`HandlePresetButton`, `HandleSmtModeCheckbox`), защита от отсутствия метрологии |
| `CurveSelectionModule.cpp` | Всё вычисление `FROM_COMPUTER`: матрицы, `ComputePhaseSmtFromComputer`, `InvertPhaseToRho` |
| `ToolDataAccess.h` | `TryGetPhaseSmt()` — чтение `phase_smt` из структуры (режим `FROM_SONDE`) |
| `SerialModule.h` | Структура `METROLOGY_DATA` с полями `Air_zz`, `L1`, `L2` |
| `DataModule.cpp` | `CalculatePhaseData()`, `NormalizePhase()`, batch-рендеринг для `FROM_SONDE` |
