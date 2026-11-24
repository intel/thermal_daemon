# thermald regression tests

Each scenario boots a small VM (virtme-ng, running kernel), creates a fake
platform in it and runs the thermald binary from this tree against it:

- **Thermal zones** with the names the tables refer to (e.g. `SEN2`), with
  temperatures set by the test: `test/module/thermald_test_kern_module.c`,
  `zone_names=` parameter.
- **RAPL**: fake `intel-rapl` and `intel-rapl-mmio` powercap domains from
  the same module (`rapl=1`). Package power is the lower of the demand and
  the enabled PL1s and drives `energy_uj`. Every write is logged. Power
  limit writes can be made to fail like a BIOS-locked part.
- **IETM**: an ACPI SSDT, loaded with `qemu -acpitable`, with an `INTC10D4`
  device whose `GDDV` returns the scenario's data vault. The real
  `int3400_thermal` driver binds to it and exposes `data_vault`, `imok`
  and `uuids`, as on a laptop.

Nothing on the host is touched: `/etc` in the VM is a throw-away overlay
holding only this repo's `data/` config files.

## Running

    test/regression/run.sh               # build, then run all scenarios
    test/regression/run.sh -k restore    # scenarios matching "restore"
    THERMALD_BIN=/path/to/thermald test/regression/run.sh

Results go to `/tmp/thermald-regression/<scenario>/` (`$THD_REG_OUT`):
`result.json` (status, a 0.5 s trace of temperatures and PL1, every RAPL
write), `thermald.log`, `dmesg.log`, `vm.log`, and the generated
`data_vault.bin` / `ietm.asl`.

Requirements: `virtme-ng`, `qemu-system-x86` with KVM, `acpica-tools`
(iasl), `python3-pytest`, and `kernel-devel` for the running kernel. The
tree must be configured with `prefix=/`, which is what the scenarios assume
for `/etc/thermald` and `/var/run/thermald`.

## Real data vaults

    test/regression/run.sh --vault-dir /private/vaults

runs `real/real_vault.py` once for every `/private/vaults/*.bin`, on a
platform built from that vault's own tables by `lib/real_vault.py`
(using `test/unit/build/gddv_dump` to parse it). Real vaults may contain
OEM data: keep them outside the repo. See `../README.md`.

## Writing a scenario

A scenario is a file in `scenarios/` with a `PLATFORM` description and a
`run(g)` function. `lib/platforms.py` has shared building blocks:

```python
import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

PLATFORM = {
    "zones": ["SEN2"],                     # thermal zones to create
    "initial_temps": {"SEN2": 35},         # C
    "rapl": rapl(),                        # fake RAPL, PL1 15 W at power on
    "switches": ["lid"],                   # uinput lid / tablet switches
    "upower": {"on_battery": False},       # dbusmock UPower
    "gddv": {                              # data vault, see lib/gddv.py
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"IETM.D0": idle_psvt()},
        "apct": [(2, [gddv.cond(gddv.LID_STATE, gddv.EQUAL, 0)]),
                 (1, [gddv.cond(gddv.DEFAULT)])],
        "apat": pl1max_target(1, "Open", 30) + pl1max_target(2, "Closed", 12),
    },
}

def run(g):
    g.wait_ready()
    g.set_lid_closed(True)
    g.wait_until(lambda: g.pl1("mmio") == 12, 20, "lid closed target")
```

Other `PLATFORM` keys: `cdevs` (extra cooling devices, e.g.
`intel_powerclamp`), `odvp` (OEM variables for Oem0-5 conditions),
`power_profile` (dbusmock power-profiles-daemon), `dbus` (private bus
for thermald itself), `thermald_args` (default `["--adaptive"]`),
`conf_files` (files for `/etc/thermald`), `cpu_model` (family 6 model,
default 0xaa Meteor Lake L) or `cpu` (raw `qemu -cpu`), `idsp`,
`ietm_hid`, `autostart`, and `known_bug` (see below). In `gddv`:
`ppcc`, `ppcc_nr_limits`, `psvt`, `apct`, `apat`, `vsct`, `vspt`,
`raw` (extra items, e.g. malformed tables) and `raw_vault`.

`g` (`lib/guest.py`) provides:

- inputs: `set_temp`, `set_demand`, `set_rapl_locked`, `set_lid_closed`,
  `set_tablet_mode`, `add_switch`, `remove_switch`, `set_on_battery`,
  `set_power_profile`, `reinit`, `unload_module`, `load_module` (the test
  module's zones, cooling devices and RAPL, e.g. to probe late)
- observations: `pl1`, `pl2`, `rapl_enabled`, `rapl_writes`, `temp`,
  `log`, `imok_values` (IETM.IMOK calls), `cpu_seconds`
- control: `wait_ready`, `wait_until`, `wait_log`, `hold`, `check`,
  `start_thermald`, `stop_thermald`

Wait on conditions with a timeout rather than sleeping a fixed time.
Messages can be functions, evaluated on failure, to show the state then.

When adding a scenario for a bug fix, check that it fails without the fix:
build the unfixed tree elsewhere and run it with `THERMALD_BIN`.

## Known bugs

A scenario with `"known_bug": "..."` in `PLATFORM` tests a thermald bug
that isn't fixed. It is reported as XFAIL while it fails, and fails once
it passes, so the marker is removed together with the fix.

| Scenario | Bug |
|---|---|
| `conditions_unsupported_fallback` | the highest-power fallback target is selected but never applied (`update_engine_state()` returns on "No change in target", since e39a74a) |
| `ppcc_pl1_only` | a PPCC with only PL1 (< 156 bytes) is dropped by `parse_ppcc()` |
| `reinit_while_throttled` | D-Bus Reinit replaces the adaptive engine with the default engine; PL1 stays throttled |

## Scenarios

| Area | Scenarios |
|---|---|
| PPCC power limits | `psvt_throttle_restore`, `ppcc_pl1_only`, `rapl_msr_parking`, `rapl_bios_locked` |
| Data vault | `vault_itmt_v1_short`, `vault_malformed_tables`, `vault_bad_count`, `vault_corrupt_header`, `vault_corrupt_adaptive_only_cpu` |
| Condition tables | `conditions_power_lid_tablet`, `conditions_oem_slider_temp`, `conditions_unsupported_fallback` |
| Temperature limits, restore | `psvt_throttle_restore`, `reinit_while_throttled` |
| Several zones, one cooling device | `multizone_shared_cdev` |
| Fixes | `virtual_sensors_two_participants` (6656673), `input_switch_removed` (PR #611), `notify_platform_imok` (PR #613), `ignore_adaptive_flow`, `late_devices_retry` |
