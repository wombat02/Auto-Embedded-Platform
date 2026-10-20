/*
 * aep_ucp_usb.c — USB CDC ↔ ucp_session.
 *
 * Architecture:
 *   1. Dedicated RX thread: bulk OUT armed with UX_WAIT_FOREVER into a ring.
 *      Short read timeouts leave OUT pending → UX_ABORTED / USBX corruption.
 *   2. Poll (session/RX-drain/TX pump) runs on the USBX device app thread —
 *      same thread that would otherwise ASCII-write CDC. Never concurrent
 *      cdc_acm_write with forever RX from a third thread (log drain).
 *   3. Com thread does NOT call USBX; it only submits sig snapshots.
 *
 * ASCII CDC is gated for the life of this module (owns_cdc == inited).
 *
 * See examples/stm32_h5/GLUE.md and BRINGUP.md.
 */
#include "aep_ucp_usb.h"
#include "ux_device_cdc_acm.h"
#include "ucp_session.h"
#include "ucp_parser.h"
#include "ucp_port.h"
#include "ux_api.h"
#include "ux_device_class_cdc_acm.h"
#include "tx_api.h"
#include "log.h"
#include <string.h>

extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm;

#define USB_TX_Q_CAP        16u
#define USB_TX_MAX          (UCP_V1_FRAME_OVERHEAD_LEN_BYTES + UCP_PKT_MAX_PAYLOAD_LEN_BYTES)
#define USB_RX_CHUNK        512u
#define USB_RX_RING_SIZE    1024u
#define USB_RX_THREAD_STACK 3072u
#define USB_RX_THREAD_PRIO  8u
/*
 * Windows often completes device bulk IN only on the next host OUT (50 ms).
 * 500 ms covers several of those. On expiry the IN is aborted and its
 * semaphore drained before another transmit is started.
 */
#define USB_WRITE_TIMEOUT_TICKS 500u

typedef struct {
	uint16_t len;
	uint16_t _pad;
	uint8_t data[USB_TX_MAX];
} usb_tx_frame_t;

static ucp_session_t s_session;
static ucp_parser_ctx_t s_parser;

static usb_tx_frame_t s_txq[USB_TX_Q_CAP];
static volatile uint16_t s_tx_head;
static volatile uint16_t s_tx_tail;
static TX_MUTEX s_tx_mu;
static bool s_tx_mu_ready;

static uint8_t s_rx_chunk[USB_RX_CHUNK] __attribute__((aligned(4)));
static uint8_t s_rx_ring[USB_RX_RING_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static TX_MUTEX s_rx_mu;
static bool s_rx_mu_ready;

static usb_tx_frame_t s_tx_bounce __attribute__((aligned(4)));

static TX_THREAD s_rx_thread;
static ULONG s_rx_stack[USB_RX_THREAD_STACK / sizeof(ULONG)];
static bool s_rx_thread_started;

static bool s_inited;
static bool s_host_ucp_seen;
static bool s_was_host_ready;
static bool s_write_timeout_set;

static void tx_queue_clear(void)
{
	if (!s_tx_mu_ready)
	{
		s_tx_head = 0u;
		s_tx_tail = 0u;
		return;
	}
	if (tx_mutex_get(&s_tx_mu, TX_WAIT_FOREVER) == TX_SUCCESS)
	{
		s_tx_head = 0u;
		s_tx_tail = 0u;
		(void)tx_mutex_put(&s_tx_mu);
	}
}

static void rx_ring_clear(void)
{
	if (!s_rx_mu_ready)
	{
		s_rx_head = 0u;
		s_rx_tail = 0u;
		return;
	}
	if (tx_mutex_get(&s_rx_mu, TX_WAIT_FOREVER) == TX_SUCCESS)
	{
		s_rx_head = 0u;
		s_rx_tail = 0u;
		(void)tx_mutex_put(&s_rx_mu);
	}
}

static void link_reset_session(void)
{
	ucp_parser_ctx_init(&s_parser);
	ucp_session_init(&s_session);
	s_host_ucp_seen = false;
	tx_queue_clear();
	rx_ring_clear();
	s_write_timeout_set = false;
}

static void rx_ring_push(const uint8_t *data, ULONG len)
{
	ULONG i;

	if (!s_rx_mu_ready || (data == NULL) || (len == 0u))
	{
		return;
	}
	if (tx_mutex_get(&s_rx_mu, TX_WAIT_FOREVER) != TX_SUCCESS)
	{
		return;
	}
	for (i = 0u; i < len; i++)
	{
		uint16_t next = (uint16_t)((s_rx_head + 1u) % USB_RX_RING_SIZE);
		if (next == s_rx_tail)
		{
			break;
		}
		s_rx_ring[s_rx_head] = data[i];
		s_rx_head = next;
	}
	(void)tx_mutex_put(&s_rx_mu);
}

static uint16_t rx_ring_drain(uint8_t *dst, uint16_t cap)
{
	uint16_t n = 0u;

	if (!s_rx_mu_ready || (dst == NULL) || (cap == 0u))
	{
		return 0u;
	}
	if (tx_mutex_get(&s_rx_mu, TX_NO_WAIT) != TX_SUCCESS)
	{
		return 0u;
	}
	while ((n < cap) && (s_rx_tail != s_rx_head))
	{
		dst[n++] = s_rx_ring[s_rx_tail];
		s_rx_tail = (uint16_t)((s_rx_tail + 1u) % USB_RX_RING_SIZE);
	}
	(void)tx_mutex_put(&s_rx_mu);
	return n;
}

static void usb_rx_arm_forever(void)
{
	if ((cdc_acm == UX_NULL) || !usb_cdc_host_ready())
	{
		return;
	}
	(void)ux_device_class_cdc_acm_ioctl(cdc_acm,
										UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_READ_TIMEOUT,
										(VOID *)(ALIGN_TYPE)UX_WAIT_FOREVER);
}

static void usb_rx_thread_entry(ULONG arg)
{
	ULONG actual;
	UINT st;
	bool armed = false;

	(void)arg;
	for (;;)
	{
		if ((cdc_acm == UX_NULL) || !usb_cdc_host_ready())
		{
			armed = false;
			tx_thread_sleep((ULONG)((20u * TX_TIMER_TICKS_PER_SECOND) / 1000u));
			continue;
		}
		if (!armed)
		{
			usb_rx_arm_forever();
			armed = true;
		}
		actual = 0u;
		st = ux_device_class_cdc_acm_read(cdc_acm, s_rx_chunk, USB_RX_CHUNK, &actual);
		if ((st == UX_SUCCESS) && (actual > 0u))
		{
			rx_ring_push(s_rx_chunk, actual);
			continue;
		}
		if ((st == UX_CONFIGURATION_HANDLE_UNKNOWN) || (st == UX_TRANSFER_NO_ANSWER) ||
			(cdc_acm == UX_NULL) || !usb_cdc_host_ready())
		{
			armed = false;
			tx_thread_sleep((ULONG)((20u * TX_TIMER_TICKS_PER_SECOND) / 1000u));
			continue;
		}
		/* UX_ABORTED: re-arm forever — do not abort-loop. */
		armed = false;
		tx_thread_sleep((ULONG)((50u * TX_TIMER_TICKS_PER_SECOND) / 1000u));
	}
}

static bool tx_queue_pop_to_bounce(void)
{
	if (!s_tx_mu_ready)
	{
		return false;
	}
	if (tx_mutex_get(&s_tx_mu, TX_NO_WAIT) != TX_SUCCESS)
	{
		return false;
	}
	if (s_tx_tail == s_tx_head)
	{
		(void)tx_mutex_put(&s_tx_mu);
		return false;
	}
	s_tx_bounce = s_txq[s_tx_tail];
	s_tx_tail = (uint16_t)((s_tx_tail + 1u) % USB_TX_Q_CAP);
	(void)tx_mutex_put(&s_tx_mu);
	return true;
}

static void usb_ensure_write_timeout(void)
{
	if (s_write_timeout_set || (cdc_acm == UX_NULL))
	{
		return;
	}
	if (ux_device_class_cdc_acm_ioctl(cdc_acm,
									 UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_WRITE_TIMEOUT,
									 (VOID *)(ALIGN_TYPE)USB_WRITE_TIMEOUT_TICKS) == UX_SUCCESS)
	{
		s_write_timeout_set = true;
	}
}

/* Same IN the CDC class write path selects (data interface: OUT then IN). */
static UX_SLAVE_TRANSFER *usb_bulk_in_transfer(void)
{
	UX_SLAVE_INTERFACE *iface;
	UX_SLAVE_ENDPOINT *endpoint;

	if ((cdc_acm == UX_NULL) || (cdc_acm->ux_slave_class_cdc_acm_interface == UX_NULL))
	{
		return UX_NULL;
	}
	iface = cdc_acm->ux_slave_class_cdc_acm_interface;
	endpoint = iface->ux_slave_interface_first_endpoint;
	if (endpoint == UX_NULL)
	{
		return UX_NULL;
	}
	if ((endpoint->ux_slave_endpoint_descriptor.bEndpointAddress & UX_ENDPOINT_DIRECTION) !=
		UX_ENDPOINT_IN)
	{
		endpoint = endpoint->ux_slave_endpoint_next_endpoint;
	}
	if (endpoint == UX_NULL)
	{
		return UX_NULL;
	}
	return &endpoint->ux_slave_endpoint_transfer_request;
}

/*
 * A timed-out cdc_acm_write leaves the IN PENDING and the completion semaphore
 * either still empty or already given by a late ISR. Abort NAKs the CHEP when
 * it is still pending; draining the semaphore stops the next write from
 * returning before its own hardware transfer finishes.
 */
static void usb_quiesce_bulk_in(void)
{
	UX_SLAVE_TRANSFER *tr;

	tr = usb_bulk_in_transfer();
	if ((tr == UX_NULL) || (cdc_acm == UX_NULL))
	{
		return;
	}
	if (tr->ux_slave_transfer_request_status == UX_TRANSFER_STATUS_PENDING)
	{
		(void)ux_device_class_cdc_acm_ioctl(cdc_acm,
											UX_SLAVE_CLASS_CDC_ACM_IOCTL_ABORT_PIPE,
											(VOID *)(ALIGN_TYPE)UX_SLAVE_CLASS_CDC_ACM_ENDPOINT_XMIT);
	}
	while (tx_semaphore_get(&tr->ux_slave_transfer_request_semaphore, TX_NO_WAIT) == TX_SUCCESS)
	{
	}
}

static void usb_pump_tx(void)
{
	ULONG actual;

	if ((cdc_acm == UX_NULL) || !usb_cdc_host_ready())
	{
		return;
	}
	usb_ensure_write_timeout();
	while (tx_queue_pop_to_bounce())
	{
		usb_quiesce_bulk_in();
		actual = 0u;
		if (ux_device_class_cdc_acm_write(cdc_acm, s_tx_bounce.data, s_tx_bounce.len,
										 &actual) != UX_SUCCESS)
		{
			usb_quiesce_bulk_in();
			return;
		}
	}
}

void aep_ucp_usb_init(void)
{
	if (s_inited)
	{
		return;
	}
	ucp_session_init(&s_session);
	ucp_parser_ctx_init(&s_parser);
	s_tx_head = 0u;
	s_tx_tail = 0u;
	s_rx_head = 0u;
	s_rx_tail = 0u;
	s_host_ucp_seen = false;
	s_was_host_ready = false;
	s_write_timeout_set = false;

	if (tx_mutex_create(&s_tx_mu, "ucp_usb_tx", TX_NO_INHERIT) == TX_SUCCESS)
	{
		s_tx_mu_ready = true;
	}
	if (tx_mutex_create(&s_rx_mu, "ucp_usb_rx", TX_NO_INHERIT) == TX_SUCCESS)
	{
		s_rx_mu_ready = true;
	}

	/* Gate ASCII CDC before RX thread can arm (no log-write race). */
	s_inited = true;

	if (!s_rx_thread_started)
	{
		if (tx_thread_create(&s_rx_thread, "ucp_usb_rx", usb_rx_thread_entry, 0, s_rx_stack,
							 USB_RX_THREAD_STACK, USB_RX_THREAD_PRIO, USB_RX_THREAD_PRIO,
							 TX_NO_TIME_SLICE, TX_AUTO_START) == TX_SUCCESS)
		{
			s_rx_thread_started = true;
		}
		else
		{
			loge("UCP USB: RX thread create failed");
		}
	}
	logi("UCP USB: forever OUT RX; poll on USBX app thread");
}

bool aep_ucp_usb_ready(void)
{
	return s_inited && ucp_session_is_ready(&s_session);
}

bool aep_ucp_usb_owns_cdc(void)
{
	/* Once inited, USBX app thread must run UCP poll on this CDC. */
	return s_inited;
}

bool aep_ucp_usb_send_frame(const uint8_t *frame, uint16_t len)
{
	uint16_t next;

	if (!s_inited || !s_tx_mu_ready || (frame == NULL) || (len == 0u) || (len > USB_TX_MAX))
	{
		return false;
	}
	if (tx_mutex_get(&s_tx_mu, TX_NO_WAIT) != TX_SUCCESS)
	{
		return false;
	}
	next = (uint16_t)((s_tx_head + 1u) % USB_TX_Q_CAP);
	if (next == s_tx_tail)
	{
		(void)tx_mutex_put(&s_tx_mu);
		return false;
	}
	memcpy(s_txq[s_tx_head].data, frame, len);
	s_txq[s_tx_head].len = len;
	s_tx_head = next;
	(void)tx_mutex_put(&s_tx_mu);
	return true;
}

void aep_ucp_usb_log_stacks(void)
{
	/* Optional: HWM for RX thread can be added later. */
}

void aep_ucp_usb_poll(void)
{
	ucp_pkt_t pkt;
	uint8_t batch[64];
	uint16_t n;
	uint16_t i;
	uint32_t now_ms;
	const bool host_ready = (usb_cdc_host_ready() != 0u) && (cdc_acm != UX_NULL);

	if (!s_inited)
	{
		return;
	}

	if (!host_ready)
	{
		if (s_was_host_ready)
		{
			link_reset_session();
			s_was_host_ready = false;
		}
		return;
	}

	if (!s_was_host_ready)
	{
		s_was_host_ready = true;
		link_reset_session();
	}

	for (;;)
	{
		n = rx_ring_drain(batch, (uint16_t)sizeof(batch));
		if (n == 0u)
		{
			break;
		}
		for (i = 0u; i < n; i++)
		{
			if (ucp_parse_byte_ctx(&s_parser, batch[i], &pkt))
			{
				s_host_ucp_seen = true;
				ucp_session_on_packet(&s_session, &pkt);
			}
		}
	}

	now_ms = ucp_port_get_time_ms();
	if (s_host_ucp_seen)
	{
		ucp_session_tick(&s_session, now_ms);
#if (AEP_UCP_TLM_ON_USB != 0)
		ucp_port_apps_tick(now_ms, ucp_session_is_ready(&s_session), 0u);
#endif
		usb_pump_tx();
	}
}

