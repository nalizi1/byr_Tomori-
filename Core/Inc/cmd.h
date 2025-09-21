//
// Created by 10616 on 2025/9/21.
//

#ifndef CMD_H
#define CMD_H
#include "stm32f4xx.h"
#include "main.h"
#include "usart.h"


#define MAX_CMD_ARG_LENGTH 16
#define MAX_CMD_INFO_LENGTH 64
#define MAX_CMD_LINE_LENGTH 128
#define MAX_ARGV 12 // 最大命令参数数量
#define PRINT_BUFFER_SIZE 128
#define DMA_BUFFER_SIZE 128

#define CMD_UART huart1

struct cmd_info
{
    void (*cmd_func)(int argc, char *argv[]); // 函数指针
    char *cmd_usage;
};

void CMD_Init(void);
void uprintf(char *fmt, ...);
void uprintf_to(UART_HandleTypeDef *huart, char *fmt, ...);


int CMD_Parse(char *cmd_line, int *argc, char *argv[]);
void CMD_Run(void);
void CMD_FuncInit(void);
void CMD_Add(char *cmd_name, char *cmd_usage, void (*cmd_func)(int argc, char *argv[]));



void CMD_HelpFunc(int argc, char *argv[]);

void CMD_Hello(int *argc, char *argv[]);
void CMD_Set_Mode(int *argc, char *argv[]);
void CMD_Set_Brightness(int *argc, char *argv[]);
#endif //CMD_H

