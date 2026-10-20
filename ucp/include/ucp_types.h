/*
 * Shared UCP payload structures and hardware identifiers.
 */
#ifndef UCP_TYPES_H_
#define UCP_TYPES_H_

#include <stdint.h>

/* Generic hardware endpoint identifiers (product maps its own board ids). */
typedef enum {
	UCP_HW_ID_UNKNOWN = 0,
	UCP_HW_ID_DEVBOARD = 0x01,
	UCP_HW_ID_HOST = 0x80
} ucp_hw_id_t;

#ifndef UNKNOWN
#define UNKNOWN UCP_HW_ID_UNKNOWN
#endif

typedef struct __attribute__((packed))
{
	uint8_t HW_ID;
	uint16_t ucp_proto_version;
	uint16_t session_nonce;
} ucp_discover_payload_t;

typedef ucp_discover_payload_t ucp_ctrl_hello_payload_t;

#define UCP_CTRL_CAP_MAX_ENTRIES 15u

typedef struct __attribute__((packed))
{
	uint8_t app_id;
	uint16_t version;
	uint8_t role_flags;
} ucp_ctrl_app_entry_t;

typedef struct __attribute__((packed))
{
	uint16_t ucp_proto_version;
	uint16_t session_nonce;
	uint8_t entry_count;
	ucp_ctrl_app_entry_t entries[UCP_CTRL_CAP_MAX_ENTRIES];
} ucp_ctrl_capability_payload_t;

typedef struct __attribute__((packed))
{
	uint8_t status;
	uint16_t negotiated_ucp_version;
	uint16_t session_nonce;
} ucp_ctrl_link_ready_payload_t;

typedef enum
{
	UCP_STATUS_OK = 0,
	UCP_STATUS_UCP_VERSION_MISMATCH = 1,
	UCP_STATUS_INVALID_PACKET = 2,
	UCP_STATUS_UNEXPECTED_STATE = 3,
	UCP_STATUS_APP_VERSION_MISMATCH = 4
} ucp_status_code_t;

typedef struct __attribute__((packed))
{
	uint8_t status_code;
	uint8_t ref_app_id;
	uint8_t ref_pkt_id;
	uint16_t detail;
} ucp_ctrl_status_payload_t;

#endif /* UCP_TYPES_H_ */
