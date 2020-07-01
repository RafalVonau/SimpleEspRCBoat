#ifndef __PWM_H__
#define __PWM_H__
void ICACHE_FLASH_ATTR pwm_init(uint32_t period, uint32_t *duty, uint32_t pwm_channel_num,uint32_t (*pin_info_list)[3]);
void ICACHE_FLASH_ATTR pwm_start(void);
void ICACHE_FLASH_ATTR pwm_set_duty(uint32_t duty, uint8_t channel);
uint32_t ICACHE_FLASH_ATTR pwm_get_duty(uint8_t channel);
void ICACHE_FLASH_ATTR pwm_set_period(uint32_t period);
uint32_t ICACHE_FLASH_ATTR pwm_get_period(void);
uint32_t ICACHE_FLASH_ATTR get_pwm_version(void);
void ICACHE_FLASH_ATTR set_pwm_debug_en(uint8_t print_en);

#endif
