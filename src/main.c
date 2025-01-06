/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/class/usb_cdc.h>

#include "hid_keyboard.h"

#include <zephyr/bluetooth/bluetooth.h>

#include "ble_hidrelay.h"
#include <math.h>

#define DEVICE_NAME		CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct pwm_dt_spec red_led = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));
static const struct pwm_dt_spec green_led = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led1));
static const struct pwm_dt_spec blue_led = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led2));

// #define HID_REPORT_SIZE 8
#define SW0_NODE DT_ALIAS(sw0)

#if DT_NODE_HAS_STATUS(SW0_NODE, okay)
static const struct gpio_dt_spec sw0_gpio = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
#endif


/* Event FIFO */

K_FIFO_DEFINE(evt_fifo);

enum evt_t {
	GPIO_BUTTON_0	= 0x00,
	LED_SIGNAL_0	= 0x01,
	KEY_UNKNOWN 	= 0x02,
	GPIO_BUTTON_3	= 0x03,
	CDC_UP		= 0x04,
	CDC_DOWN	= 0x05,
	CDC_LEFT	= 0x06,
	CDC_RIGHT	= 0x07,
	CDC_UNKNOWN	= 0x08,
	CDC_STRING	= 0x09,
	HID_MOUSE_CLEAR	= 0x0A,
	HID_KBD_CLEAR	= 0x0B,
	HID_KBD_STRING	= 0x0C,
};

struct app_evt_t {
	sys_snode_t node;
	enum evt_t event_type;
};

#define FIFO_ELEM_MIN_SZ        sizeof(struct app_evt_t)
#define FIFO_ELEM_MAX_SZ        sizeof(struct app_evt_t)
#define FIFO_ELEM_COUNT         255
#define FIFO_ELEM_ALIGN         sizeof(unsigned int)

K_HEAP_DEFINE(event_elem_pool, FIFO_ELEM_MAX_SZ * FIFO_ELEM_COUNT + 256);

static inline void app_evt_free(struct app_evt_t *ev)
{
	k_heap_free(&event_elem_pool, ev);
}

static inline void app_evt_put(struct app_evt_t *ev)
{
	k_fifo_put(&evt_fifo, ev);
}

static inline struct app_evt_t *app_evt_get(void)
{
	return k_fifo_get(&evt_fifo, K_NO_WAIT);
}

static inline void app_evt_flush(void)
{
	struct app_evt_t *ev;

	do {
		ev = app_evt_get();
		if (ev) {
			app_evt_free(ev);
		}
	} while (ev != NULL);
}

static inline struct app_evt_t *app_evt_alloc(void)
{
	struct app_evt_t *ev;

	ev = k_heap_alloc(&event_elem_pool,
			  sizeof(struct app_evt_t),
			  K_NO_WAIT);
	if (ev == NULL) {
		printk("APP event allocation failed!");
		app_evt_flush();

		ev = k_heap_alloc(&event_elem_pool,
				  sizeof(struct app_evt_t),
				  K_NO_WAIT);
		if (ev == NULL) {
			printk("APP event memory corrupted.");
			__ASSERT_NO_MSG(0);
			return NULL;
		}
		return NULL;
	}

	return ev;
}

/* HID */


static K_SEM_DEFINE(evt_sem, 0, 1);	/* starts off "not available" */

static struct gpio_callback gpio_callbacks[4];


uint8_t global_hid_report[HID_REPORT_SIZE_L] = {0};


static const char *gpio0	=	"Button 0 pressed\r\n";
static const char *unknown	=	"Command not recognized.\r\n";
static const char *evt_fail	=	"Unknown event detected!\r\n";
static const char *set_str	=	"String set to: ";
static const char *endl		=	"\r\n";


static void singal_led(void)
{
	struct app_evt_t *new_evt = app_evt_alloc();

	new_evt->event_type = LED_SIGNAL_0;
	app_evt_put(new_evt);
	k_sem_give(&evt_sem);
}

static void clear_kbd_report(void)
{
	struct app_evt_t *new_evt = app_evt_alloc();

	new_evt->event_type = HID_KBD_CLEAR;
	app_evt_put(new_evt);
	k_sem_give(&evt_sem);
}
/* CDC ACM */

static volatile bool data_transmitted;
static volatile bool data_arrived;

static void write_data(const struct device *dev, const char *buf, int len)
{
	uart_irq_tx_enable(dev);

	while (len) {
		int written;

		data_transmitted = false;
		written = uart_fifo_fill(dev, (const uint8_t *)buf, len);
		while (data_transmitted == false) {
			k_yield();
		}

		len -= written;
		buf += written;
	}

	uart_irq_tx_disable(dev);
}

/* Devices */

static void btn0(const struct device *gpio, struct gpio_callback *cb,
		 uint32_t pins)
{
	struct app_evt_t *ev = app_evt_alloc();

	ev->event_type = GPIO_BUTTON_0,
	app_evt_put(ev);
	k_sem_give(&evt_sem);
}

int callbacks_configure(const struct gpio_dt_spec *gpio,
			void (*handler)(const struct device *, struct gpio_callback*,
					uint32_t),
			struct gpio_callback *callback)
{
	if (!device_is_ready(gpio->port)) {
		printk("%s: device not ready.", gpio->port->name);
		return -ENODEV;
	}

	gpio_pin_configure_dt(gpio, GPIO_INPUT);

	gpio_init_callback(callback, handler, BIT(gpio->pin));
	gpio_add_callback(gpio->port, callback);
	gpio_pin_interrupt_configure_dt(gpio, GPIO_INT_EDGE_TO_ACTIVE);

	return 0;
}

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	printk("Status %d", status);
}

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_HIDRELAY_SVC_VAL),
};

static bool bt_disconnected = true;

static void notif_enabled(bool enabled, void *ctx)
{
	ARG_UNUSED(ctx);

	printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));

	if(enabled){
		bt_disconnected = false;

	}
	else{
		bt_disconnected = true;
		
	}
}
static uint8_t pressed_keys[6] = {0};
static uint8_t current_modifiers = 0;

static void add_key(uint8_t key)
{
    for (int i = 0; i < 6; i++) {
        if (pressed_keys[i] == 0) {
            pressed_keys[i] = key;
            return;
        }
    }
}

/* pressed_keys 배열에서 key 제거 */
static void remove_key(uint8_t key)
{
    for (int i = 0; i < 6; i++) {
        if (pressed_keys[i] == key) {
            pressed_keys[i] = 0;
        }
    }
    
	return;
}

/* 전체 상태(Modifiers + pressed_keys[])를 바탕으로 HID 리포트 전송 */
static void send_full_report(void)
{
    uint8_t rep[8] = {0};

    /* rep[0] : Modifier 비트마스크 */
    rep[0] = current_modifiers;

    /* rep[1] : Reserved(항상 0) */
    rep[1] = 0;

    /* rep[2..7] : 동시에 눌려있는 일반 키(최대 6개) */
    for (int i = 0; i < 6; i++) {
        rep[i + 2] = pressed_keys[i];
    }

    if (hid_keyboard_send_report(rep)) {
        printk("Failed to send HID report\n");
    }
}

/* modifier 키 비트마스크를 업데이트해주는 함수 */
void update_modifiers(uint8_t modifier_mask, bool press)
{
    if (press) {
        current_modifiers |= modifier_mask;
    } else {
        current_modifiers &= ~modifier_mask;
    }
	// printk("Current Modifiers: 0x%02x \n", current_modifiers);
}


static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx)
{
	char message[CONFIG_BT_L2CAP_TX_MTU + 1] = "";
	
	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);

    size_t copy_len = len < sizeof(message) - 1 ? len : sizeof(message) - 1;
    memcpy(message, data, copy_len);
    message[copy_len] = '\0'; // Null-terminate the string

	// static bool mod_act = false;

 	char *token = strtok(message, "\n");
    while (token != NULL) {
        char action;
        uint32_t qt_key;

        // Parse the action and key code
        if (sscanf(token, "%c:0x%x", &action, &qt_key) == 2) {
            bool is_press = (action == 'P');

            uint8_t hid_key = 0;
            uint8_t modifier_mask  = 0;

            if (get_hid_key(qt_key, &hid_key, &modifier_mask )) {
                if (modifier_mask != 0) {
                    // It's a modifier key
                    update_modifiers(modifier_mask, is_press);
					// printk("Modifier mask: 0x%02x\n", modifier_mask);

					if (hid_key !=0)
					{	//modifier key with regular key
						if (is_press) {
							add_key(hid_key);
						} else{
							remove_key(hid_key);
						}
					}

                } else {
                    // It's a regular key
                    if (is_press) {
						add_key(hid_key);
					} else{
						remove_key(hid_key);
                    }
                }

				send_full_report();
				singal_led();
            }
			else{
				if(!is_press)
				{
					printk("Key not found: %c:0x%x\n", action, qt_key);
					struct app_evt_t *ev = app_evt_alloc();

					ev->event_type = KEY_UNKNOWN,
					app_evt_put(ev);
					k_sem_give(&evt_sem);

				}
			}
        }

        // Get the next token
        token = strtok(NULL, "\n");
	}

}

static struct bt_hidrelay_cb hidrelay_cb = {
	.notif_enabled = notif_enabled,
	.received      = received,
};

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

int main(void)
{	
	if (!gpio_is_ready_dt(&led0)) {
		return 0;
	}
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);

	gpio_pin_set_dt(&led0, 1);
	k_msleep(50);
	gpio_pin_set_dt(&led0, 0);
	k_msleep(50);

	if (!device_is_ready(blue_led.dev) || !device_is_ready(green_led.dev) || !device_is_ready(red_led.dev))
	{
		printk("Error: PWM device not ready.\n");
		return 0;
	}

	const struct device *cdc_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

	struct app_evt_t *ev;
	int ret;

	hid_keyboard_init();

	/* Config BT */
	int err;
	err = bt_enable(NULL);
	if (err) {
		printk("Failed to enable bluetooth: %d\n", err);
		return err;
	}

	err = bt_hidrelay_init(&hidrelay_cb, NULL);
	if (err) {
		printk("Failed to register HIDRelay cb (err %d)\n", err);
		return err;
	}
	printk("HIDRelay service registered\n");


	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return 0;
	}
	printk("Advertising started with HIDRelay UUID\n");


	if (!device_is_ready(cdc_dev)) {
		printk("CDC ACM device %s is not ready",
			cdc_dev->name);
		return 0;
	}


	if (callbacks_configure(&sw0_gpio, &btn0, &gpio_callbacks[0])) {
		printk("Failed configuring button 0 callback.");
		return 0;
	}
	

	ret = usb_enable(status_cb);
	if (ret != 0) {
		printk("Failed to enable USB");
		return 0;
	}

	int cnt = 0;
	uint32_t b_fade = 0;

	while (true) {
		if (cnt++ == 1000)
		{
			cnt = 0;
		}
		int32_t fade = blue_led.period * sin((double)cnt / 1000.0 * 3.14);
		uint32_t ufade = fade < 0 ? -fade : fade;

		k_msleep(1);
		if(bt_disconnected)
		{
			if (b_fade != ufade)
			{
				b_fade = ufade;
				ret = pwm_set_dt(&blue_led, blue_led.period, ufade);

				if (ret < 0)
				{
					printk("Error: Failed to set PWM value.\n");
					return 0;
				}
			}
		}
		else //connected
		{
			if (b_fade != blue_led.period)
			{
				b_fade = blue_led.period;
				ret = pwm_set_dt(&blue_led, blue_led.period, blue_led.period);

				if (ret < 0)
				{
					printk("Error: Failed to set PWM value.\n");
					return 0;
				}
			}
		}
		// ret = pwm_set_dt(&blue_led, blue_led.period, ufade);

		if (ret < 0)
		{
			printk("Error: Failed to set PWM value.\n");
			return 0;
		}

		while ((ev = app_evt_get()) != NULL) {
			switch (ev->event_type) {
			case GPIO_BUTTON_0:
			{
								/* RESET key */
				uint8_t rep[] = {0x00, 0x00, 0x00, 0x00,
					      0x00, 0x00, 0x00,
					      0x00};

				hid_keyboard_send_report(rep);
				// hid_int_ep_write(hid0_dev, rep,
				// 		 sizeof(rep), NULL);
				write_data(cdc_dev, gpio0, strlen(gpio0));
				clear_kbd_report();
				break;
			}
			case LED_SIGNAL_0:
			{
				gpio_pin_set_dt(&led0, 1);
				k_msleep(50);
				gpio_pin_set_dt(&led0, 0);
				k_msleep(50);
				/* LED Signal 0 */

				break;
			}
			case KEY_UNKNOWN:
			{
				printk("Unknown key event!\n");
				uint8_t rep[8] = {0};
				uint8_t rep2[8] = {0};
				uint8_t crep[] = {0x00, 0x00, 0x00, 0x00,
					      0x00, 0x00, 0x00, 0x00};
				rep[2] = HID_KEY_E;
				rep[3] = HID_KEY_R;

				rep2[2] = HID_KEY_R;
				rep2[3] = HID_KEY_O;
				rep2[4] = HID_KEY_R;

				hid_keyboard_send_report(rep);
				hid_keyboard_send_report(crep);
				hid_keyboard_send_report(rep2);
				hid_keyboard_send_report(crep);
				break;
			}
			case CDC_UNKNOWN:
			{
				write_data(cdc_dev, unknown, strlen(unknown));
				break;
			}
			case CDC_STRING:
			{
				write_data(cdc_dev, set_str, strlen(set_str));
				// write_data(cdc_dev, string, strlen(string));
				write_data(cdc_dev, endl, strlen(endl));

				break;
			}
			case HID_KBD_CLEAR:
			{
				/* Clear kbd report */
				uint8_t rep[] = {0x00, 0x00, 0x00, 0x00,
					      0x00, 0x00, 0x00, 0x00};

				hid_keyboard_send_report(rep);
				break;
			}
			default:
			{
				printk("Unknown event to execute");
				write_data(cdc_dev, evt_fail,
					   strlen(evt_fail));
				break;
			}
			break;
			}
			app_evt_free(ev);
		}
	}
}
