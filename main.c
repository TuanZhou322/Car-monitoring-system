/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-5-10      ShiHao       first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <led_matrix/drv_matrix_led.h>


#include <drv_lcd.h>
#include <aht10.h>

#include <rttlogo.h>
#include <rtdbg.h>

#define DBG_TAG "main"
#define DBG_LVL         DBG_LOG
#define PIN_KEY0        GET_PIN(C, 0)     // PC0:  KEY0         --> KEY
#define PIN_KEY1        GET_PIN(C, 1)      // PC1 :  KEY1         --> KEY
#define PIN_KEY2        GET_PIN(C, 4)      // PC4 :  KEY2         --> KEY
#define PIN_WK_UP       GET_PIN(C, 5)     // PC5:  WK_UP        --> KEY
#define PIN_BEEP        GET_PIN(B, 0)      // PA1:  BEEP         --> BEEP (PB1)
#define ST_PIN    GET_PIN(E, 11)  // ST 对应 PE11
#define OE_PIN    GET_PIN(E, 12)  // OE 对应 PE12
#define EOC_PIN   GET_PIN(E, 13)  // EOC 对应 PE13
#define CLK_PIN   GET_PIN(E, 14)  // CLK 对应 PE14
#define ALE_PIN   GET_PIN(B, 14)
#define DOUT_PIN   GET_PIN(B, 2)
#define MOTOR_PIN   GET_PIN(D, 9)
#define PIN_LED_B   GET_PIN(F, 11)      // PF11 :  LED_B        --> LED
#define PIN_LED_R   GET_PIN(F, 12)      // PF12 :  LED_R        --> LED
// ADC0809 数据输出引脚（PG1-PG7）
#define PG1_PIN   GET_PIN(G, 1)   // 数据位 D0
#define PG2_PIN   GET_PIN(G, 2)   // 数据位 D1
#define PG3_PIN   GET_PIN(G, 3)   // 数据位 D2
#define PG4_PIN   GET_PIN(G, 4)   // 数据位 D3
#define PG5_PIN   GET_PIN(G, 5)   // 数据位 D4
#define PG6_PIN   GET_PIN(G, 6)   // 数据位 D5
#define PG7_PIN   GET_PIN(G, 7)   // 数据位 D6
#define PD7_PIN   GET_PIN(D, 7)   // 数据位 D7
    int temp_limit=40;
    int humi_limit=50;
    int count=0;
    int alarm=0;
    rt_uint8_t CLK = 0;       // 时钟电平状态
    rt_uint16_t adc_result = 0; // 存储 ADC 转换结果（0-1000范围）
    struct rt_timer clk_timer; // 时钟定时器
void Key_Processing()
{
    if(rt_pin_read(PIN_WK_UP)==PIN_LOW)
               {
                   rt_thread_delay(100);
                   if(rt_pin_read(PIN_WK_UP)==PIN_LOW)
                   {
                       temp_limit+=1;
                   }

               }
    if(rt_pin_read(PIN_KEY1)==PIN_LOW)
               {
                   rt_thread_delay(100);
                   if(rt_pin_read(PIN_KEY1)==PIN_LOW)
                   {
                       temp_limit-=1;
                   }
               }
    if(rt_pin_read(PIN_KEY0)==PIN_LOW)
               {
                   rt_thread_delay(100);
                   if(rt_pin_read(PIN_KEY0)==PIN_LOW)
                   {
                       humi_limit-=1;
                   }

               }
    if(rt_pin_read(PIN_KEY2)==PIN_LOW)
               {
                   rt_thread_delay(100);
                   if(rt_pin_read(PIN_KEY2)==PIN_LOW)
                   {
                       humi_limit+=1;
                   }
               }
}
void Alarm_Processing()
{
    if(alarm==0)
       {
                 rt_pin_write(PIN_BEEP,PIN_LOW);
                 rt_pin_write(PIN_LED_B, PIN_LOW);
                 rt_pin_write(PIN_LED_R, PIN_HIGH);
       }
    else if(alarm=1)
        {
                  rt_pin_write(PIN_BEEP,PIN_HIGH);
                  rt_pin_write(PIN_LED_R, PIN_LOW);
                  rt_pin_write(PIN_LED_B, PIN_HIGH);
        }
}
void Pin_Initialize()
{
    /* 设置 KEY0 引脚的模式为输入上拉模式 */
    rt_pin_mode(PIN_KEY0, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(PIN_KEY1, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(PIN_KEY2, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(PIN_WK_UP, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(PIN_BEEP, PIN_MODE_OUTPUT);
    /* 1. 初始化控制引脚 */
    rt_pin_mode(ST_PIN,  PIN_MODE_OUTPUT);
    rt_pin_mode(OE_PIN,  PIN_MODE_OUTPUT);
    rt_pin_mode(EOC_PIN, PIN_MODE_INPUT);
    rt_pin_mode(CLK_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(ALE_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(MOTOR_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_LED_R, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_LED_B, PIN_MODE_OUTPUT);
    rt_pin_mode(DOUT_PIN, PIN_MODE_INPUT);
    /* 2. 初始化数据引脚 */
    rt_pin_mode(PG1_PIN, PIN_MODE_INPUT);
    rt_pin_mode(PG2_PIN, PIN_MODE_INPUT);
    rt_pin_mode(PG3_PIN, PIN_MODE_INPUT);
    rt_pin_mode(PG4_PIN, PIN_MODE_INPUT);
    rt_pin_mode(PG5_PIN, PIN_MODE_INPUT);
    rt_pin_mode(PG6_PIN, PIN_MODE_INPUT);
    rt_pin_mode(PG7_PIN, PIN_MODE_INPUT);
    rt_pin_mode(PD7_PIN, PIN_MODE_INPUT);
}
void clk_timer_callback(void *args)
{
    CLK = !CLK;                     // 翻转时钟电平
    rt_pin_write(CLK_PIN, CLK);     // 输出到 CLK 引脚
    rt_pin_write(MOTOR_PIN, CLK);
}
void Timer_Initialize()
{
    rt_timer_init(&clk_timer, "adc_clk", clk_timer_callback,
                  RT_NULL, 1, RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(&clk_timer);
}

rt_uint8_t adc0809_read(void)
{
    rt_uint8_t adc_data = 0;

    /* 1. 启动转换 */
    rt_pin_write(ST_PIN, PIN_LOW);
    rt_pin_write(ST_PIN, PIN_HIGH);
    rt_pin_write(ST_PIN, PIN_LOW);

    /* 2. 等待转换完成 */
    while (rt_pin_read(EOC_PIN) == PIN_LOW)
    {
        rt_thread_yield(); // 让出CPU，避免死循环
    }

    /* 3. 使能输出并读取完整 8 位数据 */
    rt_pin_write(OE_PIN, PIN_HIGH);

    // 逐位读取 PG1-PG7 和 PD7 的数据，组合成 8 位值
    adc_data = (rt_pin_read(PG1_PIN) << 0) |  // D0
               (rt_pin_read(PG2_PIN) << 1) |  // D1
               (rt_pin_read(PG3_PIN) << 2) |  // D2
               (rt_pin_read(PG4_PIN) << 3) |  // D3
               (rt_pin_read(PG5_PIN) << 4) |  // D4
               (rt_pin_read(PG6_PIN) << 5) |  // D5
               (rt_pin_read(PG7_PIN) << 6) |  // D6
               (rt_pin_read(PD7_PIN) << 7);   // D7（新增）

    rt_pin_write(OE_PIN, PIN_LOW); // 关闭输出

    return adc_data;
}

int main(void)
{
    float humidity, temperature;
    aht10_device_t dev;
//    ap3216c_device_t dev2;
//    const char *i2c_bus_name_2 = "i2c2";
    /* 总线名称 */
    const char *i2c_bus_name = "i2c3";
    char CO;
    int count = 0;
    Pin_Initialize();
    Timer_Initialize();
    lcd_clear(WHITE);
    /* show RT-Thread logo */
    lcd_show_image(0, 0, 240, 69, image_rttlogo);
    /* set the background color and foreground color */
    lcd_set_color(WHITE, BLACK);
    dev = aht10_init(i2c_bus_name);
//    dev2 = ap3216c_init(i2c_bus_name_2);
        if (dev == RT_NULL)
        {
            LOG_E(" The AHT10 initializes failure");
            return 0;
        }
        rt_pin_write(ALE_PIN, PIN_LOW);
        while (1)
        {
            /* 读取湿度 */

            humidity = aht10_read_humidity(dev);
//            LOG_D("humidity   : %d.%d %%", (int)humidity, (int)(humidity * 10) % 10);
            lcd_show_string(10, 69, 24, "humidity   : %d.%d %%", (int)humidity, (int)(humidity * 10) % 10);
            /* 读取温度 */
            temperature = aht10_read_temperature(dev);
//            LOG_D("temperature: %d.%d", (int)temperature, (int)(temperature * 10) % 10);
            lcd_show_string(10, 69+24, 24, "temperature: %d.%d C", (int)temperature, (int)(temperature * 10) % 10);

            Key_Processing();
            rt_pin_write(ALE_PIN, PIN_HIGH);
            //           adc_result=adc0809_read();
            Alarm_Processing();
            if(((int)(temperature*10)>=temp_limit*10)||CO==1)
            {
                alarm=1;
            }

            else
            {
                alarm=0;

            }
            if((int)(humidity*10)>=humi_limit*10)
            {
                lcd_show_string(10, 69+120, 24, "humidity too high!");
            }

            else
            {
                lcd_show_string(10, 69+120, 24, "                  ");

            }
            if(rt_pin_read(DOUT_PIN)==PIN_LOW)
            {
                lcd_show_string(10, 69+96, 24, "CO: high  ");
                CO=1;
            }
            else
            {
                lcd_show_string(10, 69+96, 24, "CO: normal  ");
                CO=0;
            }
            lcd_show_string(10, 69+48, 24, "temp limit: %d C ", temp_limit);
            lcd_show_string(10, 69+72, 24, "humi limit: %d %% ", humi_limit);
            rt_kprintf("%d.%d %d.%d %d %d %d\n",(int)temperature, (int)(temperature * 10) % 10,(int)humidity, (int)(humidity * 10) % 10,temp_limit,humi_limit,CO);
        }

    return 0;
}
