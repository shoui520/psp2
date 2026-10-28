#include "string.h"
#include "new.h"

int PafStringVectorReallocate(PafStringVector *self, size_t capacity)
{
	PafString *data;
	size_t i;

	if (self->_data) {
		data = ScePafStdc_FC5CD359(sizeof(*data) * capacity);
		if (!data) {
			ScePafStdc_D608246A();
			return 0;
		}
		for (i = 0; i < self->_size; ++i) {
			PafString *value = PafPlacementNew(sizeof(*value), &data[i]);

			if (value) {
				PafStringInitialize(value);
				PafStringCopy(value, &self->_data[i]);
			}
		}
		for (i = 0; i < self->_size; ++i)
			PafStringDestroy(&self->_data[i]);
		ScePafStdc_1B77082E(self->_data);
		self->_data = data;
		self->_capacity = capacity;
		return 1;
	}
	data = ScePafStdc_FC5CD359(sizeof(*data) * capacity);
	if (!data)
		ScePafStdc_D608246A();
	self->_data = data;
	if (!data)
		return 0;
	self->_capacity = capacity;
	return 1;
}
