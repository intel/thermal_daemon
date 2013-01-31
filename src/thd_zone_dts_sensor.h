/*
 * thd_zone_dts_sensor.h
 *
 *  Created on: Jan 25, 2013
 *      Author: srini
 */

#ifndef THD_ZONE_DTS_SENSOR_H_
#define THD_ZONE_DTS_SENSOR_H_

#include "thd_zone_dts.h"

class cthd_zone_dts_sensor : public cthd_zone_dts
{
private:
	int index;
	unsigned int read_cpu_mask();
	bool conf_present;

public:
	cthd_zone_dts_sensor(int count, int _index, std::string path);
	int read_trip_points();
	unsigned int read_zone_temp();

};



#endif /* THD_ZONE_DTS_SENSOR_H_ */
