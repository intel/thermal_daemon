"""thermal-conf.xml: a trip's PidControl in a zone the XML defines itself
(not an existing thermal zone) controls the cooling device with the PID.

Zone "back_cover_pid" on sensor SEN2, passive trip at 48 C (hysteresis
2 C), cooling device rapl_controller (MSR PL1) with an incremental PID
(Kp 100, Ki 0, Kd 280).

  1. The XML zone is loaded with its PID values.
  2. SEN2 50 C: PL1 is set by the PID ("Set pid"), not by the default
     exponential steps.
"""

CONFIG = """<?xml version="1.0"?>
<ThermalConfiguration>
<Platform>
	<Name>PID in a new XML zone</Name>
	<ProductName>*</ProductName>
	<Preference>QUIET</Preference>
	<ThermalZones>
		<ThermalZone>
			<Type>back_cover_pid</Type>
			<TripPoints>
				<TripPoint>
					<SensorType>SEN2</SensorType>
					<Temperature>48000</Temperature>
					<Hyst>2000</Hyst>
					<type>passive</type>
					<CoolingDevice>
						<Type>rapl_controller</Type>
						<PidControl>
							<Kp>100</Kp>
							<Ki>0</Ki>
							<Kd>280</Kd>
							<PidMode>incremental</PidMode>
						</PidControl>
					</CoolingDevice>
				</TripPoint>
			</TripPoints>
		</ThermalZone>
	</ThermalZones>
</Platform>
</ThermalConfiguration>
"""

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 40},
    "rapl": {"pl1_uw": 30000000, "pl2_uw": 61000000, "max_uw": 64000000,
             "demand_uw": 45000000},
    "thermald_args": ["--ignore-default-control",
                      "--config-file=/etc/thermald/thermal-conf.xml"],
    "conf_files": {"thermal-conf.xml": CONFIG},
}


def run(g):
    g.wait_ready()
    g.check("back_cover_pid" in g.log(), "XML zone not loaded")
    g.check("PID values 100" in g.log(), "XML PID values not parsed")
    g.set_temp("SEN2", 50)
    g.wait_until(lambda: "Set pid" in g.log() or "increment exponentially" in g.log(), 30,
                 "SEN2 50 C: no PL1 control")
    g.hold(lambda: True, 10, "")
    log = g.log()
    g.check("Set pid" in log, "SEN2 50 C: the trip's PID wasn't used")
    g.check("increment exponentially" not in log,
            "SEN2 50 C: the default exponential control was used instead of the PID")
    g.check(g.stop_thermald() == 0, "exit status")
