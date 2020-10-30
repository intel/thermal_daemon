#ifndef THD_ADAPTIVE_TYPES_H_
#define THD_ADAPTIVE_TYPES_H_

#include <stdint.h>

struct adaptive_target {
	uint64_t target_id;
	std::string name;
	std::string participant;
	uint64_t domain;
	std::string code;
	std::string argument;
};

#endif /* THD_ADAPTIVE_TYPES_H_ */
