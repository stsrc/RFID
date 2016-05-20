#include "esp8266.h"

#define ESP8266_RST_PORT	GPIOA
#define ESP8266_RST_PIN		GPIO_PIN_4

#define ESP8266_FW_PORT		GPIOA
#define ESP8266_FW_PIN		GPIO_PIN_8

#define AT_RESET_CMD		"AT+RST\r\n\0"
#define AT_CWMODE_1		"AT+CWMODE=1\r\n\0"
#define AT_PING_GOOGLE		"AT+PING=\"www.google.com\"\r\n\0"
#define AT_CIPMODE_0		"AT+CIPMODE=0\r\n\0"
#define AT_CIPMUX_1		"AT+CIPMUX=1\r\n\0"
#define AT_CIPSERVER		"AT+CIPSERVER=1,80\r\n\0"
#define AT_CIPSTO		"AT+CIPSTO=7000\r\n\0"
#define AT_CIFSR		"AT+CIFSR\r\n\0"
#define AT_CIPSEND		"AT+CIPSEND=\0"
#define AT_CLOSE_SOCKET		"AT+CIPCLOSE=\0"
#define AT_UART_CUR		"AT+UART_CUR=230400,8,1,0,0\r\n\0"

#define CHNL_STATE_OPENED	8
#define CHNL_STATE_CLOSED	4
#define CHNL_STATE_TRANSMIT	2
#define CHNL_STATE_CLEAR	1

#define HELP_BUF_SIZE 32

struct channel_data {
	char buf[5][HELP_BUF_SIZE];
	int8_t state[5];
	uint8_t reset;
};

static struct channel_data chn_data;
static uint8_t do_it = 0;

static void ClearChannel(const uint8_t id, const uint8_t state)
{
	chn_data.state[id] &= ~state;
}


static void SetChannel(const uint8_t id, const uint8_t state)
{
		chn_data.state[id] |= state;
}

static int8_t CheckChannel(const uint8_t id, const uint8_t state)
{
		return chn_data.state[id] & state;
}

int8_t esp8266_ScanForFile(char *file, uint8_t *id)
{
	do_it = 1;
	for (size_t i = 0; i < 5; i++) {
		if (CheckChannel(i, CHNL_STATE_TRANSMIT)) {
			strncpy(file, chn_data.buf[i], sizeof(chn_data.buf[i]));
			*id = i;
			return 0;
		}
	}
	return -EINVAL;	
}


static void SetChannelTransmit(char *buf, uint8_t id)
{
	memset(chn_data.buf[id], 0, sizeof(chn_data.buf[id]));
	strncpy(chn_data.buf[id], buf, HELP_BUF_SIZE - 1);
	SetChannel(id, CHNL_STATE_TRANSMIT);
}

void esp8266_InitPins() 
{
	GPIO_InitTypeDef init;
	init.Mode = GPIO_MODE_OUTPUT_PP;
	init.Pull = GPIO_PULLUP;
	init.Speed = GPIO_SPEED_FREQ_HIGH;
	init.Pin = ESP8266_RST_PIN | ESP8266_FW_PIN;
	__HAL_RCC_GPIOA_CLK_ENABLE();
	HAL_GPIO_Init(ESP8266_RST_PORT, &init);
	HAL_GPIO_WritePin(ESP8266_FW_PORT, ESP8266_FW_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_SET);
}

void esp8266_HardReset() 
{
	HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_RESET);
	delay_ms(1500);
	HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_SET);
	delay_ms(1000);	
}

int8_t esp8266_WaitForOk(const char *command, unsigned int delay, uint8_t 
			 multiplier) 
{
	int8_t ret;
	char buf[BUF_MEM_SIZE];
	ret = esp8266_GetReply(command, "OK\0", buf, delay, multiplier);
	return ret;	
}

int8_t esp8266_WaitForAck(const uint8_t id, const char *command, 
			  unsigned int delay, uint8_t multiplier) 
{
	int8_t ret;
	uint8_t cnt = 0;
	char buf[BUF_MEM_SIZE];
	do {
		ret = esp8266_GetReply(command, "OK\0", buf, 10, 0);
		if (!ret)
			return 0;
		
		if (ret) {
			ret = esp8266_GetReply(command, "FAIL\0", buf, 10, 0);
			if (!ret)
				return 1;
		}

		if (ret) {
			ret = esp8266_GetReply(command, "ERROR\0", buf, 10, 0);
			if (!ret)
				return 2;
		
		}
		
		if (ret)
			delay_ms(delay);
	
	} while ((ret == -EINVAL || ret == -EBUSY) && ++cnt < multiplier);
	return ret;	
}

static int8_t esp8266_ConnectToWiFi()
{
	int ret;
	esp8266_Send(AT_CWMODE_1, strlen(AT_CWMODE_1));	
	ret = esp8266_WaitForOk(AT_CWMODE_1, 100, 100);
	if (ret)
		return -1;
	esp8266_Send(WIFI_PASSWD_SECRET, strlen(WIFI_PASSWD_SECRET));
	ret = esp8266_WaitForOk(WIFI_PASSWD_SECRET, 100, 100);
	if (ret)
		return -2;
	esp8266_Send(AT_PING_GOOGLE, strlen(AT_PING_GOOGLE));
	ret = esp8266_WaitForOk(AT_PING_GOOGLE, 100, 100);
	if (ret)
		return -3;
	return 0;
}

int8_t esp8266_MakeAsServer()
{
	int8_t ret;
	ret = esp8266_Send(AT_CIPMODE_0, strlen(AT_CIPMODE_0));
	if (ret)
		return -1;
	ret = esp8266_WaitForOk(AT_CIPMODE_0, 100, 100);
	if (ret)
		return -2;
	ret = esp8266_Send(AT_CIPMUX_1, strlen(AT_CIPMUX_1));
	if (ret)
		return -3;
	ret = esp8266_WaitForOk(AT_CIPMUX_1, 100, 100);
	if (ret)
		return -4;
	ret = esp8266_Send(AT_CIPSERVER, strlen(AT_CIPSERVER));
	if (ret)
		return -5;
	ret = esp8266_WaitForOk(AT_CIPSERVER, 100, 100);
	if (ret)
		return -6;
	ret = esp8266_Send(AT_CIPSTO, strlen(AT_CIPSTO));
	if (ret)
		return -7;
	ret = esp8266_WaitForOk(AT_CIPSTO, 100, 100);
	if (ret)
		return -8;
	return 0;
}

int8_t esp8266_Init(char *global_buf) 
{
	int8_t ret;
	const char * const RST_CMD = AT_RESET_CMD;
	esp8266_InitPins();
	esp8266_HardReset();
	UART_2_init();
	buffer_Reset(&UART2_transmit_buffer);
	delay_ms(5000);
	esp8266_Send(RST_CMD, strlen(RST_CMD));
	delay_ms(5000);
	buffer_Reset(&UART2_receive_buffer);
	ret = esp8266_Send(AT_UART_CUR, strlen(AT_UART_CUR));
	if (ret)
		return -1;
	ret = esp8266_WaitForOk(AT_UART_CUR, 100, 100);
	UART_2_ChangeSpeed(230400);
	ret = esp8266_ConnectToWiFi();
	if (ret)
		return -2;
	return 0;
}

int8_t esp8266_Send(const char *data, size_t data_size) 
{
	int8_t ret;
	ret = buffer_set_text(&UART2_transmit_buffer, data, data_size);
	if (!ret)
		UART_2_transmit();
	return ret;
}

int8_t esp8266_SendGetReply(const char *command, const char *delimiter, 
			    char *output, unsigned int delay,
			    uint8_t multiplier)
{
	int8_t ret;
	if (buffer_IsFull(&UART2_transmit_buffer))
		return -ENOMEM;
	buffer_Reset(&UART2_receive_buffer);		
	esp8266_Send(command, strlen(command));
	ret = esp8266_GetReply(command, delimiter, output, delay, multiplier);	
	return ret;
}

int8_t esp8266_GetReply(const char *command, const char *delimiter, 
			char *output, unsigned int delay, uint8_t multiplier)
{
	int8_t ret, cnt = 0;
	do {
		ret = buffer_SearchGetLabel(&UART2_receive_buffer, command, 
					    delimiter, output);
		if (ret)
			delay_ms(delay);
	} while (((ret == -EBUSY) || (ret == -EINVAL)) && (++cnt < multiplier));
	return ret;
}

static inline void ParseDate(uint8_t *day, uint8_t *month, uint16_t *year, 
			     uint8_t *hour, uint8_t *minute, uint8_t *second, 
			     char *buf)
{
	unsigned short temp_h, temp_min, temp_sec, temp_day, temp_year;
	char temp_buf[16];
	char temp[5];
	memset(temp, 0, sizeof(temp));
	memset(temp_buf, 0, sizeof(temp_buf));
	sscanf(buf, "%s %hu %s %hu %hu:%hu:%hu", temp, &temp_day, temp_buf, 
	       &temp_year, &temp_h, &temp_min, &temp_sec);
	*hour = (uint8_t)temp_h;
	*minute = (uint8_t)temp_min;
	*second = (uint8_t)temp_sec;
	*day = (uint8_t)temp_day;
	*year = temp_year;

	if (!strcmp(temp_buf, "Jan"))
		*month = 1;
	else if (!strcmp(temp_buf, "Feb"))
		*month = 2;
	else if (!strcmp(temp_buf, "Mar"))
		*month = 3;
	else if (!strcmp(temp_buf, "Apr"))
		*month = 4;
	else if (!strcmp(temp_buf, "May"))
		*month = 5;
	else if (!strcmp(temp_buf, "Jun"))
		*month = 6;
	else if (!strcmp(temp_buf, "Jul"))
		*month = 7;
	else if (!strcmp(temp_buf, "Aug"))
		*month = 8;
	else if (!strcmp(temp_buf, "Sep"))
		*month = 9;
	else if (!strcmp(temp_buf, "Oct"))
		*month = 10;
	else if (!strcmp(temp_buf, "Nov"))
		*month = 11;
	else if (!strcmp(temp_buf, "Dec"))
		*month = 12;
	else 
		*month = 0;	
}	

int8_t esp8266_GetDate(uint8_t *day, uint8_t *month, uint16_t *year, 
		       uint8_t *hour, uint8_t *minute, uint8_t *second)
{
	char *http_connect = "AT+CIPSTART=\"TCP\",\"www.google.com\",80\r\n\0";
	char *http_disconnect = "AT+CIPCLOSE\r\n\0";
	char *http_command = "AT+CIPSEND=19\r\n\0";
	char *http_data = "HEAD / HTTP/1.1\r\n\r\n\0";
	int8_t ret = 0;
	char buf[BUF_MEM_SIZE];
	memset(buf, 0, BUF_MEM_SIZE);
	esp8266_Send(http_connect, strlen(http_connect));
	ret = esp8266_WaitForOk(http_connect, 100, 100);
	if (ret)
		return -1;
	esp8266_Send(http_command, strlen(http_command));
	ret = esp8266_WaitForOk(http_command, 100, 100);
	if (ret) 
		return -2;
	esp8266_Send(http_data, strlen(http_data));
	delay_ms(2000);
	ret = buffer_SearchGetLabel(&UART2_receive_buffer, "Date: \0", " GMT\0", buf);
	if (ret)
		return -3;
	ParseDate(day, month, year, hour, minute, second, buf);
	buffer_Reset(&UART2_receive_buffer);
	esp8266_Send(http_disconnect, strlen(http_disconnect));
	ret = esp8266_WaitForOk(http_disconnect, 100, 100);
	if (ret)
		return -4;
	return 0;
}

inline int8_t esp8266_WriteATCIPSEND(char *data, size_t data_size, uint8_t id) 
{
	char temp[32];
	char temp_2[16];
	int ret;
	memset(temp, 0, 32);
	memset(temp_2, 0, 16);
	sprintf(temp_2, "%u,%u\r\n", (unsigned int)id, (unsigned int)data_size); 
	strcpy(temp, AT_CIPSEND);
	strcat(temp, temp_2);

	ret = esp8266_Send(temp, strlen(temp));
	if (ret)
		return -2;

	ret = esp8266_WaitForAck(id, temp, 100, 100);
	if (ret)
		return -3;
	
	ret = esp8266_Send(data, data_size);
	if (ret)
		return -4;
	
	return 0;
}

int8_t esp8266_WriteATCIPCLOSE(char *buf, uint8_t id) 
{
	char temp[4];
	int ret;
	memset(temp, 0, sizeof(temp));
	memset(buf, 0, BUF_MEM_SIZE);
	sprintf(temp, "%u\r\n", id);
	strcpy(buf, AT_CLOSE_SOCKET);
	strcat(buf, temp);
	
	ret = esp8266_Send(buf, strlen(buf));
	if (ret)
		return -1;
	
	ret = esp8266_WaitForAck(id, buf, 100, 10);
	if (ret)
		ret = -2;

	return ret;
}

int8_t esp8266_WritePage(char *buf, size_t data_size, uint8_t id, uint8_t close)
{
	int8_t ret;
	ret = esp8266_WriteATCIPSEND(buf, data_size, id);
	if (ret)
		return -1;
	
	if (close) {
		ret = esp8266_WaitForAck(id, "SEND\0", 100, 100);
		if (ret)
			return -5;
		esp8266_WriteATCIPCLOSE(buf, id);
	}
	return 0;	
}

inline int8_t esp8266_GetIp(char *buf)
{
	memset(buf, 0, BUF_MEM_SIZE);
	return esp8266_SendGetReply(AT_CIFSR, "OK\0", buf, 100, 10);
}

int8_t esp8266_CheckResetFlag()
{
	int8_t ret = chn_data.reset;
	if (ret) {
		memset(&chn_data, 0, sizeof(struct channel_data));
		buffer_Reset(&UART2_receive_buffer);
	}
	return ret;
}

void esp8266_ClearResetFlag()
{
		chn_data.reset = 0;
}

static void MoveInsert(char *buffer, size_t size, uint8_t new_byte)
{
	for(size_t i = 0; i < size - 1; i++)
		buffer[i] = buffer[i+1];
	buffer[size-1] = new_byte;
}

static int8_t CompareLastBytes(char *buffer, size_t size, const char *to_compare) 
{
	size_t len = strlen(to_compare);
	size_t offset = size - len;
	int8_t ret = strncmp(buffer + offset, to_compare, len);
	return ret;	
}

static void MoveToSign(char *buffer, size_t size, char sign)
{
	while(buffer[0] != sign)
		MoveInsert(buffer, size, '\0');
}

static int8_t esp8266_state0(const uint8_t data, char *buf, 
			     const size_t buf_len, uint8_t *state)
{
	int8_t ret = 0;
	uint8_t id;
	const char *IPD = "+IPD,";
	const char *CLOSED = ",CLOSED";
	const char *CONNECT = ",CONNECT";

	switch(data) {

	case ',':
		ret = CompareLastBytes(buf, buf_len, IPD);
		if (!ret)
			*state = 1;
		break;

	case 'D':
		ret = CompareLastBytes(buf, buf_len, CLOSED);
		if (!ret) {
			id = *(buf + (buf_len - 1 - strlen(CLOSED))) 
			     - 48; 
			ClearChannel(id, CHNL_STATE_TRANSMIT);
		} 
		break;

	case 'T':
		ret = CompareLastBytes(buf, buf_len, CONNECT);
		if (!ret) {
			id = *(buf + (buf_len - 1 - strlen(CONNECT))) 
			     - 48;
		} 
		break;
	
	default:
		ret = -1;
	}

	return ret;
}

static int8_t esp8266_state1(const uint8_t data, char *buf, const size_t buf_len,
			     uint8_t *state, uint8_t *cnt)
{
	int8_t ret;
	uint16_t id, len;
	size_t s_len;
	char file[HELP_BUF_SIZE];

	if (!(data == 'P'))
		return -1;
	
	ret = CompareLastBytes(buf, buf_len, " HTTP");	
	if (ret) {
		return -1;
	}

	MoveToSign(buf, buf_len, '+');
	memset(file, 0, sizeof(file));
	ret = sscanf(buf, "+IPD,%hu,%hu:GET /%s HTTP", &id, &len, file);
	if (ret != 3) {
		*state = 0;
		return -1;
	}
	s_len = strlen(file);
	if (!strcmp(file, "HTTP"))
		strcpy(file, "index.html");
	SetChannelTransmit(file, id);

	if (len > s_len)	
		len -= s_len;

	if (len > 30)
		len -= 30;

	buffer_SetIgnore(&UART2_receive_buffer, len); //27
	memset(buf, 0, buf_len);
	*state = 0;
	return 0;
}

static int8_t esp8266_CheckErrorsOnInput(const uint8_t data, uint8_t *state, 
					 char *buf, const size_t len)
{
	static size_t test = 0;
	
	if ((data != '\r') && (data != '\n') && (data != '\0')) {
		if ((data < ' ') || (data > '~'))
			test++;
		else
			test = 0;
	}
	
	if (test == 3) {
		chn_data.reset = 1;
		test = 0;
		*state = 0;
		memset(buf, 0, len);
		return 1;
	}
	
	return 0;
}

static int8_t esp8266_CheckReset(uint8_t *state, char *buf, const size_t len)
{
	int8_t ret;
	ret = CompareLastBytes(buf, sizeof(buf), "rst cause");
	if (!ret) {
		chn_data.reset = 1;
		memset(buf, 0, len);
		*state = 0;
		return 1;
	}
	return 0;
}

void esp8266_CheckInput(uint8_t data)
{
	static uint8_t state = 0;
	static uint8_t cnt = 0;
	static char buf[2 * HELP_BUF_SIZE];

	int8_t ret;
	if (!do_it)
		return;
	
	MoveInsert(buf, sizeof(buf), data);
	
	ret = esp8266_CheckErrorsOnInput(data, &state, buf, sizeof(buf));
	if (ret) { 
		cnt = 0;
		return;
	}

	ret = esp8266_CheckReset(&state, buf, sizeof(buf));
	if (ret) {
		cnt = 0;
		return;
	}

	switch(state){
	case 0:
		ret = esp8266_state0(data, buf, sizeof(buf), &state);
		if (!ret)
			return;
		break;
	case 1:
		ret = esp8266_state1(data, buf, sizeof(buf), &state, &cnt);
		if (!ret)
			return;
		break;
	}	
}
