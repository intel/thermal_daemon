"""Boolean command line options don't overwrite each other.

--ignore-critical-trip followed by --power-floor-enable and
--workaround-enabled, options declared next to it: the critical
trip must still be ignored. thermal-conf.xml has a critical trip at 90 C
on SEN2, next to a passive trip at 60 C that makes the zone active.

  1. SEN2 95 C, above the critical trip, for 15 s: thermald doesn't act
     on it (no "critical temp reached", no power off) and keeps running.
  2. SIGTERM: clean exit.
"""

CONFIG = """<?xml version="1.0"?>
<ThermalConfiguration>
<Platform>
	<Name>critical trip</Name>
	<ProductName>*</ProductName>
	<Preference>QUIET</Preference>
	<ThermalZones>
		<ThermalZone>
			<Type>SEN2</Type>
			<TripPoints>
				<TripPoint>
					<SensorType>SEN2</SensorType>
					<Temperature>60000</Temperature>
					<type>passive</type>
					<CoolingDevice>
						<Type>rapl_controller</Type>
					</CoolingDevice>
				</TripPoint>
				<TripPoint>
					<SensorType>SEN2</SensorType>
					<Temperature>90000</Temperature>
					<type>critical</type>
					<CoolingDevice>
						<Type>rapl_controller</Type>
					</CoolingDevice>
				</TripPoint>
			</TripPoints>
		</ThermalZone>
	</ThermalZones>
	<CoolingDevices>
		<CoolingDevice>
			<Type>rapl_controller</Type>
		</CoolingDevice>
	</CoolingDevices>
</Platform>
</ThermalConfiguration>
"""

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 40},
    "rapl": {"pl1_uw": 30000000, "pl2_uw": 61000000, "max_uw": 64000000,
             "demand_uw": 45000000},
    "thermald_args": ["--ignore-default-control",
                      "--config-file=/etc/thermald/thermal-conf.xml",
                      "--ignore-critical-trip", "--power-floor-enable",
                      "--workaround-enabled"],
    "conf_files": {"thermal-conf.xml": CONFIG},
}


def run(g):
    g.wait_ready()
    g.set_temp("SEN2", 95)
    g.hold(lambda: g.proc.poll() is None and "critical temp reached" not in g.log(), 15,
           lambda: "SEN2 95 C with --ignore-critical-trip: thermald acted on "
           "the critical trip%s" % (" and exited" if g.proc.poll() is not None else ""))
    g.check(g.stop_thermald() == 0, "exit status")
