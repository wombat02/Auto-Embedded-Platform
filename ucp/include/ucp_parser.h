#ifndef UCP_PARSER_H_
#define UCP_PARSER_H_

#include <stdbool.h>
#include <string.h>
#include "ucp_protocol.h"

typedef enum
{
	UCP_RX_WAIT_SOF = 0,
	UCP_RX_WAIT_APP_ID,
	UCP_RX_WAIT_PKT_ID,
	UCP_RX_WAIT_LEN,
	UCP_RX_WAIT_PAYLOAD,
	UCP_RX_WAIT_CHECKSUM
} ucp_rx_state_t;

typedef struct
{
	ucp_rx_state_t state;
	uint8_t app_id;
	uint8_t pkt_id;
	uint8_t len;
	uint8_t index;
	uint8_t checksum;
	uint8_t payload[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
} ucp_parser_ctx_t;

typedef struct
{
	uint32_t checksum_failures;
	uint32_t oversize_length;
	uint8_t last_oversize_len;
	uint8_t last_chk_fail_pkt_id;
	uint8_t last_chk_fail_len;
	uint8_t last_chk_fail_expected;
	uint8_t last_chk_fail_got;
	uint8_t last_chk_fail_payload_head[4];
} ucp_parser_stats_t;

void ucp_parser_ctx_init(ucp_parser_ctx_t *ctx);
void ucp_parser_ctx_reset(ucp_parser_ctx_t *ctx);
bool ucp_parse_byte_ctx(ucp_parser_ctx_t *ctx, uint8_t byte, ucp_pkt_t *out_pkt);

bool ucp_parse_byte(const uint8_t byte, ucp_pkt_t *out_pkt);
void ucp_parser_get_stats(ucp_parser_stats_t *out_stats);
void ucp_parser_reset_stats(void);
void ucp_parser_reset(void);

typedef void (*ucp_post_rx_handler_t)(const ucp_pkt_t *pkt);
void ucp_set_post_rx_handler(ucp_post_rx_handler_t handler);

#endif /* UCP_PARSER_H_ */
