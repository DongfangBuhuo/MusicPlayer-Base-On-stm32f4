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
    .priority = (osPriority_t)osPriorityAboveNormal,
};

/* Definitions for audioTask */
osThreadId_t audioTaskHandle;
const osThreadAttr_t audioTask_attributes = {
    .name = "audioTask",
    .stack_size = 1536 * 4,
    .priority = (osPriority_t)osPriorityHigh,
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

    HAL_TIM_Base_Start_IT(&htim6);
    music_player_init();
    gui_app_init();

    lv_task_handler();
    osDelay(10);

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
    Music_Event event;
    while (1)
    {
        if (osMessageQueueGet(music_eventQueueHandle, &event, NULL, 0) == osOK)
        {
            switch (event.type)
            {
                case MUSIC_RELOAD:
                    music_player_process_song();
                    break;
                case MUSIC_PAUSE:
                    music_player_pause();
                    break;
                case MUSIC_RESUME:
                    music_player_resume();
                    break;
                case MUSIC_STOP:
                    music_player_stop();
                    break;
                case MUSIC_SET_HEADPHONE_VOL:
                    music_player_set_headphone_volume(event.param);
                    break;
                case MUSIC_SET_SPEAKER_VOL:
                    music_player_set_speaker_volume(event.param);
                    break;
                case MUSIC_NEXT:
                    music_player_set_currentIndex(music_player_get_currentIndex() + 1);
                    music_player_process_song();
                    break;
                case MUSIC_PREV:
                    music_player_set_currentIndex(music_player_get_currentIndex() - 1);
                    music_player_process_song();
                    break;
                default:
                    break;
            }
        }

        // 2. 维持音频播放 (填充数据)
        music_player_update();

        // 3. 稍微让出 CPU
        if (!isPlaying)
        {
            osDelay(10);
        }
    }
}
/* USER CODE END Application */
