#define _XOPEN_SOURCE

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "adc.h"
#include "key.h"
#include "oled.h"
#include "rtc.h"
#include "watch.h"

#define RTC_INIT_FLAG 0x1024

#define BATTERY_W 16
#define BATTERY_H 8
#define BATTERY_X (COL_NUM - BATTERY_W - 1)
#define BATTERY_Y 0

#define WEEK_MODIFY_LINEW 1
#define WEEK_MODIFY_LINE_SPACING 1
#define WEEK_X week_x
#define WEEK_Y 0
#define WEEK_SPACING 0

// 日期之间的分隔符
#define DATA_SEP_W 6
#define DATA_SEP_H 1
#define DATA_MODIFY_LINEW 1
#define DATA_MODIFY_LINE_SPACING 1
#define DATA_NUM_W FONT_W
#define DATA_NUM_H FONT_H
#define DATA_NUM_LINEW 1
#define DATA_NUM_SPACING 2
#define DATA_SPACING 10
// 居中
#define DATA_X                                                                 \
    ((COL_NUM - (DATA_NUM_W * 8 + DATA_NUM_SPACING * 5 + DATA_SPACING * 2)) / 2)
#define DATA_Y                                                                 \
    (ROW_NUM - DATA_NUM_H - DATA_MODIFY_LINEW - DATA_MODIFY_LINE_SPACING)

// 时间之间的':'
#define TIME_DOT_W 3
#define TIME_DOT_H TIME_DOT_W
#define TIME_MODIFY_LINEW 2
#define TIME_MODIFY_LINE_SPACING 2
#define TIME_NUM_W 16
#define TIME_NUM_H 25
#define TIME_NUM_LINEW 3
#define TIME_NUM_SPACING 4 // 某一个时间量中的数字的间距, 比如秒的两个数字的间距
#define TIME_SPACING 8     // 相邻时间量的数字的间距, 比如秒和分之间的间距
// 居中
#define TIME_X                                                                 \
    ((COL_NUM - (TIME_NUM_W * 6 + TIME_NUM_SPACING * 3 + TIME_SPACING * 2)) / 2)
// 放在week和data之间
#define TIME_Y (FONT_H + (ROW_NUM - FONT_H - DATA_NUM_H - TIME_NUM_H) / 2)

typedef enum {
    WATCH_START,
    MODIFY_HOUR,
    MODIFY_MIN,
    MODIFY_SEC,
    MODIFY_YEAR,
    MODIFY_MON,
    MODIFY_DAY
} WatchState;

WatchState curstate = WATCH_START;
struct tm curtime;
uint8_t format_24hour = 1;
uint8_t charging = 0;
struct {
    uint8_t x, y, w, h;
    uint8_t flag;
} modify_line[MODIFY_DAY + 2] = {[MODIFY_DAY + 1] = {.flag = 2}};
struct {
    WatchState next_state;
    WatchState pre_state;
    int *modify_value;
} modify_action[] = {
    [MODIFY_HOUR] = {MODIFY_MIN, WATCH_START, &curtime.tm_hour},
    [MODIFY_MIN] = {MODIFY_SEC, MODIFY_HOUR, &curtime.tm_min},
    [MODIFY_SEC] = {MODIFY_YEAR, MODIFY_MIN, &curtime.tm_sec},
    [MODIFY_YEAR] = {MODIFY_MON, MODIFY_SEC, &curtime.tm_year},
    [MODIFY_MON] = {MODIFY_DAY, MODIFY_YEAR, &curtime.tm_mon},
    [MODIFY_DAY] = {WATCH_START, MODIFY_MON, &curtime.tm_mday},
};

void show_battery_level()
{
    static uint8_t empty_flag = 0;

    uint32_t value = (BATTERY_W - 2) * HAL_ADC_GetValue(&hadc1) / 0xfff;
    for (uint8_t i = 0; i < BATTERY_W; i++)
    {
        for (uint8_t j = 0; j < BATTERY_H; j++)
        {
            uint8_t state = 0;
            if (i == BATTERY_W - 1 && j != 0 && j != BATTERY_H - 1) // 右侧凸起
                oled_setpixel(BATTERY_X + i + 1, BATTERY_Y + j, 1);
            if (i <= value) // 填充
                state = 1;
            if (i == 0 || i == BATTERY_W - 1 || j == 0
                || j == BATTERY_H - 1) // 边框
            {
                state = 1;
                if (charging == 1)
                {
                    if (i % 2 == empty_flag && j == 0)
                        state = 0;
                    else if (i % 2 == 1 - empty_flag && j == BATTERY_H - 1)
                        state = 0;
                    else if (j % 2 == empty_flag && i == 0)
                        state = 0;
                    else if (j % 2 == 1 - empty_flag && i == BATTERY_W - 1)
                        state = 0;
                }
            }
            oled_setpixel(BATTERY_X + i, BATTERY_Y + j, state);
        }
    }
    if (charging == 1) empty_flag = 1 - empty_flag;
}

static uint32_t RTC_ReadTimeCounter(RTC_HandleTypeDef *hrtc)
{
    uint16_t high1 = 0U, high2 = 0U, low = 0U;
    uint32_t timecounter = 0U;

    high1 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);
    low = READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT);
    high2 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);

    if (high1 != high2)
    {
        /* In this case the counter roll over during reading of CNTL and CNTH
           registers, read again CNTL register then return the counter value */
        timecounter = (((uint32_t)high2 << 16U)
                       | READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT));
    }
    else
    {
        /* No counter roll over during reading of CNTL and CNTH registers,
           counter value is equal to first value of CNTL and CNTH */
        timecounter = (((uint32_t)high1 << 16U) | low);
    }

    return timecounter;
}

static HAL_StatusTypeDef RTC_EnterInitMode(RTC_HandleTypeDef *hrtc)
{
    uint32_t tickstart = 0U;

    tickstart = HAL_GetTick();
    /* Wait till RTC is in INIT state and if Time out is reached exit */
    while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
    {
        if ((HAL_GetTick() - tickstart) > RTC_TIMEOUT_VALUE)
        {
            return HAL_TIMEOUT;
        }
    }

    /* Disable the write protection for RTC registers */
    __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);

    return HAL_OK;
}

static HAL_StatusTypeDef RTC_ExitInitMode(RTC_HandleTypeDef *hrtc)
{
    uint32_t tickstart = 0U;

    /* Disable the write protection for RTC registers */
    __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);

    tickstart = HAL_GetTick();
    /* Wait till RTC is in INIT state and if Time out is reached exit */
    while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
    {
        if ((HAL_GetTick() - tickstart) > RTC_TIMEOUT_VALUE)
        {
            return HAL_TIMEOUT;
        }
    }

    return HAL_OK;
}

static HAL_StatusTypeDef RTC_WriteTimeCounter(RTC_HandleTypeDef *hrtc,
                                              uint32_t TimeCounter)
{
    HAL_StatusTypeDef status = HAL_OK;

    /* Set Initialization mode */
    if (RTC_EnterInitMode(hrtc) != HAL_OK)
    {
        status = HAL_ERROR;
    }
    else
    {
        /* Set RTC COUNTER MSB word */
        WRITE_REG(hrtc->Instance->CNTH, (TimeCounter >> 16U));
        /* Set RTC COUNTER LSB word */
        WRITE_REG(hrtc->Instance->CNTL, (TimeCounter & RTC_CNTL_RTC_CNT));

        /* Wait for synchro */
        if (RTC_ExitInitMode(hrtc) != HAL_OK)
        {
            status = HAL_ERROR;
        }
    }

    return status;
}

void update_time()
{
    time_t stamp = RTC_ReadTimeCounter(&hrtc);
    curtime = *localtime(&stamp);
}

void check_time()
{
    time_t stamp = mktime(&curtime);
    curtime = *localtime(&stamp);
}

void set_time()
{
    time_t stamp = mktime(&curtime);
    RTC_WriteTimeCounter(&hrtc, stamp);
}

void show_time()
{
    uint8_t x = TIME_X;
    for (int i = 0; i < 3; i++)
    {
        int value;
        switch (i)
        {
        case 0:
            value = curtime.tm_hour;
            if (format_24hour == 0)
            {
                if (value <= 12)
                    oled_show_string("a.m.", 0, 0, 0);
                else
                {
                    value -= 12;
                    oled_show_string("p.m.", 0, 0, 0);
                }
            };
            break;
        case 1: value = curtime.tm_min; break;
        case 2: value = curtime.tm_sec; break;
        }
        modify_line[MODIFY_HOUR + i] = (typeof(modify_line[0])){
            .x = x,
            .y = TIME_Y + TIME_NUM_H + TIME_MODIFY_LINE_SPACING,
            .w = TIME_NUM_W * 2 + TIME_NUM_SPACING,
            .h = TIME_MODIFY_LINEW,
            .flag = modify_line[MODIFY_HOUR + i].flag,
        };
        oled_show_numbers(value, x, TIME_Y, TIME_NUM_W, TIME_NUM_H,
                          TIME_NUM_LINEW, TIME_NUM_SPACING, 2);
        x += TIME_NUM_W * 2 + TIME_NUM_SPACING;

        if (i < 2)
        {
            uint8_t x_spacing =
                (TIME_SPACING - TIME_DOT_W) / 2; // 点与两边的间距
            uint8_t y_spacing =
                (TIME_NUM_H - TIME_DOT_H * 2) / 3; // 点与上下或者另一个点的间距
            for (uint8_t j = x; j < x + TIME_SPACING; j++)
            {
                for (uint8_t k = TIME_Y; k < TIME_Y + TIME_NUM_H; k++)
                {
                    if (j >= x + x_spacing && j < x + x_spacing + TIME_DOT_W
                        && ((k >= TIME_Y + y_spacing
                             && k < TIME_Y + y_spacing + TIME_DOT_H)
                            || (k >= TIME_Y + y_spacing * 2 + TIME_DOT_H
                                && k < TIME_Y + y_spacing * 2
                                           + TIME_DOT_H * 2)))
                        oled_setpixel(j, k, 1);
                    else
                        oled_setpixel(j, k, 0);
                }
            }
        }
        x += TIME_SPACING;
    }

    x = DATA_X;
    for (int i = 0; i < 3; i++)
    {
        int value, length;
        switch (i)
        {
        case 0:
            value = curtime.tm_year + 1900;
            length = 4;
            break;
        case 1:
            value = curtime.tm_mon + 1;
            length = 2;
            break;
        case 2:
            value = curtime.tm_mday;
            length = 2;
            break;
        }
        modify_line[MODIFY_YEAR + i] = (typeof(modify_line[0])){
            .x = x,
            .y = DATA_Y + DATA_NUM_H + DATA_MODIFY_LINE_SPACING,
            .w = DATA_NUM_W * length + DATA_NUM_SPACING * (length - 1),
            .h = DATA_NUM_LINEW,
            .flag = modify_line[MODIFY_YEAR + i].flag,
        };
        oled_show_numbers(value, x, DATA_Y, DATA_NUM_W, DATA_NUM_H,
                          DATA_NUM_LINEW, DATA_NUM_SPACING, length);
        x += DATA_NUM_W * length + DATA_NUM_SPACING * (length - 1);
        if (i < 2)
        {
            uint8_t x_spacing = (DATA_SPACING - DATA_SEP_W) / 2;
            uint8_t y_spacing = (DATA_NUM_H - DATA_SEP_H) / 2;
            for (uint8_t j = x; j < x + DATA_SPACING; j++)
            {
                for (uint8_t k = DATA_Y; k < DATA_Y + DATA_NUM_H; k++)
                {
                    if (j >= x + x_spacing && j < x + x_spacing + DATA_SEP_W
                        && k >= DATA_Y + y_spacing
                        && k < DATA_Y + y_spacing + DATA_SEP_H)
                        oled_setpixel(j, k, 1);
                    else
                        oled_setpixel(j, k, 0);
                }
            }
        }
        x += DATA_SPACING;
    }
    char *week_day;
    switch (curtime.tm_wday)
    {
    case 1: week_day = "Monday"; break;
    case 2: week_day = "Tuesday"; break;
    case 3: week_day = "Wednesday"; break;
    case 4: week_day = "Thursday"; break;
    case 5: week_day = "Friday"; break;
    case 6: week_day = "Saturday"; break;
    case 0: week_day = "Sunday"; break;
    }
    uint8_t week_x = (COL_NUM - strlen(week_day) * FONT_W) / 2;
    // 清空这一片区域
    for (uint8_t i = week_x - FONT_W * 3; i < WEEK_X + FONT_W * 9; i++)
        for (uint8_t j = WEEK_Y; j < WEEK_Y + FONT_H; j++)
            oled_setpixel(i, j, 0);
    oled_show_string(week_day, WEEK_X, WEEK_Y, WEEK_SPACING);
}

void show_modify_line()
{
    for (uint8_t i = 0;; i++)
    {
        if (modify_line[i].flag == 2) break;
        for (uint8_t x = 0; x < modify_line[i].w; x++)
        {
            for (uint8_t y = 0; y < modify_line[i].h; y++)
                oled_setpixel(modify_line[i].x + x, modify_line[i].y + y,
                              modify_line[i].flag);
        }
    }
}

void watch_init()
{
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == RTC_INIT_FLAG) return;
    strptime(__TIMESTAMP__, "%a %b  %d %H:%M:%S %Y", &curtime);
    set_time();
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_INIT_FLAG);
}

void watch_update()
{
    charging =
        HAL_GPIO_ReadPin(Charging_GPIO_Port, Charging_Pin) == GPIO_PIN_SET;

    for (uint8_t i = 0;; i++)
    {
        if (modify_line[i].flag == 2) break;
        modify_line[i].flag = 0;
    }
    switch (curstate)
    {
    case WATCH_START: update_time(); break;
    case MODIFY_HOUR:
    case MODIFY_MIN:
    case MODIFY_SEC:
    case MODIFY_YEAR:
    case MODIFY_MON:
    case MODIFY_DAY: modify_line[curstate].flag = 1; break;
    }
    show_time();
    show_battery_level();
    show_modify_line();
}

void key_clicked(int key_num)
{
    static uint8_t oled_display_state = 1;

    switch (curstate)
    {
    case WATCH_START:
        switch (key_num)
        {
        case 0:
            if (oled_display_state == 1)
                oled_display_off();
            else
                oled_display_on();
            oled_display_state = 1 - oled_display_state;
            break;
        case 1: curstate = MODIFY_HOUR; break;
        }
        break;
    case MODIFY_HOUR:
    case MODIFY_MIN:
    case MODIFY_SEC:
    case MODIFY_YEAR:
    case MODIFY_MON:
    case MODIFY_DAY:
        switch (key_num)
        {
        case 0:
            set_time();
            curstate = WATCH_START;
            break;
        case 1: curstate = modify_action[curstate].next_state; break;
        case 2:
            (*modify_action[curstate].modify_value)--;
            check_time();
            break;
        case 3:
            (*modify_action[curstate].modify_value)++;
            check_time();
            break;
        }
        break;
    }
}

void key_long_clicked(int key_num)
{
    switch (curstate)
    {
    case MODIFY_HOUR:
    case MODIFY_MIN:
    case MODIFY_SEC:
    case MODIFY_YEAR:
    case MODIFY_MON:
    case MODIFY_DAY:
        switch (key_num)
        {
        case 1: curstate = modify_action[curstate].pre_state; break;
        }
        break;
    default: break;
    }
}