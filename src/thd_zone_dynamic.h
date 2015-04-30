/*
 * thd_zone_dynamic.h
 *
 *  Created on: Sep 19, 2014
 *      Author: spandruvada
 */

#ifndef THD_ZONE_DYNAMIC_H_
#define THD_ZONE_DYNAMIC_H_

#include "thd_zone.h"

class cthd_zone_dynamic: public cthd_zone {
private:
	std::string name;
	unsigned int trip_temp;
	trip_point_type_t trip_type;
	std::string sensor_name;
	std::string cdev_name;

public:

	cthd_zone_dynamic(int index, std::string _name, unsigned int _trip_temp, trip_point_type_t _trip_type,
			std::string _sensor, std::string _cdev);

	virtual int read_trip_points();
	virtual int read_cdev_trip_points();
	virtual int zone_bind_sensors();
};

#endif /* THD_ZONE_DYNAMIC_H_ */
