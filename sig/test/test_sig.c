#include "sig_catalog.h"
#include "sig_table.h"
#include "sig_tlv.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

static void expect_true(const char *name, int cond)
{
	if (!cond)
	{
		printf("FAIL: %s\n", name);
		g_failures++;
	}
	else
	{
		printf("ok: %s\n", name);
	}
}

static void test_append_and_walk(void)
{
	uint8_t buf[64];
	uint8_t used = 0u;
	sig_tlv_cursor_t cur;
	uint16_t id;
	uint8_t type;
	uint8_t qflags;
	uint32_t raw;

	/* Record size = 4 + W: U16=6, I16=6, FLAG32=8 */
	used = (uint8_t)(used + sig_tlv_append(buf, sizeof(buf), used, SIG_ENGINE_RPM, SIG_T_U16,
										   SIG_TLV_Q_VALID, 42u));
	used = (uint8_t)(used + sig_tlv_append(buf, sizeof(buf), used, SIG_COOLANT_TEMP, SIG_T_I16,
										   SIG_TLV_Q_VALID, (uint32_t)(int16_t)-150));
	used = (uint8_t)(used + sig_tlv_append(buf, sizeof(buf), used, SIG_PT_FLAGS, SIG_T_FLAG32,
										   SIG_TLV_Q_AGED, 0x00000005u));
	expect_true("append three records", used == (6u + 6u + 8u));

	sig_tlv_cursor_init(&cur, buf, used);
	expect_true("next rpm",
				sig_tlv_next(&cur, &id, &type, &qflags, &raw) && id == SIG_ENGINE_RPM &&
					raw == 42u && qflags == SIG_TLV_Q_VALID);
	expect_true("next coolant",
				sig_tlv_next(&cur, &id, &type, &qflags, &raw) && id == SIG_COOLANT_TEMP &&
					(int16_t)(raw & 0xFFFFu) == -150 && qflags == SIG_TLV_Q_VALID);
	expect_true("next flags aged",
				sig_tlv_next(&cur, &id, &type, &qflags, &raw) && id == SIG_PT_FLAGS &&
					raw == 5u && qflags == SIG_TLV_Q_AGED);
	expect_true("end", !sig_tlv_next(&cur, &id, &type, &qflags, &raw));
}

static void test_skip_unknown_id(void)
{
	uint8_t buf[32];
	uint8_t used = 0u;
	sig_tlv_cursor_t cur;
	uint16_t id;
	uint8_t type;
	uint8_t qflags;
	uint32_t raw;
	int saw_unknown = 0;
	int saw_known = 0;

	used = (uint8_t)(used + sig_tlv_append(buf, sizeof(buf), used, 0x00AAu, SIG_T_U16,
										   SIG_TLV_Q_VALID, 7u));
	used = (uint8_t)(used + sig_tlv_append(buf, sizeof(buf), used, SIG_ENGINE_RPM, SIG_T_U16,
										   SIG_TLV_Q_VALID, 9u));
	sig_tlv_cursor_init(&cur, buf, used);
	while (sig_tlv_next(&cur, &id, &type, &qflags, &raw))
	{
		if (id == 0x00AAu)
		{
			saw_unknown = 1;
		}
		if (id == SIG_ENGINE_RPM && raw == 9u)
		{
			saw_known = 1;
		}
	}
	expect_true("unknown id returned", saw_unknown);
	expect_true("known id after unknown", saw_known);
}

static void test_unknown_type_stops(void)
{
	/* id=1, type=0x99 unknown, qflags=0, then junk */
	uint8_t buf[16] = { 0x01, 0x00, 0x99, 0x00, 0x02, 0x00, 0x04, 0x01, 0x00 };
	sig_tlv_cursor_t cur;
	uint16_t id;
	uint8_t type;
	uint8_t qflags;
	uint32_t raw;

	sig_tlv_cursor_init(&cur, buf, sizeof(buf));
	expect_true("unknown type stops", !sig_tlv_next(&cur, &id, &type, &qflags, &raw));
}

static void test_scale_display(void)
{
	int32_t eng = 0;

	/* Coolant: eng = raw - 40 */
	expect_true("coolant eng",
				sig_decode_eng(SIG_COOLANT_TEMP, SIG_T_I16, (uint32_t)(int16_t)(90 + 40), &eng) &&
					eng == 90);
	/* RPM: raw = rpm*4 => eng = raw/4 */
	expect_true("rpm eng", sig_decode_eng(SIG_ENGINE_RPM, SIG_T_U16, 3000u * 4u, &eng) && eng == 3000);
	/* Torque: eng = raw/10 */
	expect_true("torque eng",
				sig_decode_eng(SIG_ENGINE_TORQUE, SIG_T_I16, (uint32_t)(int16_t)(250 * 10), &eng) &&
					eng == 250);
	expect_true("catalog revision", sig_catalog_revision() == SIG_CATALOG_REVISION);
	expect_true("catalog has pt rows", sig_catalog_count() >= 30u);
	expect_true("vss eng", sig_decode_eng(SIG_VEHICLE_SPEED, SIG_T_U16, 10000u, &eng) && eng == 100);
	expect_true("batt eng", sig_decode_eng(SIG_BATT_VOLT, SIG_T_U16, 1380u, &eng) && eng == 13);
}

static void test_table_bind_write(void)
{
	sig_table_t t;
	sig_snapshot_t snap;
	uint32_t raw = 0u;
	uint8_t valid = 0u;

	sig_table_init(&t);
	sig_table_rebind_begin(&t);
	expect_true("bind rpm", sig_table_bind(&t, SIG_ENGINE_RPM, SIG_T_U16, SIG_RATE_HF));
	expect_true("bind flags", sig_table_bind(&t, SIG_PT_FLAGS, SIG_T_FLAG32, SIG_RATE_LF));
	expect_true("bind coolant", sig_table_bind(&t, SIG_COOLANT_TEMP, SIG_T_I16, SIG_RATE_HF));
	expect_true("write", sig_table_write(&t, SIG_ENGINE_RPM, 11u, 1u, 1000u));
	expect_true("read",
				sig_table_read(&t, SIG_ENGINE_RPM, &raw, &valid, NULL) && raw == 11u && valid == 1u);
	sig_table_snapshot(&t, &snap);
	expect_true("snapshot gen", snap.generation != 0u && snap.count == 3u);
	expect_true("default publish", snap.rec[0].publish == 1u);
	expect_true("set unpublish", sig_table_set_publish(&t, SIG_ENGINE_RPM, 0u));
	sig_table_snapshot(&t, &snap);
	expect_true("unpublished", snap.rec[0].publish == 0u);
}

static void test_table_age(void)
{
	sig_table_t t;
	uint32_t raw = 0u;
	uint8_t valid = 1u;
	const sig_catalog_entry_t *cat = sig_catalog_find(SIG_ENGINE_RPM);

	sig_table_init(&t);
	sig_table_rebind_begin(&t);
	expect_true("age bind", sig_table_bind(&t, SIG_ENGINE_RPM, SIG_T_U16, SIG_RATE_HF));
	expect_true("age write", sig_table_write(&t, SIG_ENGINE_RPM, 99u, 1u, 1000u));
	expect_true("catalog stale", cat != NULL && cat->stale_ms > 0u);

	sig_table_age(&t, 1000u + (uint32_t)cat->stale_ms - 1u);
	expect_true("not yet aged",
				sig_table_read(&t, SIG_ENGINE_RPM, &raw, &valid, NULL) && valid == 1u &&
					raw == 99u);

	sig_table_age(&t, 1000u + (uint32_t)cat->stale_ms);
	expect_true("aged clears valid",
				sig_table_read(&t, SIG_ENGINE_RPM, &raw, &valid, NULL) && valid == 0u &&
					raw == 99u);
	expect_true("aged flag set", t.slot[0].aged == 1u);

	expect_true("rewrite clears aged", sig_table_write(&t, SIG_ENGINE_RPM, 100u, 1u, 2000u));
	expect_true("valid again",
				sig_table_read(&t, SIG_ENGINE_RPM, &raw, &valid, NULL) && valid == 1u &&
					t.slot[0].aged == 0u);
}

static void test_change_dirty_clear(void)
{
	sig_table_t t;

	sig_table_init(&t);
	sig_table_rebind_begin(&t);
	expect_true("bind change", sig_table_bind(&t, SIG_PT_FLAGS, SIG_T_FLAG32, SIG_RATE_CHANGE));
	expect_true("write dirty", sig_table_write(&t, SIG_PT_FLAGS, 1u, 1u, 1000u));
	expect_true("dirty set", t.slot[0].dirty == 1u);
	expect_true("clear dirty", sig_table_clear_dirty(&t, SIG_PT_FLAGS));
	expect_true("dirty cleared", t.slot[0].dirty == 0u);
	expect_true("rates live/mf", SIG_RATE_LIVE == 1 && SIG_RATE_MF == 3 && SIG_RATE_CHANGE == 5);
}

int main(void)
{
	g_failures = 0;
	test_append_and_walk();
	test_skip_unknown_id();
	test_unknown_type_stops();
	test_scale_display();
	test_table_bind_write();
	test_table_age();
	test_change_dirty_clear();
	if (g_failures != 0)
	{
		printf("%d failure(s)\n", g_failures);
		return 1;
	}
	printf("all sig tests passed\n");
	return 0;
}
