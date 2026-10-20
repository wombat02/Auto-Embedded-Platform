# ucp_tlm testing

No separate `make` target yet; coverage is via `ucp` host tests and on-target:

```powershell
python tools/ucp_host.py --port COMx
```

Expect Tier A / extension signals from the PT demo to update when CAN workers and the board are running.
