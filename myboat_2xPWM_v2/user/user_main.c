#include <stddef.h>
#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "espconn.h"
#include "mystuff.h"
#include "driver/gpio16.h"
#include <stdlib.h>
#include "driver/pwm.h"

#define PORT 28800
#define SERVER_TIMEOUT 28800
#define MAX_CONNS 1

/* PINS:
	LED       - GPIO2 - PIN 4
	PWMA      - GPIO4 - PIN 2
	PWMB      - GPIO13- PIN 7
	AIN1/BIN1 - GPIO5 - PIN 1
	AIN2/BIN2 - GPIO12- PIN 6
	STBY      - GPIO14- PIN 5

*/
#define GPIO_LED_PIN 4
#define GPIO_AIN1_PIN 1
#define GPIO_AIN2_PIN 6
#define GPIO_STBY_PIN 5

#define PWM_CHANNELS 2
const uint32_t period = 5000; // * 200ns ^= 1 kHz
uint32_t io_info[PWM_CHANNELS][3] = {
    // MUX, FUNC, PIN
    {PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4 ,  4},
    {PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13, 13}
};
// initial duty: all off
uint32_t pwm_duty_c[PWM_CHANNELS] = {0};
uint32 light_blink = 0;
int32 light_on = 0;
uint32 dir = 0;

/* timers */
static volatile os_timer_t batt_timer;
static volatile os_timer_t conn_timer;
static volatile os_timer_t servo_timer;
/* UDP server */
static struct espconn *pUdpServer;


#define DIRECT_MODE_OUTPUT(pin)    platform_gpio_mode(pin,PLATFORM_GPIO_OUTPUT,PLATFORM_GPIO_PULLUP)
#define DIRECT_WRITE_LOW(pin)    (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 0))
#define DIRECT_WRITE_HIGH(pin)   (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 1))

void user_rf_pre_init(void)
{
	//nothing.
}
//===========================================================================================

static void ICACHE_FLASH_ATTR connTimer(void *arg)
{
	/* Disable PWM ? */
	pwm_duty_c[0] = 0;
	pwm_duty_c[1] = 0;
	DIRECT_WRITE_LOW(GPIO_STBY_PIN);
	/* Blink Led */
	os_timer_disarm(&conn_timer);
	os_timer_setfn(&conn_timer, (os_timer_func_t *)connTimer, NULL);
	if (light_blink&1) {
		DIRECT_WRITE_LOW(GPIO_LED_PIN);
		os_timer_arm(&conn_timer, 950, 1);
	} else {
		DIRECT_WRITE_HIGH(GPIO_LED_PIN);
		os_timer_arm(&conn_timer, 50, 1);
	}
	light_blink++;
}
//===========================================================================================

static const int __bm_vtable[11] = { 4200, 4100, 4000, 3900, 3800, 3700, 3600, 3500, 3400, 3300, 3200 };
static const int __bm_ptable[11] = {  100,  94,   83,   72,   59,   50,   33,   15,   6,     0,   0  };

int ICACHE_FLASH_ATTR bat_interpolate(int x)
{
	int n = 11;
	int const *tabx = __bm_vtable;
	int const *taby = __bm_ptable;
	int index;
	int y;

	if (x >= tabx[0]) {
		y = taby[0];
	} else if (x <= tabx[n - 1]) {
		y = taby[n - 1];
	} else {
		for (index = 1; index < n; index++)
		if (x > tabx[index]) break;
		/*  interpolate */
		y = (taby[index - 1] - taby[index]) * (x - tabx[index]) /(tabx[index - 1] - tabx[index]);
		y += taby[index];
	}
	return y;
}
//===========================================================================================

static void ICACHE_FLASH_ATTR battTimer(void *arg)
{
	uint16	val = system_adc_read();
	char out[50];
	uint32_t ip =  ipaddr_addr("192.168.1.100");
	int volt = (((int)val) * ((int)4120))/((int)857);
	int per = bat_interpolate(volt);
	ets_sprintf(out,"adu=%d, volt=%dmV, per=%d%\n",val, volt, per);
	uart0_sendStr(out);
	ets_sprintf(out,"bat %d\nvolt %d\nboattype 2\n",per,volt);
	pUdpServer->proto.udp->remote_port = PORT;
	os_memcpy(pUdpServer->proto.udp->remote_ip, &ip, 4);
	espconn_sent( pUdpServer, out, os_strlen(out) );
}
//===========================================================================================

// A simple atoi() function
static int ICACHE_FLASH_ATTR myAtoi(const char *str, int len)
{
	int i, res = 0; // Initialize result
	char ch;
	// Iterate through all characters of input string and
	// update result
	for (i = 0; i<len; ++i) {
		ch = str[i];
		if ((ch >='0') && (ch <= '9')) {
			res = res*10 + (str[i] - '0');
		}
	}
	// return result.
	return res;
}
//===========================================================================================

static int32_t dm[2] = {0};

//Called when new packet comes in.
static void ICACHE_FLASH_ATTR udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	struct espconn *pespconn = (struct espconn *)arg;
	uint8_t ch,pin;
	int32_t val,t;
	int i;
	char *pt = pusrdata;
	char *pt_end = pusrdata + len;

	os_timer_disarm(&conn_timer);
	os_timer_setfn(&conn_timer, (os_timer_func_t *)connTimer, NULL);
	light_blink = 0;
	os_timer_arm(&conn_timer, 500, 1);
	DIRECT_WRITE_HIGH(GPIO_STBY_PIN);

	//Seems to be optional, still can cause crashes.

	while (pt < pt_end) {
		/* Find the End */
		while ((*pt != '\n') && (pt < pt_end)) pt++;
		/* Execute command */
		len = (pt - pusrdata);	
		if ((pusrdata[0] == 'c') && (len > 4)) {
			ch = pusrdata[2] - '0';
			if (ch<2) {
				val = myAtoi(pusrdata + 4, len-4);
				if ((pusrdata[1] == 'p') && (val >=0) && (val < 1024)) {
					val = (val)*5000/1023;
					if (val != pwm_duty_c[ch]) {
						pwm_duty_c[ch] = val;
					}
				}
				if ((pusrdata[1] == 'x') && (val >=0) && (val < 3000)) {
					if (ch == 0) {dm[0]=(val)*5000/1023;} else {dm[1]=val;}
					if (dm[1] > 1000) {
						val=((dm[1]-1000)*dm[0])/1000;
						pwm_duty_c[0]=(dm[0]-val);
						pwm_duty_c[1]=(dm[0]);
					} else {
						val=((1000-dm[1])*dm[0])/1000;
						pwm_duty_c[0]=(dm[0]);
						pwm_duty_c[1]=(dm[0]-val);
					}
				}
				if (pusrdata[1] == 'l') {
					/* Light control */
					light_on = val;
				}
				if (pusrdata[1] == 'd') {
					/* Motor direction */
					dir = val;
				}
			}
		}
		pt++;
		pusrdata = pt;
	}
	if (light_on) {DIRECT_WRITE_HIGH(GPIO_LED_PIN);} else {DIRECT_WRITE_LOW(GPIO_LED_PIN);}
}
//===========================================================================================

static void ICACHE_FLASH_ATTR servoTimer(void *arg)
{
	pwm_set_duty(pwm_duty_c[0], 0);
	pwm_set_duty(pwm_duty_c[1], 1);
	pwm_start();
	if (dir) {
		DIRECT_WRITE_LOW(GPIO_AIN2_PIN);
		DIRECT_WRITE_HIGH(GPIO_AIN1_PIN);
	} else {
		DIRECT_WRITE_LOW(GPIO_AIN1_PIN);
		DIRECT_WRITE_HIGH(GPIO_AIN2_PIN);
	}
}
//===========================================================================================


void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}
//===========================================================================================

void ICACHE_FLASH_ATTR wifiInit()
{
	struct ip_info ipinfo;
	struct softap_config config;
	struct dhcps_lease dhcp_lease;
	size_t len;


	if( wifi_get_phy_mode() != PHY_MODE_11B ) {
		wifi_set_phy_mode( PHY_MODE_11B );
	}

	if (wifi_get_opmode() != SOFTAP_MODE) {
		wifi_set_opmode(SOFTAP_MODE);
		//after esp_iot_sdk_v0.9.2, need not to restart
		system_restart();
	}


	/* Config */
	wifi_softap_get_config(&config);
	os_memset(config.ssid, 0, sizeof(config.ssid));
	os_memset(config.password, 0, sizeof(config.password));
	os_memcpy(config.ssid, "myboat", 6);
	config.ssid_len = 6;
	config.ssid_hidden = 0;
	os_memcpy(config.password, "rctymek123", 10);
	config.authmode = AUTH_WPA_WPA2_PSK;
	config.channel = 7;
	config.max_connection = MAX_CONNS;
	config.beacon_interval = 100;
	wifi_softap_set_config(&config);

	/* Set IP */
	wifi_softap_dhcps_stop();
	IP4_ADDR(&ipinfo.ip, 192, 168, 1, 25);
	IP4_ADDR(&ipinfo.gw, 192, 168, 1, 1);
	IP4_ADDR(&ipinfo.netmask, 255, 255, 255, 0);
	wifi_set_ip_info(SOFTAP_IF, &ipinfo);


	//wifi_softap_get_dhcps_lease(&dhcp_lease);
	dhcp_lease.enable = 1;
	IP4_ADDR(&dhcp_lease.start_ip, 192, 168, 1, 100);
	IP4_ADDR(&dhcp_lease.end_ip, 192, 168, 1, 103);
	wifi_softap_set_dhcps_lease(&dhcp_lease);

	os_delay_us(500000);
	wifi_softap_dhcps_start();
}
//===========================================================================================

static void ICACHE_FLASH_ATTR system_init_done(void)
{
	uint32_t ip =  ipaddr_addr("192.168.1.100");
	// Connect to Wifi Station
	uart0_sendStr("System Initialized\r\n");
	pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
	espconn_create( pUdpServer );
	pUdpServer->type = ESPCONN_UDP;
	pUdpServer->state = ESPCONN_NONE;
	pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pUdpServer->proto.udp->local_port = PORT;
	pUdpServer->proto.udp->remote_port = PORT;
	os_memcpy(pUdpServer->proto.udp->remote_ip, &ip, 4);
	espconn_regist_recvcb(pUdpServer, udpserver_recv);
	if( espconn_create( pUdpServer ) ) {
		while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }
	}
}
//===========================================================================================

uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void)
{
	enum flash_size_map size_map = system_get_flash_size_map();
	uint32 rf_cal_sec = 0;
	switch (size_map) {
		case FLASH_SIZE_4M_MAP_256_256:
			rf_cal_sec = 128 - 5;
		break;

		case FLASH_SIZE_8M_MAP_512_512:
			rf_cal_sec = 256 - 5;
		break;

		case FLASH_SIZE_16M_MAP_512_512:
		case FLASH_SIZE_16M_MAP_1024_1024:
			rf_cal_sec = 512 - 5;
		break;

		case FLASH_SIZE_32M_MAP_512_512:
		case FLASH_SIZE_32M_MAP_1024_1024:
			rf_cal_sec = 1024 - 5;
		break;

		default:
			rf_cal_sec = 0;
		break;
	}
	return rf_cal_sec;
}
//===========================================================================================

//void ets_update_cpu_frequency (uint32_t mhz);
//#define CPU160MHZ 160

void user_init(void)
{
	int i;
	ets_wdt_disable();

	//REG_SET_BIT(0x3ff00014, BIT(0));
	//ets_update_cpu_frequency(CPU160MHZ);

	gpio_init();
	uart_init(BIT_RATE_74880, BIT_RATE_74880);
	uart0_sendStr("\r\nBoat 2xPWM UDP Server\r\n");

	/* Setup gpio */
	set_gpio_mode(GPIO_LED_PIN, GPIO_PULLUP, GPIO_OUTPUT);gpio_write(GPIO_LED_PIN, 1);
	set_gpio_mode(GPIO_STBY_PIN, GPIO_PULLUP, GPIO_OUTPUT);gpio_write(GPIO_STBY_PIN, 0);
	set_gpio_mode(GPIO_AIN1_PIN, GPIO_PULLUP, GPIO_OUTPUT);gpio_write(GPIO_AIN1_PIN, 1);
	set_gpio_mode(GPIO_AIN2_PIN, GPIO_PULLUP, GPIO_OUTPUT);gpio_write(GPIO_AIN2_PIN, 0);

	/* Setup PWM's */
	pwm_init(period, pwm_duty_c, PWM_CHANNELS, io_info);
	pwm_start();

	/* Init wifi */
	wifiInit();

	uart0_sendStr("\r\nBooting\r\n");

	//Setup Timers
	os_timer_disarm(&servo_timer);
	os_timer_setfn(&servo_timer, (os_timer_func_t *)servoTimer, NULL);
	os_timer_arm(&servo_timer, 20, 1);

	os_timer_disarm(&batt_timer);
	os_timer_setfn(&batt_timer, (os_timer_func_t *)battTimer, NULL);
	os_timer_arm(&batt_timer, 1000, 1);

	os_timer_disarm(&conn_timer);
	os_timer_setfn(&conn_timer, (os_timer_func_t *)connTimer, NULL);
	os_timer_arm(&conn_timer, 500, 1);
	
	system_init_done_cb(&system_init_done);
}
//===========================================================================================

