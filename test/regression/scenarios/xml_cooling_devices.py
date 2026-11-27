"""thermal-conf.xml trips on each kind of cooling device other than RAPL:
intel_pstate, cpufreq, intel_powerclamp, the backlight (LCD) and a
generic sysfs cooling device defined in the XML.

One zone, "skin", with one passive trip per device, each on its own
sensor (SEN1-SEN5, trips at 50 C, 2 C hysteresis). The trips have no
TargetState: thermald steps each device on its own while its sensor is
above the trip. Default control is off, so only these trips act.

  1. 60 C on all sensors: each device reaches its limit:
     - intel_pstate: max_perf_pct 0 (state 10 of 10), turbo off
     - cpufreq: maximum 800 MHz
     - intel_powerclamp: 100
     - LCD: brightness down to 25 % of max_brightness (thermald's floor),
       never below
     - gen_test (/sys/kernel/thermald_test/gen_cdev, states 0-10, step 2):
       10
  2. 45 C, below trip - hysteresis: each device back at its unthrottled
     value: max_perf_pct 100 and turbo on, 3.2 GHz, powerclamp 0, the
     brightness the backlight had before throttling, gen_test 0.
  3. SIGTERM: exit status 0.
"""

from default_engine import MAX_KHZ, MIN_KHZ

BL_MAX, BL_START = 1000, 800
BL_FLOOR = BL_MAX * 25 // 100
SENSORS = ["SEN1", "SEN2", "SEN3", "SEN4", "SEN5"]
DEVICES = ["intel_pstate", "cpufreq", "intel_powerclamp", "LCD", "gen_test"]


def trip(sensor, cdev):
    return """				<TripPoint>
					<SensorType>%s</SensorType>
					<Temperature>50000</Temperature>
					<Hyst>2000</Hyst>
					<type>passive</type>
					<CoolingDevice>
						<type>%s</type>
					</CoolingDevice>
				</TripPoint>
""" % (sensor, cdev)


CONFIG = """<?xml version="1.0"?>
<ThermalConfiguration>
<Platform>
	<Name>cooling device test</Name>
	<ProductName>*</ProductName>
	<Preference>QUIET</Preference>
	<CoolingDevices>
		<CoolingDevice>
			<Type>gen_test</Type>
			<Path>/sys/kernel/thermald_test/gen_cdev</Path>
			<MinState>0</MinState>
			<MaxState>10</MaxState>
			<IncDecStep>2</IncDecStep>
			<ReadBack>1</ReadBack>
		</CoolingDevice>
	</CoolingDevices>
	<ThermalZones>
		<ThermalZone>
			<Type>skin</Type>
			<TripPoints>
%s			</TripPoints>
		</ThermalZone>
	</ThermalZones>
</Platform>
</ThermalConfiguration>
""" % "".join(trip(s, d) for s, d in zip(SENSORS, DEVICES))

PLATFORM = {
    "zones": SENSORS,
    "initial_temps": {s: 40 for s in SENSORS},
    "cdevs": ["intel_powerclamp"],
    "intel_pstate": True,
    "cpufreq": True,
    "backlight": True,
    "thermald_args": ["--ignore-cpuid-check", "--ignore-default-control",
                      "--config-file=/etc/thermald/thermal-conf.xml"],
    "conf_files": {"thermal-conf.xml": CONFIG},
}


def state(g):
    return ("max_perf_pct %d no_turbo %d, cpufreq max %d kHz, powerclamp %d, "
            "brightness %d, gen_cdev %d"
            % (g.pstate(), g.pstate("no_turbo"), g.cpufreq_max_khz(),
               g.cdev_state("intel_powerclamp"), g.brightness(),
               g.gen_cdev()))


def run(g):
    g.wait_ready()
    g.check(g.brightness() == BL_START, "brightness %d at start"
            % g.brightness())
    for s in SENSORS:
        g.set_temp(s, 60)
    g.wait_until(lambda: g.pstate() == 0 and g.cpufreq_max_khz() == MIN_KHZ
                 and g.cdev_state("intel_powerclamp") == 100
                 and g.brightness() == BL_FLOOR and g.gen_cdev() == 10, 120,
                 lambda: "all devices at their limit: " + state(g))
    g.check(g.pstate("no_turbo") == 1, "turbo still on: " + state(g))
    low = [w["value"] for w in g.cdev_writes("intel_backlight", "brightness")]
    g.check(min(low) >= BL_FLOOR, "brightness below %d: %s" % (BL_FLOOR, low))
    gen = [w["value"] for w in g.cdev_writes("gen_cdev")]
    g.check(all(0 <= v <= 10 for v in gen), "gen_cdev out of 0-10: %s" % gen)

    for s in SENSORS:
        g.set_temp(s, 45)
    g.wait_until(lambda: g.pstate() == 100 and g.pstate("no_turbo") == 0
                 and g.cpufreq_max_khz() == MAX_KHZ
                 and g.cdev_state("intel_powerclamp") == 0
                 and g.brightness() == BL_START and g.gen_cdev() == 0, 120,
                 lambda: "all devices released: " + state(g))

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
