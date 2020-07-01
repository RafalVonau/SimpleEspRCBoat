#define PWM_MAX_CHANNELS 2
#define PWM_USE_NMI 0

/* no user servicable parts beyond this point */
#define PWM_MAX_TICKS 0x7fffff
#define PWM_PERIOD_TO_TICKS(x) (x)
#define PWM_DUTY_TO_TICKS(x) (x)
#define PWM_MAX_DUTY PWM_MAX_TICKS
#define PWM_MAX_PERIOD PWM_MAX_TICKS

#include <c_types.h>
#include <eagle_soc.h>
#include <ets_sys.h>
#include "driver/pwm.h"
// from SDK hw_timer.c
#define TIMER1_DIVIDE_BY_16             0x0004
#define TIMER1_ENABLE_TIMER             0x0080

struct pwm_phase {
	uint32_t ticks;    ///< delay until next phase, in 200ns units
	uint16_t on_mask;  ///< GPIO mask to switch on
	uint16_t off_mask; ///< GPIO mask to switch off
};

static volatile struct pwm_phase pwm_phases[4];
static uint8_t current_phase = 0;
static uint32_t pwm_period;
static uint32_t pwm_periodh;
static uint32_t pwm_duty[PWM_MAX_CHANNELS];
static uint16_t gpio_mask[PWM_MAX_CHANNELS];
static uint16_t all_mask = 0;
static uint8_t int_active = 0;


// 3-tuples of MUX_REGISTER, MUX_VALUE and GPIO number
typedef uint32_t (pin_info_type)[3];

struct gpio_regs {
	uint32_t out;         /* 0x60000300 */
	uint32_t out_w1ts;    /* 0x60000304 */
	uint32_t out_w1tc;    /* 0x60000308 */
	uint32_t enable;      /* 0x6000030C */
	uint32_t enable_w1ts; /* 0x60000310 */
	uint32_t enable_w1tc; /* 0x60000314 */
	uint32_t in;          /* 0x60000318 */
	uint32_t status;      /* 0x6000031C */
	uint32_t status_w1ts; /* 0x60000320 */
	uint32_t status_w1tc; /* 0x60000324 */
};
static struct gpio_regs* gpio = (void*)(0x60000300);

struct timer_regs {
	uint32_t frc1_load;   /* 0x60000600 */
	uint32_t frc1_count;  /* 0x60000604 */
	uint32_t frc1_ctrl;   /* 0x60000608 */
	uint32_t frc1_int;    /* 0x6000060C */
	uint8_t  pad[16];
	uint32_t frc2_load;   /* 0x60000620 */
	uint32_t frc2_count;  /* 0x60000624 */
	uint32_t frc2_ctrl;   /* 0x60000628 */
	uint32_t frc2_int;    /* 0x6000062C */
	uint32_t frc2_alarm;  /* 0x60000630 */
};
static struct timer_regs* timer = (void*)(0x60000600);


void pwm_intr_handler(void)
{
	uint32_t ticks;
#if 1 
	struct pwm_phase ph;
	
	ph = pwm_phases[current_phase];

	asm volatile ("" : : : "memory");
	gpio->out_w1ts = (uint32_t)(ph.on_mask);
	gpio->out_w1tc = (uint32_t)(ph.off_mask);
	ticks = ph.ticks;
#else
	asm volatile ("" : : : "memory");
	gpio->out_w1ts = (uint32_t)(pwm_phases[current_phase].on_mask);
	gpio->out_w1tc = (uint32_t)(pwm_phases[current_phase].off_mask);
	ticks = pwm_phases[current_phase].ticks;
#endif
	current_phase++;
	current_phase&=3;
	timer->frc1_int &= ~FRC1_INT_CLR_MASK;
	WRITE_PERI_REG(&timer->frc1_load, ticks);
}
//===========================================================================================

void ICACHE_FLASH_ATTR pwm_init(uint32_t period, uint32_t *duty, uint32_t pwm_channel_num,uint32_t (*pin_info_list)[3])
{
	int i;

	for (i = 0; i < 4; i++) {
		pwm_phases[i].ticks = 25000; /* 5ms */
		pwm_phases[i].on_mask = 0;   /* NOP */
		pwm_phases[i].off_mask = 0;  /* NOP */
	}
	current_phase = 0;
	all_mask = 0;
	// PIN info: MUX-Register, Mux-Setting, PIN-Nr
	for (i = 0; i < pwm_channel_num; i++) {
		pin_info_type* pin_info = &pin_info_list[i];
		PIN_FUNC_SELECT((*pin_info)[0], (*pin_info)[1]);
		gpio_mask[i] = 1 << (*pin_info)[2];
		all_mask |= 1 << (*pin_info)[2];
		if (duty) pwm_set_duty(duty[i], i);
	}
	GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, all_mask);
	GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, all_mask);
	pwm_set_period(period);

	timer->frc1_int &= ~FRC1_INT_CLR_MASK;
	timer->frc1_ctrl = 0;
#if PWM_USE_NMI
	ETS_FRC_TIMER1_INTR_ATTACH(NULL, NULL);
	ETS_FRC_TIMER1_NMI_INTR_ATTACH(pwm_intr_handler);
#else
	ETS_FRC_TIMER1_INTR_ATTACH(pwm_intr_handler, NULL);
#endif
	TM1_EDGE_INT_ENABLE();
	timer->frc1_int &= ~FRC1_INT_CLR_MASK;
	timer->frc1_ctrl = 0;
	int_active = 0;
	pwm_start();
}
//===========================================================================================

#if 0
#define access_start() ETS_FRC1_INTR_DISABLE()
#define access_end() ETS_FRC1_INTR_ENABLE()
#else
#define access_start()
#define access_end()
#endif


void ICACHE_FLASH_ATTR pwm_start(void)
{
	uint32_t ticksH, ticksL;
	
	if ((pwm_duty[0] == 0) && (pwm_duty[1] == 0)) {
		/* All 0% - disable IRQ */
		timer->frc1_ctrl = 0;
		ETS_FRC1_INTR_DISABLE();
		GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, all_mask);
		int_active = 0;
		return;
	}
	/* ===--- Channel 0 ---=== */
	if (pwm_duty[0] == 0) {
		/* 0% */
		ticksL = pwm_periodh - 5000;
		access_start();
		pwm_phases[0].ticks = 5000;
		pwm_phases[0].off_mask = gpio_mask[0];
		pwm_phases[0].on_mask = 0;
		pwm_phases[1].ticks = ticksL;
		pwm_phases[1].off_mask = gpio_mask[0];
		pwm_phases[1].on_mask = 0;
		access_end();
	} else {
		/* 1% - 49% */
		ticksH = pwm_duty[0];
		if (ticksH < 100) ticksH = 100;
		if (pwm_duty[0] < pwm_periodh) ticksL = pwm_periodh - pwm_duty[0]; else ticksL = 100;
		if (ticksL < 100) ticksL = 100;
		access_start();
		pwm_phases[0].ticks = ticksH;
		pwm_phases[0].off_mask = 0;
		pwm_phases[0].on_mask = gpio_mask[0];
		pwm_phases[1].ticks = ticksL;
		pwm_phases[1].off_mask = gpio_mask[0];;
		pwm_phases[1].on_mask = 0;
		access_end();
	}
	/* ===--- Channel 1 ---=== */
	if (pwm_duty[1] == 0) {
		/* 0% */
		ticksL = pwm_periodh - (5*1000);
		access_start();
		pwm_phases[2].ticks = (5*1000);
		pwm_phases[2].off_mask = gpio_mask[1];
		pwm_phases[2].on_mask = 0;
		pwm_phases[3].ticks = ticksL;
		pwm_phases[3].off_mask = gpio_mask[1];
		pwm_phases[3].on_mask = 0;
		access_end();
	} else {
		/* 1% - 49% */
		ticksH = pwm_duty[1];
		if (ticksH < 100) ticksH = 100;
		if (pwm_duty[1] < pwm_periodh) ticksL = pwm_periodh - pwm_duty[1]; else ticksL = 100;
		if (ticksL < 100) ticksL = 100;
		access_start();
		pwm_phases[2].ticks = ticksH;
		pwm_phases[2].off_mask = 0;
		pwm_phases[2].on_mask = gpio_mask[1];
		pwm_phases[3].ticks = ticksL;
		pwm_phases[3].off_mask = gpio_mask[1];;
		pwm_phases[3].on_mask = 0;
		access_end();
	}
	// start irq if not running
	if (!int_active) {
		int_active = 1;
		ETS_FRC1_INTR_ENABLE();
		RTC_REG_WRITE(FRC1_LOAD_ADDRESS, 0);
		timer->frc1_ctrl = TIMER1_DIVIDE_BY_16 | TIMER1_ENABLE_TIMER;
	}
}
//===========================================================================================

void ICACHE_FLASH_ATTR pwm_set_duty(uint32_t duty, uint8_t channel)
{
	pwm_duty[channel] = duty;
}
//===========================================================================================

uint32_t ICACHE_FLASH_ATTR pwm_get_duty(uint8_t channel)
{
	return pwm_duty[channel];
}
//===========================================================================================

void ICACHE_FLASH_ATTR pwm_set_period(uint32_t period)
{
	pwm_period = period;
	if (period > PWM_MAX_PERIOD) pwm_period = PWM_MAX_PERIOD; else pwm_period = period;
	pwm_periodh = pwm_period/2;
}
//===========================================================================================

uint32_t ICACHE_FLASH_ATTR pwm_get_period(void)
{
	return pwm_period;
}
//===========================================================================================



