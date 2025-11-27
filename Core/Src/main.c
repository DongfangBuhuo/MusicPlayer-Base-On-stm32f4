/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include "main.h"
#include "dma.h"
#include "fatfs.h"
#include "i2c.h"
#include "i2s.h"
#include "sdio.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "es8388.h"
#include <string.h>
#include "lcd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
	char ChunkID[4];       // "RIFF"
	uint32_t ChunkSize;
	char Format[4];        // "WAVE"
	char Subchunk1ID[4];   // "fmt "
	uint32_t Subchunk1Size;
	uint16_t AudioFormat;
	uint16_t NumChannels;
	uint32_t SampleRate;
	uint32_t ByteRate;
	uint16_t BlockAlign;
	uint16_t BitsPerSample;
	char Subchunk2ID[4];   // "data"
	uint32_t Subchunk2Size;
} WAV_Header_TypeDef;

#define AUDIO_BUFFER_SIZE 4096 // 4KB 缓存
uint16_t audio_buffer[AUDIO_BUFFER_SIZE];
FRESULT res;
FATFS fs;
FIL wavFile;
WAV_Header_TypeDef wavHeader;
UINT bytesRead;
uint8_t isPlaying = 0;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_I2S2_Init();
	MX_I2C1_Init();
	MX_SDIO_SD_Init();
	MX_FATFS_Init();
	MX_FSMC_Init();
	/* USER CODE BEGIN 2 */
	HAL_Delay(100); // 等待电源稳定
	lcd_init();
	// LED快闪3次表示开始初始化
	for (int i = 0; i < 3; i++)
	{
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_Delay(100);
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_Delay(100);
	}

	// 初始化ES8388
	if (ES8388_Init(&hi2c1) != 0)
	{
		// LED常亮表示ES8388初始化失败
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
		while (1)
			;
	}

	// 明确关闭喇叭输出，只使用耳机
	ES8388_SetSpeakerEnable(0);

	// LED闪1次表示ES8388初始化成功
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
	HAL_Delay(300);
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
	HAL_Delay(300);

	// 挂载SD卡
	res = f_mount(&fs, "0:/", 1);
	if (res != FR_OK)
	{
		// LED慢闪表示SD卡挂载失败
		while (1)
		{
			HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
			HAL_Delay(500);
		}
	}

	// LED闪2次表示SD卡挂载成功
	for (int i = 0; i < 2; i++)
	{
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_Delay(200);
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_Delay(200);
	}

	// 打开WAV文件
	res = f_open(&wavFile, "0:/music/1.wav", FA_READ);
	if (res != FR_OK)
	{
		// LED快速闪烁表示文件打开失败
		while (1)
		{
			HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
			HAL_Delay(100);
		}
	}

	// 读取WAV头
	f_read(&wavFile, &wavHeader, sizeof(wavHeader), &bytesRead);

	// 简单检查是否为 WAV 格式
	if (strncmp(wavHeader.ChunkID, "RIFF", 4) == 0
			&& strncmp(wavHeader.Format, "WAVE", 4) == 0)
	{
		// 根据WAV文件的采样率重新配置I2S
		uint32_t sampleRate = wavHeader.SampleRate;

		// 停止I2S（如果正在运行）
		HAL_I2S_DMAStop(&hi2s2);

		// 根据采样率重新初始化I2S
		if (sampleRate == 8000)
		{
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_8K;
		}
		else if (sampleRate == 11025)
		{
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_11K;
		}
		else if (sampleRate == 16000)
		{
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_16K;
		}
		else if (sampleRate == 22050)
		{
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_22K;
		}
		else if (sampleRate == 32000)
		{
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_32K;
		}
		else if (sampleRate == 44100)
		{
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_44K;
		}
		else if (sampleRate == 48000)
		{
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_48K;
		}
		else if (sampleRate == 96000)
		{
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_96K;
		}
		else if (sampleRate == 192000)
		{
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_192K;
		}
		else
		{
			// 不支持的采样率，默认使用44.1kHz
			hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_44K;
		}

		// 重新初始化I2S
		HAL_I2S_Init(&hi2s2);

		// 读取第一块音频数据（注意：AUDIO_BUFFER_SIZE是uint16_t数组大小，所以字节数是*2）
		f_read(&wavFile, audio_buffer, AUDIO_BUFFER_SIZE * 2, &bytesRead);
		// LED关闭表示准备播放
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_Delay(500);

		// 启动 DMA 循环传输
		HAL_I2S_Transmit_DMA(&hi2s2, audio_buffer, AUDIO_BUFFER_SIZE);

		isPlaying = 1;
	}
	else
	{
		// 文件格式错误，LED持续快闪
		f_close(&wavFile);
		while (1)
		{
			HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
			HAL_Delay(50);
		}
	}
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
		// LED慢速闪烁表示正在播放
		if (isPlaying)
		{
			HAL_Delay(1000);
			HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
		}
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct =
	{ 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct =
	{ 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
	{
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */
// DMA传输一半完成回调 (填充Buffer的前半部分)
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI2) // 确认是 I2S2
	{
		// 填充Buffer前半部分 (0 到 AUDIO_BUFFER_SIZE/2)
		// audio_buffer是uint16_t数组，AUDIO_BUFFER_SIZE=4096
		// 前半部分是2048个uint16_t，即4096字节
		f_read(&wavFile, &audio_buffer[0], AUDIO_BUFFER_SIZE, &bytesRead);

		// 处理文件结束 (循环播放)
		if (bytesRead < AUDIO_BUFFER_SIZE)
		{
			f_lseek(&wavFile, sizeof(WAV_Header_TypeDef)); // 跳过WAV头重新开始
			// 如果读取不足，补零防止杂音
			if (bytesRead < AUDIO_BUFFER_SIZE)
			{
				for (uint32_t i = bytesRead / 2; i < AUDIO_BUFFER_SIZE / 2; i++)
				{
					audio_buffer[i] = 0;
				}
			}
		}
	}
}

// DMA传输全部完成回调 (填充Buffer的后半部分)
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI2)
	{
		// 填充Buffer后半部分 (AUDIO_BUFFER_SIZE/2 到 AUDIO_BUFFER_SIZE)
		// 后半部分也是2048个uint16_t，即4096字节
		f_read(&wavFile, &audio_buffer[AUDIO_BUFFER_SIZE / 2],
				AUDIO_BUFFER_SIZE, &bytesRead);

		if (bytesRead < AUDIO_BUFFER_SIZE)
		{
			f_lseek(&wavFile, sizeof(WAV_Header_TypeDef));
			// 补零防止杂音
			if (bytesRead < AUDIO_BUFFER_SIZE)
			{
				for (uint32_t i = AUDIO_BUFFER_SIZE / 2 + bytesRead / 2;
						i < AUDIO_BUFFER_SIZE; i++)
				{
					audio_buffer[i] = 0;
				}
			}
		}
	}
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
