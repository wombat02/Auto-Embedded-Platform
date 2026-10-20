/*
 * Tier A signal catalog — portable PT / vehicle-common semantics.
 * Append-only ids. Tier B/C use >= 0xF000 (or reserved pack blocks) outside this enum.
 */
#ifndef SIG_CATALOG_H_
#define SIG_CATALOG_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Revision counter (not major.minor). Bump when adding/changing Tier A rows. */
#define SIG_CATALOG_REVISION 0x0001u

typedef enum {
	SIG_UNIT_NONE = 0,
	SIG_UNIT_COUNT,
	SIG_UNIT_RAW,
	SIG_UNIT_BOOL,
	SIG_UNIT_ENUM,
	SIG_UNIT_RPM,
	SIG_UNIT_CELSIUS,
	SIG_UNIT_NM,
	SIG_UNIT_KPA,
	SIG_UNIT_PERCENT,
	SIG_UNIT_KMH,
	SIG_UNIT_VOLT,
	SIG_UNIT_AFR,
	SIG_UNIT_DEG,
	SIG_UNIT_GPS
} sig_unit_t;

typedef enum {
	SIG_T_BOOL = 0x01,
	SIG_T_U8 = 0x02,
	SIG_T_I8 = 0x03,
	SIG_T_U16 = 0x04,
	SIG_T_I16 = 0x05,
	SIG_T_U32 = 0x06,
	SIG_T_I32 = 0x07,
	SIG_T_CHAR = 0x08,
	SIG_T_FLAG32 = 0x10
} sig_type_t;

typedef enum {
	SIG_RATE_LIVE = 1,	 /* every TLM tick */
	SIG_RATE_HF = 2,
	SIG_RATE_MF = 3,
	SIG_RATE_LF = 4,
	SIG_RATE_CHANGE = 5 /* send when dirty; clear after send */
} sig_rate_t;

/*
 * Tier A ids (append only).
 * Scaling: eng = (raw * mult) / div + offset  (see sig_decode_eng).
 */
typedef enum {
	/* Engine / air / fuel (type-agnostic) */
	SIG_ENGINE_RPM = 0x0004,      /* u16; raw = rpm*4 */
	SIG_COOLANT_TEMP = 0x0005,    /* i16; eng = raw - 40 */
	SIG_ENGINE_TORQUE = 0x0006,   /* i16; eng = raw/10 (N·m) */
	SIG_ENGINE_LOAD = 0x0007,     /* u16; eng = raw/10 (%) */
	SIG_THROTTLE_POS = 0x0008,    /* u16; eng = raw/10 (%) */
	SIG_MAP_KPA = 0x0009,         /* u16; eng = raw/10 (kPa abs) */
	SIG_MAF_GPS = 0x000A,         /* u16; eng = raw/100 (g/s) */
	SIG_IAT = 0x000B,             /* i16; eng = raw - 40 (°C) */
	SIG_BOOST_KPA = 0x000C,       /* i16; eng = raw/10 (gauge kPa) */
	SIG_FUEL_PRESS_KPA = 0x000D,  /* u16; eng = raw (kPa) */
	SIG_OIL_PRESS_KPA = 0x000E,   /* u16; eng = raw/10 (kPa) */
	SIG_OIL_TEMP = 0x000F,        /* i16; eng = raw - 40 (°C) */
	SIG_LAMBDA = 0x0010,          /* u16; eng = raw/1000 (λ) */
	SIG_AFR = 0x0011,             /* u16; eng = raw/100 */
	SIG_IGN_ADV_DEG = 0x0012,     /* i16; eng = raw/10 (°) */
	SIG_INJ_PW_US = 0x0013,       /* u16; eng = raw (µs) */
	SIG_FUEL_DUTY = 0x0014,       /* u16; eng = raw/10 (%) */
	SIG_WASTEGATE_DUTY = 0x0015,  /* u16; eng = raw/10 (%) */
	SIG_ETHANOL_PCT = 0x0016,     /* u16; eng = raw/10 (%) */

	/* Driveline / chassis */
	SIG_VEHICLE_SPEED = 0x0020, /* u16; eng = raw/100 (km/h) */
	SIG_GEAR = 0x0021,          /* i8; eng = raw (enum/gear) */
	SIG_CLUTCH = 0x0022,        /* u8; 0/1 */
	SIG_BRAKE = 0x0023,         /* u8; 0/1 */
	SIG_ACCEL_PEDAL = 0x0024,   /* u16; eng = raw/10 (%) */
	SIG_WHEEL_FL_KMH = 0x0025,  /* u16; eng = raw/100 */
	SIG_WHEEL_FR_KMH = 0x0026,
	SIG_WHEEL_RL_KMH = 0x0027,
	SIG_WHEEL_RR_KMH = 0x0028,
	SIG_YAW_RATE = 0x0029,      /* i16; eng = raw/100 (°/s) */
	SIG_LAT_ACCEL = 0x002A,     /* i16; eng = raw/100 (m/s² scaled) */
	SIG_LONG_ACCEL = 0x002B,    /* i16; eng = raw/100 */

	/* Electrical / status */
	SIG_BATT_VOLT = 0x0030,     /* u16; eng = raw/100 (V) */
	SIG_ALT_STATUS = 0x0031,    /* u8 */
	SIG_MIL = 0x0032,           /* u8 */
	SIG_ENGINE_RUN = 0x0033,    /* u8 */
	SIG_DRIVE_MODE = 0x0034,    /* u8 enum */
	SIG_PT_FLAGS = 0x0035       /* FLAG32 */
} sig_id_t;

typedef struct {
	uint16_t id;
	uint8_t type;
	uint8_t unit;
	int16_t mult;
	uint16_t div;
	int16_t offset;
	uint16_t stale_ms;
	const char *name;
} sig_catalog_entry_t;

const sig_catalog_entry_t *sig_catalog_find(uint16_t id);
uint8_t sig_catalog_count(void);
const sig_catalog_entry_t *sig_catalog_at(uint8_t index);
uint16_t sig_catalog_revision(void);

/* Engineering value from raw bit pattern. Returns false if id unknown or div==0. */
bool sig_decode_eng(uint16_t id, uint8_t type, uint32_t raw_bits, int32_t *out_eng);

#ifdef __cplusplus
}
#endif

#endif /* SIG_CATALOG_H_ */
