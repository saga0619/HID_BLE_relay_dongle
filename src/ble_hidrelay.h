/*
 * HID Relay Custom BLE Service (Static GATT)
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_HIDRELAY_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_HIDRELAY_H_

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h> /* BT_UUID_128_ENCODE, etc. */

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------------
 * 1) UUID 
 *
 * Service: 597f1290-5b99-477d-9261-f0ed801fc566
 * RX Char: 597f1291-5b99-477d-9261-f0ed801fc566
 * TX Char: 597f1292-5b99-477d-9261-f0ed801fc566
 *------------------------------------------------------------------------------
 */

#define BT_UUID_HIDRELAY_SVC_VAL \
	BT_UUID_128_ENCODE(0x597f1290, 0x5b99, 0x477d, 0x9261, 0xf0ed801fc566)

#define BT_UUID_HIDRELAY_RX_VAL \
	BT_UUID_128_ENCODE(0x597f1291, 0x5b99, 0x477d, 0x9261, 0xf0ed801fc566)

#define BT_UUID_HIDRELAY_TX_VAL \
	BT_UUID_128_ENCODE(0x597f1292, 0x5b99, 0x477d, 0x9261, 0xf0ed801fc566)

#define BT_UUID_HIDRELAY_SERVICE BT_UUID_DECLARE_128(BT_UUID_HIDRELAY_SVC_VAL)
#define BT_UUID_HIDRELAY_RX_CHAR BT_UUID_DECLARE_128(BT_UUID_HIDRELAY_RX_VAL)
#define BT_UUID_HIDRELAY_TX_CHAR BT_UUID_DECLARE_128(BT_UUID_HIDRELAY_TX_VAL)

/*------------------------------------------------------------------------------
 * 2) Callbacks
 *------------------------------------------------------------------------------
 */

struct bt_hidrelay_cb {
	/** @brief TX Notify : Change Subscription (Notify On/Off) */
	void (*notif_enabled)(bool enabled, void *user_data);

	/** @brief RX Write Event (Central -> Peripheral) */
	void (*received)(struct bt_conn *conn, const void *data, uint16_t len,
			 void *user_data);
};

/*------------------------------------------------------------------------------
 * 3) API Functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief HIDRelay Service Initialization
 *
 *
 * @param cb        callback functions
 * @param user_data user data
 *
 * @return 0 on success, negative on error
 */
int bt_hidrelay_init(const struct bt_hidrelay_cb *cb, void *user_data);

/**
 * @brief HIDRelay TX Notify transmit
 *
 * @param conn   Target Connection Handle
 * @param data   
 * @param len    
 *
 * @return 0 on success, negative error
 */
int bt_hidrelay_send(struct bt_conn *conn, const void *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_HIDRELAY_H_ */
