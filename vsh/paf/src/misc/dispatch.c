/* Object-owned virtual dispatch, reconstructed as C calls. */
#include <stdint.h>

typedef struct PafDispatchObject PafDispatchObject;

typedef struct PafDispatchMethods {
	uint32_t (*first)(PafDispatchObject *);
	uint32_t (*second)(PafDispatchObject *);
	uint32_t (*third)(PafDispatchObject *);
	uint32_t (*fourth)(PafDispatchObject *);
} PafDispatchMethods;

struct PafDispatchObject {
	const PafDispatchMethods *methods;
};

typedef struct PafDispatchOwner {
	uint8_t reserved[44];
	PafDispatchObject *target;
} PafDispatchOwner;

uint32_t ScePafMisc_7E7BC632(const PafDispatchOwner *owner)
{
	PafDispatchObject *target = owner->target;

	return target->methods->first(target);
}

uint32_t ScePafMisc_E56F76BF(const PafDispatchOwner *owner)
{
	PafDispatchObject *target = owner->target;

	return target->methods->second(target);
}

uint32_t ScePafMisc_B8A95C4E(const PafDispatchOwner *owner)
{
	PafDispatchObject *target = owner->target;

	return target->methods->third(target);
}

uint32_t ScePafMisc_12D2FAA7(const PafDispatchOwner *owner)
{
	PafDispatchObject *target = owner->target;

	return target->methods->fourth(target);
}
