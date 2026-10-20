/*
 * UCP frame format, packet IDs, and payload type definitions.
 */
#ifndef UCP_PROTOCOL_H_
#define UCP_PROTOCOL_H_

#include <stdint.h>
#include <stddef.h>
#include "ucp_types.h"

/* High byte = major, low byte = minor. 0x0100 = V1.0. See UCP_APPLICATION_LAYER.md. */
#define UCP_PROTOCOL_VERSION (uint16_t)0x0100

#define UCP_PKT_SOF                          0xA5
#define UCP_PKT_MAX_PAYLOAD_LEN_BYTES        64
#define UCP_V1_LOGICAL_HEADER_LEN_BYTES      2u
#define UCP_V1_FRAME_OVERHEAD_LEN_BYTES      5u

typedef enum
{
	UCP_APP_ID_CONTROL = 0x00,
	/* Transport-owned transfer channel (see ucp_xfer). Not CAP-advertised. */
	UCP_APP_ID_XFER = 0x01,
	UCP_APP_ID_UCP_RESERVED_END = 0x1F,
	UCP_APP_ID_APP_BASE = 0x20,
	/* Throwaway demo telemetry application (see ucp_tlm). */
	UCP_APP_ID_TLM_SIG = 0x20,
	UCP_APP_ID_SERVICE_BASE = 0x60,
	UCP_APP_ID_SYSTEM_BASE = 0xF0
} ucp_app_id_t;

typedef enum
{
	UCP_CTRL_PKT_ID_DEVICE_HELLO_REQ = 0x00,
	UCP_CTRL_PKT_ID_DEVICE_HELLO_RSP = 0x01,
	UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_REQ = 0x02,
	UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_RSP = 0x03,
	UCP_CTRL_PKT_ID_APP_VERSION_QUERY_REQ = 0x04,
	UCP_CTRL_PKT_ID_APP_VERSION_QUERY_RSP = 0x05,
	UCP_CTRL_PKT_ID_KEEPALIVE = 0x06,
	UCP_CTRL_PKT_ID_LINK_READY_REQ = 0x07,
	UCP_CTRL_PKT_ID_LINK_READY_RSP = 0x08,
	UCP_CTRL_PKT_ID_NOT_SUPPORTED = 0xF0,
	UCP_CTRL_PKT_ID_WARNING = 0xF1,
	UCP_CTRL_PKT_ID_ERROR = 0xF2
} ucp_control_pkt_id_t;

typedef enum
{
	UCP_SERVICE_PKT_ID_REQ = 0xFA,
	UCP_SERVICE_PKT_ID_RSP = 0xFB
} ucp_service_pkt_id_t;

typedef enum
{
	UCP_SERVICE_STATUS_OK = 0x00,
	UCP_SERVICE_STATUS_BAD_PACKET = 0x01,
	UCP_SERVICE_STATUS_UNSUPPORTED_OPCODE = 0x02,
	UCP_SERVICE_STATUS_BUSY = 0x03,
	UCP_SERVICE_STATUS_TIMEOUT = 0x04,
	UCP_SERVICE_STATUS_INVALID_ARG = 0x05,
	UCP_SERVICE_STATUS_VALIDATE_FAILED = 0x06,
	UCP_SERVICE_STATUS_BACKEND_ERROR = 0x07
} ucp_service_status_t;

typedef struct __attribute__((packed))
{
	uint8_t app_id;
	uint8_t pkt_id;
} ucp_v1_logical_header_t;

typedef struct __attribute__((packed))
{
	uint8_t sof;
	ucp_v1_logical_header_t logical_header;
	uint8_t len;
} ucp_v1_frame_prefix_t;

typedef struct
{
	uint8_t app_id;
	uint8_t pkt_id;
	uint8_t payload_len;
	uint8_t payload[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
} ucp_pkt_t;

uint8_t ucp_calc_checksum(const uint8_t *data, uint8_t len);
uint8_t ucp_payload_len_for_app_pkt(uint8_t app_id, uint8_t pkt_id);

#endif /* UCP_PROTOCOL_H_ */
