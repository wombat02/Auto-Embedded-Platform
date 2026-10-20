# ucp testing

```bash
make -C test test
```

Requires a host `gcc` (MinGW on Windows). Or run `./tools/run_host_tests.ps1` from the repo root.

On-target session: `python tools/ucp_host.py --port COMx`.
