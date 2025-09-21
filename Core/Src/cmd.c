//
// Created by 10616 on 2025/9/21.
//
#include "cmd.h"
#include "hash.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *delim = ", \r\n\0";
static HashTable CMD_CommandTable; // 指令表
uint8_t UART_DMA_RxOK_Flag = 0;

char *cmd_argv[MAX_ARGV];
uint8_t UART_RxBuffer_Raw[DMA_BUFFER_SIZE];
char UART_RxBuffer[DMA_BUFFER_SIZE];


/* private----------------------------------------------------- */
/**
 * @brief	字符串比较函数，hash表中使用
 */
static int str_cmp(const void *a, const void *b)
{
    return strcmp((char *)a, (char *)b) != 0;
}

/**
 * @brief	输出函数使用说明，遍历hash表中使用
 * @param key cmd_name
 * @param value cmd_usage
 */
static void _cmd_help(const void *key, void **value, void *c1)
{
    UNUSED(c1);
    char *usage = ((struct cmd_info *)(*value))->cmd_usage;
    uprintf("|%31s: %-31s|\r\n", key, usage);
}


/* public----------------------------------------------------- */
/**
 * @brief 串口中断回调函数
 * @param huart 串口号
 */
void HAL_UART_IDLECallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == CMD_UART.Instance)
    {
        __HAL_UART_CLEAR_IDLEFLAG(huart); //清除空闲标志
        // uint8_t temp;
        // temp = huart->Instance->SR;
        // temp = huart->Instance->DR; //读出串口的数据，防止在关闭DMA期间有数据进来，造成ORE错误
        // UNUSED(temp);
        /* HAL_UART_DMAStop(&CMD_UART); */ //停止本次DMA
        CLEAR_BIT(huart->Instance->CR3, USART_CR3_DMAR);
        HAL_DMA_Abort(huart->hdmarx);
        CLEAR_BIT(huart->Instance->CR1, (USART_CR1_RXNEIE | USART_CR1_PEIE));
        CLEAR_BIT(huart->Instance->CR3, USART_CR3_EIE);
        huart->RxState = HAL_UART_STATE_READY;
        uint8_t *clr = UART_RxBuffer_Raw;
        while (*(clr++) == '\0' && clr < UART_RxBuffer_Raw + DMA_BUFFER_SIZE) // 找到开头，避免传输噪声
            ;
        strcpy((char *)UART_RxBuffer, (char *)(clr - 1));
        if (UART_RxBuffer[0] != '\0')
        {
            UART_DMA_RxOK_Flag = 1;
        }
        memset(UART_RxBuffer_Raw, 0, DMA_BUFFER_SIZE);
        HAL_UART_Receive_DMA(&CMD_UART, (uint8_t *)&UART_RxBuffer_Raw, DMA_BUFFER_SIZE); // 开启下一次中断
    }
}

/**
 * @brief	指令初始化函数
 * @return	None
 */
void CMD_Init(void)
{
    HAL_UART_Receive_DMA(&CMD_UART, (uint8_t *)&UART_RxBuffer_Raw, 99);
    if (CMD_CommandTable == NULL)
    {
        CMD_CommandTable = HashTable_Create(str_cmp, HashStr, NULL);
    }
    CMD_Add("help", "show cmd usage", CMD_HelpFunc);
    __HAL_UART_ENABLE_IT(&CMD_UART, UART_IT_IDLE); // 开启空闲中断
    uprintf("init \r\n\r\n");
    CMD_FuncInit();
}

char print_buffer[PRINT_BUFFER_SIZE];

/**
 * @brief   打印到cmd串口
 *
 */
void uprintf(char *fmt, ...)
{
    // 等待DMA准备完毕
    while (HAL_DMA_GetState(CMD_UART.hdmatx) == HAL_DMA_STATE_BUSY)
        HAL_Delay(1);
    int size;
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    size = vsnprintf(print_buffer, PRINT_BUFFER_SIZE, fmt, arg_ptr);
    va_end(arg_ptr);

    HAL_UART_Transmit_DMA(&CMD_UART, (uint8_t *)print_buffer, size);
}

/**
 * @brief   打印到指定串口
 * @param huart     指定串口
 *
 */
void uprintf_to(UART_HandleTypeDef *huart, char *fmt, ...)
{
    // 等待DMA准备完毕
    while (HAL_DMA_GetState(CMD_UART.hdmatx) == HAL_DMA_STATE_BUSY)
        HAL_Delay(1);
    int size;
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    size = vsnprintf(print_buffer, PRINT_BUFFER_SIZE, fmt, arg_ptr);
    va_end(arg_ptr);

    HAL_UART_Transmit_DMA(huart, (uint8_t *)print_buffer, size);
    // HAL_UART_Transmit(huart,(uint8_t *)print_buffer,size,1000);
}
/**
 * @brief	指令帮助函数
 *
 */
void CMD_HelpFunc(int argc, char *argv[])
{
    // FIXME: ZeroVoid	2019/09/23	 dma usage 输出不完整，调试输出没问题
    uprintf("===============================help===============================\r\n");
    uprintf("|              CMD              |           Description          |\r\n");
    HashTable_Map(CMD_CommandTable, _cmd_help, NULL); // 遍历哈希表，打印所有帮助指令
    uprintf("==================================================================\r\n");
}
/**
* @brief	指令添加函数
* @param	cmd_name    指令名称
* @param   cmd_usage   指令使用说明
* @param   cmd_func    指令函数指针 argc 参数个数(含指令名称), argv 参数字符串数组
* @return	None
*/
void CMD_Add(char *cmd_name, char *cmd_usage, void (*cmd_func)(int argc, char *argv[]))
{
    // FIXME: ZeroVoid	2019/9/23	 name or usage too long
    struct cmd_info *new_cmd = (struct cmd_info *)malloc(sizeof(struct cmd_info));
    char *name = (char *)malloc(sizeof(char) * (strlen(cmd_name) + 1));
    char *usage = (char *)malloc(sizeof(char) * (strlen(cmd_usage) + 1));
    strcpy(name, cmd_name);
    strcpy(usage, cmd_usage);
    new_cmd->cmd_func = cmd_func;
    new_cmd->cmd_usage = usage;
    HashTable_Insert(CMD_CommandTable, name, new_cmd);
}

/**
 * @brief	将输入分割，并记录参数个数
 * @param	cmd_line	输入指令字符串
 * @param   argc        指令个数
 * @param   argv        分割后参数列表
 * @return	None
 */
int CMD_Parse(char *cmd_line, int *argc, char *argv[])
{
    char *token = strtok(cmd_line, delim);
    int arg_index = 0;

    while (token && arg_index <= MAX_ARGV)
    {
        argv[arg_index++] = token;
        token = strtok(NULL, delim);
    }
    *argc = arg_index;
    return 0;
}

/**
 * @brief	cmd指令执行函数
 * @param	argc    参数个数
 * @param   argv    参数列表
 * @return	0   正常执行返回
 *          1   未找到指令
 */
int CMD_Exec(int argc, char *argv[])
{
    struct cmd_info *cmd = (struct cmd_info *)HashTable_GetValue(CMD_CommandTable, argv[0]);
    if (cmd != NULL)
    {
        cmd->cmd_func(argc, argv);
        return 0;
    }
    uprintf("cmd not find\r\n");
    return 1;
}

void CMD_FuncInit(void) {
    CMD_Add("hello","hello\r\n",CMD_Hello);

    uprintf("==cmd init done==\r\n");
}

/**
 * @brief   运行时调用，处理cmd命令
 */
void CMD_Run(void) {
    if (UART_DMA_RxOK_Flag)
    {
        int cmd_argc;
        int erro_n;
        erro_n = CMD_Parse((char *)UART_RxBuffer, &cmd_argc, cmd_argv); //解析命令
        erro_n = CMD_Exec(cmd_argc, cmd_argv);                          //执行命令
        UNUSED(erro_n);
        memset(UART_RxBuffer, 0, 98);
        UART_DMA_RxOK_Flag = 0;
    }
}

/*自定义指令*/


void CMD_Hello(int *argc, char *argv[]) {
    uprintf("Hello\r\n");
}
