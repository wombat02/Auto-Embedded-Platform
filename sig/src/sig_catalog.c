#include "sig_catalog.h"

static const sig_catalog_entry_t s_catalog[] = {
	/* Engine / air / fuel */
	{ SIG_ENGINE_RPM, SIG_T_U16, SIG_UNIT_RPM, 1, 4, 0, 200, "engine_rpm" },
	{ SIG_COOLANT_TEMP, SIG_T_I16, SIG_UNIT_CELSIUS, 1, 1, -40, 1000, "coolant_temp" },
	{ SIG_ENGINE_TORQUE, SIG_T_I16, SIG_UNIT_NM, 1, 10, 0, 500, "engine_torque" },
	{ SIG_ENGINE_LOAD, SIG_T_U16, SIG_UNIT_PERCENT, 1, 10, 0, 500, "engine_load" },
	{ SIG_THROTTLE_POS, SIG_T_U16, SIG_UNIT_PERCENT, 1, 10, 0, 200, "throttle_pos" },
	{ SIG_MAP_KPA, SIG_T_U16, SIG_UNIT_KPA, 1, 10, 0, 200, "map_kpa" },
	{ SIG_MAF_GPS, SIG_T_U16, SIG_UNIT_GPS, 1, 100, 0, 200, "maf_gps" },
	{ SIG_IAT, SIG_T_I16, SIG_UNIT_CELSIUS, 1, 1, -40, 1000, "iat" },
	{ SIG_BOOST_KPA, SIG_T_I16, SIG_UNIT_KPA, 1, 10, 0, 200, "boost_kpa" },
	{ SIG_FUEL_PRESS_KPA, SIG_T_U16, SIG_UNIT_KPA, 1, 1, 0, 500, "fuel_press_kpa" },
	{ SIG_OIL_PRESS_KPA, SIG_T_U16, SIG_UNIT_KPA, 1, 10, 0, 500, "oil_press_kpa" },
	{ SIG_OIL_TEMP, SIG_T_I16, SIG_UNIT_CELSIUS, 1, 1, -40, 1000, "oil_temp" },
	{ SIG_LAMBDA, SIG_T_U16, SIG_UNIT_RAW, 1, 1000, 0, 200, "lambda" },
	{ SIG_AFR, SIG_T_U16, SIG_UNIT_AFR, 1, 100, 0, 200, "afr" },
	{ SIG_IGN_ADV_DEG, SIG_T_I16, SIG_UNIT_DEG, 1, 10, 0, 200, "ign_adv_deg" },
	{ SIG_INJ_PW_US, SIG_T_U16, SIG_UNIT_RAW, 1, 1, 0, 200, "inj_pw_us" },
	{ SIG_FUEL_DUTY, SIG_T_U16, SIG_UNIT_PERCENT, 1, 10, 0, 500, "fuel_duty" },
	{ SIG_WASTEGATE_DUTY, SIG_T_U16, SIG_UNIT_PERCENT, 1, 10, 0, 500, "wastegate_duty" },
	{ SIG_ETHANOL_PCT, SIG_T_U16, SIG_UNIT_PERCENT, 1, 10, 0, 2000, "ethanol_pct" },

	/* Driveline / chassis */
	{ SIG_VEHICLE_SPEED, SIG_T_U16, SIG_UNIT_KMH, 1, 100, 0, 200, "vehicle_speed" },
	{ SIG_GEAR, SIG_T_I8, SIG_UNIT_ENUM, 1, 1, 0, 500, "gear" },
	{ SIG_CLUTCH, SIG_T_U8, SIG_UNIT_BOOL, 1, 1, 0, 200, "clutch" },
	{ SIG_BRAKE, SIG_T_U8, SIG_UNIT_BOOL, 1, 1, 0, 200, "brake" },
	{ SIG_ACCEL_PEDAL, SIG_T_U16, SIG_UNIT_PERCENT, 1, 10, 0, 200, "accel_pedal" },
	{ SIG_WHEEL_FL_KMH, SIG_T_U16, SIG_UNIT_KMH, 1, 100, 0, 200, "wheel_fl_kmh" },
	{ SIG_WHEEL_FR_KMH, SIG_T_U16, SIG_UNIT_KMH, 1, 100, 0, 200, "wheel_fr_kmh" },
	{ SIG_WHEEL_RL_KMH, SIG_T_U16, SIG_UNIT_KMH, 1, 100, 0, 200, "wheel_rl_kmh" },
	{ SIG_WHEEL_RR_KMH, SIG_T_U16, SIG_UNIT_KMH, 1, 100, 0, 200, "wheel_rr_kmh" },
	{ SIG_YAW_RATE, SIG_T_I16, SIG_UNIT_DEG, 1, 100, 0, 200, "yaw_rate" },
	{ SIG_LAT_ACCEL, SIG_T_I16, SIG_UNIT_RAW, 1, 100, 0, 200, "lat_accel" },
	{ SIG_LONG_ACCEL, SIG_T_I16, SIG_UNIT_RAW, 1, 100, 0, 200, "long_accel" },

	/* Electrical / status */
	{ SIG_BATT_VOLT, SIG_T_U16, SIG_UNIT_VOLT, 1, 100, 0, 1000, "batt_volt" },
	{ SIG_ALT_STATUS, SIG_T_U8, SIG_UNIT_ENUM, 1, 1, 0, 1000, "alt_status" },
	{ SIG_MIL, SIG_T_U8, SIG_UNIT_BOOL, 1, 1, 0, 1000, "mil" },
	{ SIG_ENGINE_RUN, SIG_T_U8, SIG_UNIT_BOOL, 1, 1, 0, 500, "engine_run" },
	{ SIG_DRIVE_MODE, SIG_T_U8, SIG_UNIT_ENUM, 1, 1, 0, 1000, "drive_mode" },
	{ SIG_PT_FLAGS, SIG_T_FLAG32, SIG_UNIT_RAW, 1, 1, 0, 500, "pt_flags" },
};

uint16_t sig_catalog_revision(void)
{
	return SIG_CATALOG_REVISION;
}

uint8_t sig_catalog_count(void)
{
	return (uint8_t)(sizeof(s_catalog) / sizeof(s_catalog[0]));
}

const sig_catalog_entry_t *sig_catalog_at(uint8_t index)
{
	if (index >= sig_catalog_count())
	{
		return NULL;
	}
	return &s_catalog[index];
}

const sig_catalog_entry_t *sig_catalog_find(uint16_t id)
{
	uint8_t i;

	for (i = 0u; i < sig_catalog_count(); i++)
	{
		if (s_catalog[i].id == id)
		{
			return &s_catalog[i];
		}
	}
	return NULL;
}

static int64_t sig_sign_extend(uint8_t type, uint32_t raw_bits)
{
	switch (type)
	{
	case SIG_T_I8:
		return (int64_t)(int8_t)(raw_bits & 0xFFu);
	case SIG_T_I16:
		return (int64_t)(int16_t)(raw_bits & 0xFFFFu);
	case SIG_T_I32:
		return (int64_t)(int32_t)raw_bits;
	case SIG_T_BOOL:
		return (raw_bits != 0u) ? 1 : 0;
	default:
		return (int64_t)(uint64_t)raw_bits;
	}
}

bool sig_decode_eng(uint16_t id, uint8_t type, uint32_t raw_bits, int32_t *out_eng)
{
	const sig_catalog_entry_t *e = sig_catalog_find(id);
	int64_t raw;
	int64_t eng;

	if ((out_eng == NULL) || (e == NULL) || (e->div == 0u))
	{
		return false;
	}
	(void)type;
	raw = sig_sign_extend(e->type, raw_bits);
	eng = (raw * (int64_t)e->mult) / (int64_t)e->div + (int64_t)e->offset;
	*out_eng = (int32_t)eng;
	return true;
}
