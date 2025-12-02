/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../APP/Player/music_player.h"
#include "../App/GUI/gui_app.h"
#include "../Gui/lvgl/lvgl.h"
#include "../Gui/lvgl_port/lv_port_disp.h"
#include "../Gui/lvgl_port/lv_port_indev.h"
#include "../Touch/touch.h"
#include "lcd.h"
#include "tim.h"

// 外部变量声明（来自 music_player.c）
extern volatile uint8_t request_play;
extern char current_song_name[64];
extern void music_player_process_song(const char *filename);

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern TIM_HandleTypeDef htim6;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

/* Definitions for audioTask */
osThreadId_t audioTaskHandle;
const osThreadAttr_t audioTask_attributes = {
    .name = "audioTask",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityAboveNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartAudioTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void)
{
    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* creation of defaultTask */
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    /* creation of audioTask */
    audioTaskHandle = osThreadNew(StartAudioTask, NULL, &audioTask_attributes);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
    /* USER CODE END RTOS_THREADS */

    /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */
    /* USER CODE END RTOS_EVENTS */
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
    /* USER CODE BEGIN StartDefaultTask */
    HAL_Delay(100);  // Wait for power stability
    lcd_init();
    lv_init();
    lv_port_disp_init();

    // Initialize touch screen hardware
    tp_init();

    // Initialize LVGL input device
    lv_port_indev_init();

    // 【关键修改】先启动 LVGL tick timer，再初始化 GUI
    HAL_TIM_Base_Start_IT(&htim6);

    // 【临时注释】先验证 LVGL 显示，避免被 ES8388/SD 卡初始化卡住
    music_player_init();

    // 初始化 GUI（此时 LVGL tick 已经在运行）
    gui_app_init();

    // 【关键修改】手动触发一次渲染，确保界面显示
    lv_task_handler();
    osDelay(10);  // 给足够时间完成第一帧渲染

    // music_player_play("1.wav");

    /* Infinite loop */
    for (;;)
    {
        lv_task_handler();  // Handle LVGL tasks
        osDelay(5);         // Delay 5ms
    }
    /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void StartAudioTask(void *argument)
{
    while (1)
    {
        if (request_play)
        {
            request_play = 0;
            music_player_process_song(current_song_name);
        }
        osDelay(10);
    }
}
/* USER CODE END Application */
