# Third-party pins

## iso14229

| Field | Value |
| --- | --- |
| Upstream | https://github.com/driftregion/iso14229 |
| Tag | `0.11.0` (git describe) |
| Amalgamated sources | `iso14229/iso14229.c`, `iso14229/iso14229.h` |
| License | MIT (`iso14229/LICENSE`) |
| Embedded transport | SimonCahill `isotp-c` (inside the amalgamation) |

Sources are unmodified upstream amalgamation files from tag `0.11.0`. The upstream `VERSION` file still prints `0.10.2`; trust the git tag.

Do not vendor a second copy of `isotp-c`. Compile with:

```
-DUDS_SYS=0 -DUDS_TP_ISOTP_C
```

(`0` is `UDS_SYS_CUSTOM`.) Provide `UDSMillis`, `isotp_user_send_can`, and `isotp_user_get_us` — `diag_link.c` supplies them for the single active link.
