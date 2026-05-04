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
void _uart_RX_0x01(unsigned char CountdownSet) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x01;
  param.d_uchar = CountdownSet;
  buff[3] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 4] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 6);
}

// action: RX_0x02
void _uart_RX_0x02(unsigned char TimeHor, unsigned char TimeMin) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x02;
  param.d_uchar = TimeHor;
  buff[3] = param.d_uchar;
  param.d_uchar = TimeMin;
  buff[4] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 5] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 7);
}

// action: RX_0x03
void _uart_RX_0x03(unsigned char HeartRate, unsigned char Cadence) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x03;
  param.d_uchar = HeartRate;
  buff[3] = param.d_uchar;
  param.d_uchar = Cadence;
  buff[4] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 5] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 7);
}

// action: RX_0x04
void _uart_RX_0x04(unsigned char CountdownTenSec) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x04;
  param.d_uchar = CountdownTenSec;
  buff[3] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 4] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 6);
}

// action: RX_0x05
void _uart_RX_0x05(unsigned char Cadence) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x05;
  param.d_uchar = Cadence;
  buff[3] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 4] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 6);
}

// action: RX_0x06
void _uart_RX_0x06(unsigned char HeartRate) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x06;
  param.d_uchar = HeartRate;
  buff[3] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 4] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 6);
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
void _uart_RX_0x09(unsigned char CS_KM_INT, unsigned char CS_KM_DEC, unsigned char CS_HOUR, unsigned char CS_MIN) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_RX_0x09;
  param.d_uchar = CS_KM_INT;
  buff[3] = param.d_uchar;
  param.d_uchar = CS_KM_DEC;
  buff[4] = param.d_uchar;
  param.d_uchar = CS_HOUR;
  buff[5] = param.d_uchar;
  param.d_uchar = CS_MIN;
  buff[6] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 7] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 9);
}

// action: AlarmSetSuccess
void _uart_AlarmSetSuccess(unsigned char Hours, unsigned char Minutes) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_AlarmSetSuccess;
  param.d_uchar = Hours;
  buff[3] = param.d_uchar;
  param.d_uchar = Minutes;
  buff[4] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 5] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 7);
}

// action: AlarmSetError
void _uart_AlarmSetError() {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_AlarmSetError;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 3] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 5);
}

// action: CountDownSuc
void _uart_CountDownSuc(unsigned char CountDownMin) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_CountDownSuc;
  param.d_uchar = CountDownMin;
  buff[3] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 4] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 6);
}

// action: CountDownEro
void _uart_CountDownEro() {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_CountDownEro;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 3] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 5);
}

// action: AlarmGroupOK
void _uart_AlarmGroupOK(unsigned char AlarmGroup) {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_AlarmGroupOK;
  param.d_uchar = AlarmGroup;
  buff[3] = param.d_uchar;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 4] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 6);
}

// action: AlarmGroupEro
void _uart_AlarmGroupEro() {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_AlarmGroupEro;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 3] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 5);
}

// action: INPUT_TIMEOUT
void _uart_INPUT_TIMEOUT() {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_INPUT_TIMEOUT;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 3] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 5);
}

// action: Reminder
void _uart_Reminder() {
  uart_param_t param;
  int i = 0;
  unsigned char buff[UART_SEND_MAX] = {0};
  for (i = 0; i < UART_MSG_HEAD_LEN; i++) {
      buff[i + 0] = g_uart_send_head[i];
  }
  buff[2] = U_MSG_Reminder;
  for (i = 0; i < UART_MSG_FOOT_LEN; i++) {
      buff[i + 3] = g_uart_send_foot[i];
  }
  _uart_send_impl(buff, 5);
}

