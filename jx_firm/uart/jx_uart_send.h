#ifndef JX_UART_SEND_H_
#define JX_UART_SEND_H_

#ifdef __cplusplus
extern "C" {
#endif

// 串口发送消息最大长度
#define UART_SEND_MAX      32
#define UART_MSG_HEAD_LEN  2
#define UART_MSG_FOOT_LEN  2

// 串口发送消息号
#define U_MSG_RX_0x01      1
#define U_MSG_RX_0x02      2
#define U_MSG_RX_0x03      3
#define U_MSG_RX_0x04      4
#define U_MSG_RX_0x05      5
#define U_MSG_RX_0x06      6
#define U_MSG_RX_0x07      7
#define U_MSG_RX_0x08      8
#define U_MSG_RX_0x09      9

// 串口消息参数类型
typedef union {
  double d_double;
  int d_int;
  unsigned char d_ucs[8];
  char d_char;
  unsigned char d_uchar;
  unsigned long d_long;
  short d_short;
  float d_float;
}uart_param_t;

// 十六位整数转32位整数
void _int16_to_int32(uart_param_t* param) {
  if (sizeof(int) >= 4)
    return;
  unsigned long value = param->d_long;
  unsigned long sign = (value >> 15) & 1;
  unsigned long v = value;
  if (sign)
    v = 0xFFFF0000 | value;
  uart_param_t p;
  p.d_long = v;
  param->d_ucs[0] = p.d_ucs[0];
  param->d_ucs[1] = p.d_ucs[1];
  param->d_ucs[2] = p.d_ucs[2];
  param->d_ucs[3] = p.d_ucs[3];
}

// 浮点数转双精度 
void _float_to_double(uart_param_t* param) {
  if (sizeof(int) >= 4)
    return;
  unsigned long value = param->d_long;
  unsigned long sign = value >> 31;
  unsigned long M = value & 0x007FFFFF;
  unsigned long e =  ((value >> 23 ) & 0xFF) - 127 + 1023;
  uart_param_t p0, p1;  
  p1.d_long = ((sign & 1) << 31) | ((e & 0x7FF) << 20) | (M >> 3);
  p0.d_long = (M & 0x7) << 29;
  param->d_ucs[0] = p0.d_ucs[0];
  param->d_ucs[1] = p0.d_ucs[1];
  param->d_ucs[2] = p0.d_ucs[2];
  param->d_ucs[3] = p0.d_ucs[3];
  param->d_ucs[4] = p1.d_ucs[0];
  param->d_ucs[5] = p1.d_ucs[1];
  param->d_ucs[6] = p1.d_ucs[2];
  param->d_ucs[7] = p1.d_ucs[3];
}

// action: RX_0x01
void _uart_RX_0x01(unsigned char CountdownSet);

// action: RX_0x02
void _uart_RX_0x02(unsigned char TimeHor, unsigned char TimeMin);

// action: RX_0x03
void _uart_RX_0x03(unsigned char HeartRate, unsigned char Cadence);

// action: RX_0x04
void _uart_RX_0x04(unsigned char CountdownTenSec);

// action: RX_0x05
void _uart_RX_0x05(unsigned char Cadence);

// action: RX_0x06
void _uart_RX_0x06(unsigned char HeartRate);

// action: RX_0x07
void _uart_RX_0x07();

// action: RX_0x08
void _uart_RX_0x08();

// action: RX_0x09
void _uart_RX_0x09(unsigned char CS_KM_INT, unsigned char CS_KM_DEC, unsigned char CS_HOUR, unsigned char CS_MIN);

#ifdef __cplusplus
}
#endif

#endif /*JX_UART_SEND_H_*/

