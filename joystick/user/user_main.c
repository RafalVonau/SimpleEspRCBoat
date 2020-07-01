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

/* PINS !! (
    LED      - GPIO2,
    MUXA     - GPIO5,
    MUXB     - GPIO4,
    MUXC     - GPIO15,
    BAT_LED1 - GPIO16,
    BAT_LED2 - GPIO14,
    BUTTON0  - GPIO12,
    BUTTON1  - GPIO13,
) */

/* Analog multilpexer CD4051:
   Channel 0 - X1,
   Channel 1 - Unused,
   Channel 2 - Unused,
   Channel 3 - Y,
   Channel 4 - Y1,
   Channel 5 - Reference for potentiometers (maximum value).
   Channel 6 - X,
   Channel 7 - Battery (R1 = 20k, R2 = 5k)
*/

#define GPIO_LED_PIN      4
#define GPIO_MUXA_PIN     1
#define GPIO_MUXB_PIN     2
#define GPIO_MUXC_PIN     8
#define GPIO_BAT_LED1_PIN 0
#define GPIO_BAT_LED2_PIN 5
#define GPIO_BUTTON1_PIN  6
#define GPIO_BUTTON2_PIN  7

#define DEBUG_ENABLE 1

#define ADC_CHANNEL_X      0
#define ADC_CHANNEL_Y      1
#define ADC_CHANNEL_BAT    2
#define ADC_CHANNEL_REF    3

#define ADC_CHANNELS       8
#define ZERO_S             20


static volatile uint32 channel     = 0;
static volatile uint32 adc_callibration = 0;
static volatile uint32 adc_res[ADC_CHANNELS];
static volatile uint32 adc_cal[ADC_CHANNELS];
//uint32 adc_map[ADC_CHANNELS] = {0,1,2,3,4,5,6,7};
const uint32 adc_map[ADC_CHANNELS] = {6,3,7,5,0,0,0,0};

static volatile uint32 light_blink = 0;
static volatile uint32 boatType    = 2;
static volatile uint32 batCounter  = 0;

static volatile uint32 boat_bat_percent = 0;
static volatile uint32 joystick_bat_percent = 0;
static volatile uint32 bat_led_blink = 0;
static volatile uint32 boat_light = 0;

/* timers */
static volatile os_timer_t adc_timer;
static volatile os_timer_t conn_timer;
static volatile os_timer_t bat_led_timer;
/* UDP */
static struct espconn *pUdpServer;

#define DIRECT_MODE_OUTPUT(pin)    platform_gpio_mode(pin,PLATFORM_GPIO_OUTPUT,PLATFORM_GPIO_PULLUP)
#define DIRECT_WRITE_LOW(pin)    (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 0))
#define DIRECT_WRITE_HIGH(pin)   (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 1))
#define DIRECT_WRITE_VAL(pin,v)   (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), v))


#if DEBUG_ENABLE
#define JDEBUG( ... ) {os_sprintf( generic_print_buffer, __VA_ARGS__ );  uart0_sendStr( generic_print_buffer );}
#else
#define JDEBUG( ... )
#endif

static void ICACHE_FLASH_ATTR calibrationTimerFn(void *arg);
static void ICACHE_FLASH_ATTR adcTimerFn(void *arg);


void user_rf_pre_init(void)
{
	//nothing.
}
//===========================================================================================

void ICACHE_FLASH_ATTR intr_callback(unsigned pin, unsigned level)
{
	JDEBUG("INTERRUPT: GPIO%d = %d\r\n", pin_num[pin], level);
	if (pin == 6) {
		boat_light = boat_light?0:1;
	}
}
//===========================================================================================

/*!
 * \brief Set analog mux (0-7).
 */
static void ICACHE_FLASH_ATTR setMux(uint32_t channel)
{
	DIRECT_WRITE_VAL(GPIO_MUXA_PIN, channel&1);
	DIRECT_WRITE_VAL(GPIO_MUXB_PIN, (channel>>1)&1);
	DIRECT_WRITE_VAL(GPIO_MUXC_PIN, (channel>>2)&1);
}
//===========================================================================================

/*!
 * \brief Battery status leds timer - drive battery status leds.
 */
static void ICACHE_FLASH_ATTR batLedTimerFn(void *arg)
{
	/* Boat battery */
	if (boat_bat_percent>50) {
		gpio_write(GPIO_BAT_LED1_PIN,0);
	} else if (boat_bat_percent<20) {
		gpio_write(GPIO_BAT_LED1_PIN,1);
	} else {
		gpio_write(GPIO_BAT_LED1_PIN,bat_led_blink&1);
	}
	/* Joystick battery */
	if (joystick_bat_percent>50) {
		gpio_write(GPIO_BAT_LED2_PIN,0);
	} else if (joystick_bat_percent<20) {
		gpio_write(GPIO_BAT_LED2_PIN,1);
	} else {
		gpio_write(GPIO_BAT_LED2_PIN,bat_led_blink&1);
	}
	bat_led_blink++;
}
//===========================================================================================

/*!
 * \brief Connection timeout timmer.
 */
static void ICACHE_FLASH_ATTR connTimer(void *arg)
{
	/* Blink Led */
	os_timer_disarm(&conn_timer);
	os_timer_setfn(&conn_timer, (os_timer_func_t *)connTimer, NULL);
	if (light_blink&1) {
		DIRECT_WRITE_HIGH(GPIO_LED_PIN);
		os_timer_arm(&conn_timer, 950, 1);
	} else {
		DIRECT_WRITE_LOW(GPIO_LED_PIN);
		os_timer_arm(&conn_timer, 50, 1);
	}
	light_blink++;
	boat_bat_percent = 0;
}
//===========================================================================================


static char adc_buffer[200];
#define adc_printf( ... ) os_sprintf( adc_buffer, __VA_ARGS__ );


/*!
 * \brief Obtain calibration data.
 */
static void ICACHE_FLASH_ATTR calibrationTimerFn(void *arg)
{
	uint16	val;
	uint32_t i;
	
	val = system_adc_read();
	adc_res[channel] = val;
	//JDEBUG("Channel %d = %d\n",channel, val);
	channel++;
	if (channel >= 4) {
		channel = 0;
		setMux(adc_map[channel]);
		/* ADC zero callibration */
		if (adc_callibration == 0) {
			for (i=0;i<ADC_CHANNELS;++i) adc_cal[i] = adc_res[i];
			adc_callibration = 1;
			return;
		} else if (adc_callibration < 8) {
			for (i=0;i<ADC_CHANNELS;++i) adc_cal[i]+=adc_res[i];
			adc_callibration++;
			if (adc_callibration == 8) {
				for (i=0;i<ADC_CHANNELS;++i) adc_cal[i]>>=3;
				adc_res[ADC_CHANNEL_REF] = adc_cal[ADC_CHANNEL_REF];
				JDEBUG("Calibration ready X0=%d, Y0= %d, MAX=%d\n",adc_cal[ADC_CHANNEL_X], adc_cal[ADC_CHANNEL_Y], adc_res[ADC_CHANNEL_REF]);
				os_timer_disarm(&adc_timer);
				os_timer_setfn(&adc_timer, (os_timer_func_t *)adcTimerFn, NULL);
				os_timer_arm(&adc_timer, 20, 1);
			}
		}
	} else {
		setMux(adc_map[channel]);
	}
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

/*!
 * \brief Obtain calibration data.
 */
static void ICACHE_FLASH_ATTR measureBatteryFn(void *arg)
{
	uint16	val;
	int volt;
	
	val = system_adc_read();
	channel = 0;
	setMux(adc_map[channel]);

	volt = (((int)val) * ((int)4130))/((int)880);
	joystick_bat_percent = bat_interpolate(volt);
	JDEBUG("JoyStick bat voltage = %d, percent = %d\n",volt, joystick_bat_percent);
	
	os_timer_disarm(&adc_timer);
	os_timer_setfn(&adc_timer, (os_timer_func_t *)adcTimerFn, NULL);
	os_timer_arm(&adc_timer, 20, 1);
}
//===========================================================================================


int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
//===========================================================================================



/*!
 * \brief ADC Measurement timer.
 */
static void ICACHE_FLASH_ATTR adcTimerFn(void *arg)
{
	uint32_t i, ip=ipaddr_addr("192.168.1.25");
	uint16	val = system_adc_read();
	int32_t vx,vy,dir,dx,dy;
	
	adc_res[channel] = val;
	//JDEBUG("Channel %d = %d\n",channel, val);
	channel++;
	if (channel >= 2) {
		channel = 0;
		setMux(adc_map[channel]);
		
		if (batCounter > 25) {
			batCounter = 0;
			/* Time to measure battery */
			channel = ADC_CHANNEL_BAT;
			setMux(adc_map[channel]);
			os_timer_disarm(&adc_timer);
			os_timer_setfn(&adc_timer, (os_timer_func_t *)measureBatteryFn, NULL);
			os_timer_arm(&adc_timer, 20, 1);
		} else {
			batCounter++;
		}
		
		pUdpServer->proto.udp->remote_port = PORT;
		os_memcpy(pUdpServer->proto.udp->remote_ip, &ip, 4);
		adc_printf("\n");
		vx = adc_res[ADC_CHANNEL_X];
		vy = adc_res[ADC_CHANNEL_Y];
		dx = vx - adc_cal[ADC_CHANNEL_X];
		dy = vy - adc_cal[ADC_CHANNEL_Y];
		/* Zero insense */
		if ((dx < ZERO_S) && (dx > -ZERO_S)) dx = adc_cal[ADC_CHANNEL_X];
		if ((dy < ZERO_S) && (dy > -ZERO_S)) dy = adc_cal[ADC_CHANNEL_Y];
		switch (boatType) {
			case 1: {
				/* Typ 1 - 1xPWM, 1XServo, 1xdir, 1xlight */
				if (vx >= adc_cal[ADC_CHANNEL_X]) {
					dir = 0;
					vx = map(vx, adc_cal[ADC_CHANNEL_X], adc_res[ADC_CHANNEL_REF], 0, 1023);
				} else {
					dir = 1;
					vx = map(vx, adc_cal[ADC_CHANNEL_X], 0, 0, 1024);
				}
				if (vy >= adc_cal[ADC_CHANNEL_Y]) {
					vy = map(vy, adc_cal[ADC_CHANNEL_Y], adc_res[ADC_CHANNEL_REF], 1500, 2000);
				} else {
					vy = map(vy, adc_cal[ADC_CHANNEL_Y], 0, 1500, 1000);
				}
				//String requestmsg = "cp0 "+Integer.toString(cmd.getVal0()) + "\nch0 " + Integer.toString(1000 + ((cmd.getVal1()*1000)/1023)) + "\ncd0 " + Integer.toString(dir) + "
				adc_printf("cp0 %d\nch0 %d\ncd0 %d\ncl0 %d\n",vx,vy,dir,boat_light);
			} break;
			case 2: {
				/* Typ 2 - 2xPWM (cx0 - Motor speed (0-1023), cx1 - rotation (0-2000) */
				if (vx >= adc_cal[ADC_CHANNEL_X]) {
					dir = 0;
					vx = map(vx, adc_cal[ADC_CHANNEL_X], adc_res[ADC_CHANNEL_REF], 0, 1023);
				} else {
					dir = 1;
					vx = map(vx, adc_cal[ADC_CHANNEL_X], 0, 0, 1024);
				}
				if (vy >= adc_cal[ADC_CHANNEL_Y]) {
					vy = map(vy, adc_cal[ADC_CHANNEL_Y], adc_res[ADC_CHANNEL_REF], 1000, 0);
				} else {
					vy = map(vy, adc_cal[ADC_CHANNEL_Y], 0, 1000, 2000);
				}
				//String requestmsg = "cx0 "+Integer.toString(cmd.getVal0()) + "\ncx1 " + Integer.toString((cmd.getVal1()*2000)/1023) + "\n";
				adc_printf("cx0 %d\ncx1 %d\ncd0 %d\ncl0 %d\n",vx,vy,dir,boat_light);
				if (batCounter == 0) JDEBUG("vx = %d, vy=%d => cx0 %d, cx1 %d cd0 %d cl0 %d\n",adc_res[ADC_CHANNEL_X], adc_res[ADC_CHANNEL_Y], vx,vy,dir,boat_light);
			} break;
			case 3: {
				/* Typ 3 - 2xServo (Speed limited to 50%) (ch0 - Motor speed (1000-1500), ch2 - rotation (1000-2000) */
				if (vx >= adc_cal[ADC_CHANNEL_X]) {
					dir = 0;
					vx = map(vx, adc_cal[ADC_CHANNEL_X], adc_res[ADC_CHANNEL_REF], 1000, 1500);
				} else {
					dir = 1;
					vx = 1000;
				}
				if (vy >= adc_cal[ADC_CHANNEL_Y]) {
					vy = map(vy, adc_cal[ADC_CHANNEL_Y], adc_res[ADC_CHANNEL_REF], 1500, 2000);
				} else {
					vy = map(vy, adc_cal[ADC_CHANNEL_Y], 0, 1500, 1000);
				}
				//String requestmsg = "ch0 "+Integer.toString(1000 + ((cmd.getVal0()*500)/1023)) + "\nch1 " + Integer.toString(1000 + ((cmd.getVal1()*1000)/1023)) + "\n";
				adc_printf("ch0 %d\nch1 %d\ncd0 %d\ncl0 %d\n",vx,vy,dir,boat_light);
			} break;
			default:break;
		}
		espconn_sent( pUdpServer, adc_buffer, os_strlen(adc_buffer) );
	} else {
		setMux(adc_map[channel]);
	}
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

/*!
 * \brief UDP packet received.
 */
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
	os_timer_arm(&conn_timer, 1500, 1);
	DIRECT_WRITE_LOW(GPIO_LED_PIN);

	while (pt < pt_end) {
		/* Find the End */
		while ((*pt != '\n') && (pt < pt_end)) pt++;
		len = (pt - pusrdata);
		if (len > 4) {
			/* bat */
			if ((pusrdata[0] == 'b') && (pusrdata[1] == 'a') && (pusrdata[2] == 't')) {
				val = myAtoi(pusrdata + 4, len-4);
				JDEBUG("Got battery percent = %d [%]\n",val);
				boat_bat_percent = val;
			}
			/* volt */
			if ((pusrdata[0] == 'v') && (pusrdata[1] == 'o') && (pusrdata[2] == 'l') && (pusrdata[3] == 't')) {
				val = myAtoi(pusrdata + 5, len-5);
				//JDEBUG("Got battery voltage = %d [mV]\n",val);
			}
			/* boattype */
			if ((pusrdata[0] == 'b') && (pusrdata[1] == 'o') && (pusrdata[2] == 'a') && (pusrdata[3] == 't') && (len > 9)) {
				boatType = myAtoi(pusrdata + 9, len-9);
				//JDEBUG("Got boat type = %d\n",boatType);
			}
		}
		pt++;
		pusrdata = pt;
	}
}
//===========================================================================================



void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}
//===========================================================================================

/*!
 * Check Station
 *
 * Check that we've successfully entered station mode.
 */
static void ICACHE_FLASH_ATTR check_station(void *p)
{
	int i;
	char has_ip = 0;
	struct ip_info ip;
	uint8_t currChan;

	(void)p;

	os_memset(&ip, 0, sizeof(struct ip_info));
	wifi_get_ip_info(STATION_IF, &ip);
	for (i = 0; i < sizeof(struct ip_info); ++i) {has_ip |= ((char *) &ip)[i];}
	currChan = wifi_get_channel();
	if (has_ip == 0) {
		/* No IP Address yet, so check the reported status */
		uint8_t curr_status = wifi_station_get_connect_status();
		JDEBUG("status=%d,chan=%d\n", curr_status, currChan);
		if (curr_status == 2 || curr_status == 3 || curr_status == 4) {
		}
		return;
	}
	//os_sprintf (state->ipaddr, "%d.%d.%d.%d", IP2STR(&ip.ip.addr));
	//state->success = 1;
}
//===========================================================================================


const char ssid[32] = "myboat";
const char password[32] = "rctymek123";

/*!
 * \brief Initialize Wifi - connect to myboat network.
 */
void ICACHE_FLASH_ATTR wifiInit()
{
	struct ip_info ipinfo;
	struct station_config stationConf;
	struct dhcps_lease dhcp_lease;
	size_t len;

	if( wifi_get_phy_mode() != PHY_MODE_11B ) {wifi_set_phy_mode( PHY_MODE_11B );}
	if (wifi_get_opmode() != STATION_MODE) {
		wifi_set_opmode(STATION_MODE);
		//after esp_iot_sdk_v0.9.2, need not to restart
		system_restart();
	}
	/* Config */
	os_memcpy(&stationConf.ssid, ssid, 32);
	os_memcpy(&stationConf.password, password, 32);
	wifi_station_set_config(&stationConf);
}
//===========================================================================================

static void ICACHE_FLASH_ATTR system_init_done(void)
{
	uint32_t ip =  ipaddr_addr("192.168.1.25");
	// Create UDP server
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
	if( espconn_create( pUdpServer ) ) {while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }}
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
	ets_wdt_disable();

	//REG_SET_BIT(0x3ff00014, BIT(0));
	//ets_update_cpu_frequency(CPU160MHZ);

	gpio_init();
	uart_init(BIT_RATE_74880, BIT_RATE_74880);
	uart0_sendStr("\r\nJoyStick\r\n");

	/* Setup led */
	set_gpio_mode(GPIO_LED_PIN, GPIO_PULLUP, GPIO_OUTPUT);
	gpio_write(GPIO_LED_PIN, 1);
	set_gpio_mode(GPIO_MUXA_PIN, GPIO_PULLUP, GPIO_OUTPUT);
	set_gpio_mode(GPIO_MUXB_PIN, GPIO_PULLUP, GPIO_OUTPUT);
	set_gpio_mode(GPIO_MUXC_PIN, GPIO_PULLUP, GPIO_OUTPUT);


	set_gpio_mode(GPIO_BAT_LED1_PIN, GPIO_PULLUP, GPIO_OUTPUT);
	gpio_write(GPIO_BAT_LED1_PIN, 1);
	set_gpio_mode(GPIO_BAT_LED2_PIN, GPIO_PULLUP, GPIO_OUTPUT);
	gpio_write(GPIO_BAT_LED2_PIN, 1);


	if (set_gpio_mode(GPIO_BUTTON1_PIN, GPIO_PULLUP, GPIO_INT)) {
		if (gpio_intr_init(GPIO_BUTTON1_PIN, GPIO_PIN_INTR_NEGEDGE)) {
			gpio_intr_attach(intr_callback);
		}
	}

	if (set_gpio_mode(GPIO_BUTTON2_PIN, GPIO_PULLUP, GPIO_INT)) {
		if (gpio_intr_init(GPIO_BUTTON2_PIN, GPIO_PIN_INTR_NEGEDGE)) {
			gpio_intr_attach(intr_callback);
		}
	}

	//set_gpio_mode(GPIO_BUTTON1_PIN, GPIO_PULLUP, GPIO_INPUT);
	//set_gpio_mode(GPIO_BUTTON2_PIN, GPIO_PULLUP, GPIO_INPUT);

	/* Init wifi */
	wifiInit();
	setMux(adc_map[0]);
	uart0_sendStr("\r\nBooting\r\n");

	//Setup Timers
	os_timer_disarm(&adc_timer);
	os_timer_setfn(&adc_timer, (os_timer_func_t *)calibrationTimerFn, NULL);
	os_timer_arm(&adc_timer, 100, 1);

	os_timer_disarm(&conn_timer);
	os_timer_setfn(&conn_timer, (os_timer_func_t *)connTimer, NULL);
	os_timer_arm(&conn_timer, 1500, 1);
	
	os_timer_disarm(&bat_led_timer);
	os_timer_setfn(&bat_led_timer, (os_timer_func_t *)batLedTimerFn, NULL);
	os_timer_arm(&bat_led_timer, 500, 1);
	
	system_init_done_cb(&system_init_done);
}
//===========================================================================================

