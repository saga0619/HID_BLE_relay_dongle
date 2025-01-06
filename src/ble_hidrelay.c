/*
 *
 * HID Relay Custom BLE Service (Static GATT)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hidrelay, LOG_LEVEL_INF);

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include "ble_hidrelay.h"

static const struct bt_hidrelay_cb *g_cb;
static void *g_user_data;

/* -----------------------------------------------------------------------------
 * RX (Write) handler
 * -----------------------------------------------------------------------------
 */
static ssize_t hidrelay_rx_write(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr,
				 const void *buf,
				 uint16_t len,
				 uint16_t offset,
				 uint8_t flags)
{
	LOG_DBG("hidrelay_rx_write: len=%u", len);

	if (g_cb && g_cb->received) {
		g_cb->received(conn, buf, len, g_user_data);
	}
	return len;
}

/* -----------------------------------------------------------------------------
 * Change CCC Descriptor (Notify On/Off)
 * -----------------------------------------------------------------------------
 */
static void hidrelay_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	bool enabled = (value == BT_GATT_CCC_NOTIFY);

	LOG_DBG("hidrelay_ccc_cfg_changed: %s", (enabled ? "Notify On" : "Notify Off"));

	if (g_cb && g_cb->notif_enabled) {
		g_cb->notif_enabled(enabled, g_user_data);
	}
}

/* -----------------------------------------------------------------------------
 * GATT Table Definition
 *
 * 0: Service Declaration
 * 1: RX Char Declaration
 * 2: RX Char Value
 * 3: TX Char Declaration
 * 4: TX Char Value
 * 5: CCC Descriptor
 * -----------------------------------------------------------------------------
 */
BT_GATT_SERVICE_DEFINE(hidrelay_svc,
	/* Service */
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDRELAY_SERVICE),

	/* RX Characteristic */
	BT_GATT_CHARACTERISTIC(
		BT_UUID_HIDRELAY_RX_CHAR,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE,
		NULL,                /* read callback */
		hidrelay_rx_write,   /* write callback */
		NULL                 /* value */
	),

	/* TX Characteristic */
	BT_GATT_CHARACTERISTIC(
		BT_UUID_HIDRELAY_TX_CHAR,
		BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ,
		NULL,                /* read callback */
		NULL,                /* write callback */
		NULL                 /* value */
	),

	/* CCC Descriptor for TX */
	BT_GATT_CCC(hidrelay_ccc_cfg_changed,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

/* -----------------------------------------------------------------------------
 * API: HIDRelay Service Initialization
 * -----------------------------------------------------------------------------
 */
int bt_hidrelay_init(const struct bt_hidrelay_cb *cb, void *user_data)
{
	if (!cb) {
		return -EINVAL;
	}

	g_cb = cb;
	g_user_data = user_data;

	LOG_INF("HIDRelay service (static GATT) initialized");
	return 0;
}

int bt_hidrelay_send(struct bt_conn *conn, const void *data, uint16_t len)
{
	if (!data || !len) {
		return -EINVAL;
	}

	return bt_gatt_notify(conn, &hidrelay_svc.attrs[4], data, len);
}
