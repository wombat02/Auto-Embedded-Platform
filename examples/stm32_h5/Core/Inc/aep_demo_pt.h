/*
 * Shared mock PT-CAN / UDS DID / extension-sig / dash contract for stm32_h5.
 * Mirrored by tools/aep_demo_pt.py — keep both in sync.
 */
#ifndef AEP_DEMO_PT_H_
#define AEP_DEMO_PT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PT profile A (CAN0) — original demo layouts */
#define AEP_PT_PROFILE_A 0u
#define AEP_PT_FRAME_A_POWERTRAIN 0x201u
#define AEP_PT_FRAME_A_TORQUE 0x202u

/* PT profile B (CAN0) — alternate IDs/layout */
#define AEP_PT_PROFILE_B 1u
#define AEP_PT_FRAME_B_POWERTRAIN 0x211u
#define AEP_PT_FRAME_B_VEHICLE 0x212u

/* Legacy aliases (profile A) */
#define AEP_PT_FRAME_POWERTRAIN AEP_PT_FRAME_A_POWERTRAIN
#define AEP_PT_FRAME_TORQUE AEP_PT_FRAME_A_TORQUE

/* CAN1 cluster out (board module → host sniff) */
#define AEP_CLUSTER_FRAME_ID 0x301u

/* CAN1 dash in (host gen → board dash module) */
#define AEP_DASH_FRAME_STEER 0x401u
#define AEP_DASH_FRAME_STALKS 0x402u

/* Tier B demo dash pack */
#define AEP_PACK_DEMO_DASH 0xD001u
#define AEP_PACK_DEMO_DASH_REV 0x0001u
#define AEP_SIG_DASH_STEER_DEG 0xF010u
#define AEP_SIG_DASH_TURN_L 0xF011u
#define AEP_SIG_DASH_TURN_R 0xF012u
#define AEP_SIG_DASH_WIPER 0xF013u
#define AEP_SIG_DASH_HIGH_BEAM 0xF014u

/* UDS physical addressing on CAN0 (host ECU ↔ board client) */
#define AEP_PT_UDS_REQ_ID 0x7E0u
#define AEP_PT_UDS_RESP_ID 0x7E8u
#define AEP_PT_UDS_FUNC_ID 0x7DFu

/* Extension signal ids (PID path; publish optional) */
#define AEP_SIG_EXT_BOOST 0xF001u
#define AEP_SIG_EXT_FUEL_PRESS 0xF002u

#define AEP_DID_BOOST 0xF201u
#define AEP_DID_FUEL_PRESS 0xF202u

/* Component descriptor ids (stable for config records) */
#define AEP_COMP_ID_PT_PARSER 0x0001u
#define AEP_COMP_ID_PID_CLIENT 0x0002u
#define AEP_COMP_ID_CLUSTER 0x0003u
#define AEP_COMP_ID_DASH 0x0004u

/* NVM / host cfg blobs (materialized with core/table/demux in glue) */
typedef struct {
	uint8_t bus;
	uint8_t profile; /* AEP_PT_PROFILE_A/B */
} aep_pt_nvm_cfg_t;

typedef struct {
	uint8_t bus;
} aep_bus_nvm_cfg_t;

#ifdef __cplusplus
}
#endif

#endif /* AEP_DEMO_PT_H_ */
