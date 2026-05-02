#include "jx_uart_send.h"

// 串口通信消息头
const unsigned char g_uart_send_head[] = {
  0xaa, 0x55
};

// 串口通信消息尾
const unsigned char g_uart_send_foot[] = {
  0x55, 0xaa
};

// 串口发送函数实现
void _uart_send_impl(unsigned char* buff, int len) {
  // TODO: 调用项目实际的串口发送函数
  /*
  int i = 0;
  unsigned char c;
  for (i = 0; i < len; i++) {
    c = buff[i];
    printf("%02X ", c);
  }
  printf("\n");
  */
}

// action: RX_0x01
void _uart_RX_0x01(unsigned char CS_1) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x01;
  param.d_uchar = CS_1;
  buff[3] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 4] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 6);
}

// action: RX_0x02
void _uart_RX_0x02(unsigned char CS_2, unsigned char CS_3) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x02;
  param.d_uchar = CS_2;
  buff[3] = param.d_uchar;
  param.d_uchar = CS_3;
  buff[4] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 5] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 7);
}

// action: RX_0x03
void _uart_RX_0x03(unsigned char CS_4, unsigned char CS_5) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x03;
  param.d_uchar = CS_4;
  buff[3] = param.d_uchar;
  param.d_uchar = CS_5;
  buff[4] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 5] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 7);
}

// action: RX_0x04
void _uart_RX_0x04(unsigned char CS_6) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x04;
  param.d_uchar = CS_6;
  buff[3] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 4] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 6);
}

// action: RX_0x05
void _uart_RX_0x05() {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x05;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 3] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 5);
}

// action: RX_0x06
void _uart_RX_0x06() {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x06;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 3] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 5);
}

// action: RX_0x07
void _uart_RX_0x07() {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x07;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 3] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 5);
}

// action: RX_0x08
void _uart_RX_0x08() {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x08;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 3] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 5);
}

// action: RX_0x09
void _uart_RX_0x09(unsigned char CS_7, unsigned char CS_8, unsigned char CS_9, unsigned char CS_10) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x09;
  param.d_uchar = CS_7;
  buff[3] = param.d_uchar;
  param.d_uchar = CS_8;
  buff[4] = param.d_uchar;
  param.d_uchar = CS_9;
  buff[5] = param.d_uchar;
  param.d_uchar = CS_10;
  buff[6] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 7] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 9);
}

