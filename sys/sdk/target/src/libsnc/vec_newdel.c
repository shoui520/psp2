/*
 * vec_newdel.c: SNC's array new and delete support (libsnc vec_newdel.o, EDG's runtime,
 * compiled as C++): the __cxa_vec_* entry points of the C++ ABI, which construct, copy and
 * destroy the elements of an array, and allocate and free it with its header (the element
 * size and count, before the first element). A constructor that throws destroys the
 * elements built so far and frees what was allocated here; a destructor that throws while
 * the array is destroyed lets the others run, and a second one terminates.
 */
#include <stddef.h>
/*
 * The runtime declares what it uses of the library itself: terminate() may throw (its
 * calls in a handler keep their cleanup), and the array-length exception's constructor
 * builds the bad_alloc with no message (the SDK's <new> passes one).
 */
namespace std {
void terminate();

class exception {
public:
	exception() throw();
	virtual ~exception() throw();
	virtual const char *what() const throw();
};

class bad_alloc : public exception {
public:
	bad_alloc() throw();
	bad_alloc(const bad_alloc &) throw();
	virtual ~bad_alloc() throw();
	virtual const char *what() const throw();
};

class bad_array_new_length : public bad_alloc {
public:
	bad_array_new_length() throw() {}
	virtual ~bad_array_new_length() throw();
};
}

#define FALSE	0
#define TRUE	1

typedef void *(*a_constructor_ptr)(void *obj);
typedef void *(*a_copy_constructor_ptr)(void *obj, void *src);
typedef void *(*a_destructor_ptr)(void *obj);
typedef void *(*an_allocation_function_ptr)(size_t size);
typedef void (*a_deallocation_function_ptr)(void *p);
typedef void (*a_sized_deallocation_function_ptr)(void *p, size_t size);

extern "C" void __memzero(void *p, size_t n);

extern "C" void __cxa_throw_bad_array_new_length(void)
{
	throw std::bad_array_new_length();
}

static void *array_new_general(void *array_address, int number_of_elements,
	size_t element_size, size_t header_size, void *source_array, a_constructor_ptr ctor,
	a_destructor_ptr dtor, an_allocation_function_ptr alloc,
	a_deallocation_function_ptr dealloc, int dealloc_takes_size, int zero_memory)
{	/* allocate (when array_address is NULL) and construct an array */
	int allocated_here = FALSE;
	int i = 0;
	int terminate_needed = FALSE;
	size_t size;
	char *p;

	if (array_address == NULL)
		allocated_here = TRUE;
	if (array_address != NULL && header_size == 0) {
		size = number_of_elements * element_size;
	} else {
		if (element_size != 0
			&& (size_t)number_of_elements > ((size_t)-1 - header_size) / element_size)
			throw std::bad_array_new_length();
		size = number_of_elements * element_size;
		if (size == 0)
			size = 1;
		if (array_address == NULL) {
			if (alloc != NULL)
				p = (char *)alloc(header_size + size);
			else
				p = (char *)operator new[](header_size + size);
			if (p != NULL)
				p += header_size;
			array_address = p;
			if (array_address == NULL)
				return array_address;
		}
		if (header_size != 0) {
			((size_t *)array_address)[-2] = element_size;
			((size_t *)array_address)[-1] = number_of_elements;
		}
	}
	if (zero_memory)
		__memzero(array_address, size);
	if (ctor == NULL)
		return array_address;
	try {
		for (i = 0, p = (char *)array_address; i < number_of_elements;
			i++, p += element_size) {
			if (source_array != NULL)
				((a_copy_constructor_ptr)ctor)(p, source_array);
			else
				ctor(p);
			if (source_array != NULL)
				source_array = (char *)source_array + element_size;
		}
	} catch (...) {
		if (dtor != NULL) {
			try {
				for (p -= element_size; i > 0; i--, p -= element_size)
					dtor(p);
			} catch (...) {
				terminate_needed = TRUE;
			}
			if (dealloc != NULL) {
				try {
					if (dealloc_takes_size)
						((a_sized_deallocation_function_ptr)dealloc)(
							(char *)array_address - header_size,
							header_size + element_size * number_of_elements);
					else
						dealloc((char *)array_address - header_size);
				} catch (...) {
				}
			}
			if (terminate_needed)
				std::terminate();
		} else if (allocated_here) {
			if (dealloc == NULL)
				operator delete[]((char *)array_address - header_size);
			else if (dealloc_takes_size)
				((a_sized_deallocation_function_ptr)dealloc)(
					(char *)array_address - header_size,
					header_size + element_size * number_of_elements);
			else
				dealloc((char *)array_address - header_size);
		}
		throw;
	}
	return array_address;
}

extern "C" void *__cxa_vec_new(size_t element_count, size_t element_size,
	size_t padding_size, a_constructor_ptr constructor, a_destructor_ptr destructor)
{
	return array_new_general(NULL, element_count, element_size, padding_size, NULL,
		constructor, destructor, NULL, NULL, FALSE, FALSE);
}

extern "C" void *__cxa_vec_new2(size_t element_count, size_t element_size,
	size_t padding_size, a_constructor_ptr constructor, a_destructor_ptr destructor,
	an_allocation_function_ptr alloc, a_deallocation_function_ptr dealloc)
{
	return array_new_general(NULL, element_count, element_size, padding_size, NULL,
		constructor, destructor, alloc, dealloc, FALSE, FALSE);
}

extern "C" void *__cxa_vec_new3(size_t element_count, size_t element_size,
	size_t padding_size, a_constructor_ptr constructor, a_destructor_ptr destructor,
	an_allocation_function_ptr alloc, a_sized_deallocation_function_ptr dealloc)
{
	return array_new_general(NULL, element_count, element_size, padding_size, NULL,
		constructor, destructor, alloc, (a_deallocation_function_ptr)dealloc, TRUE, FALSE);
}

extern "C" void *__cxa_vec_ctor(void *array_address, size_t element_count,
	size_t element_size, a_constructor_ptr constructor, a_destructor_ptr destructor)
{
	return array_new_general(array_address, element_count, element_size, 0, NULL,
		constructor, destructor, NULL, NULL, FALSE, FALSE);
}

extern "C" void *__cxa_vec_cctor(void *dest_array, void *src_array, size_t element_count,
	size_t element_size, a_copy_constructor_ptr constructor, a_destructor_ptr destructor)
{
	return array_new_general(dest_array, element_count, element_size, 0, src_array,
		(a_constructor_ptr)constructor, destructor, NULL, NULL, FALSE, FALSE);
}

static void array_delete_general(void *array_address, int number_of_elements,
	size_t element_size, size_t header_size, a_destructor_ptr dtor, int deallocate,
	a_deallocation_function_ptr dealloc, int dealloc_takes_size, int from_cleanup)
{	/* destroy an array's elements, last first, and free it (deallocate) */
	size_t i = 0;
	int rethrow_needed = FALSE;
	size_t size = 0;
	char *p;

	if (array_address == NULL)
		return;
	if (number_of_elements == -1 && header_size != 0) {
		number_of_elements = ((size_t *)array_address)[-1];
		size = element_size * number_of_elements;
	}
	if (dtor != NULL) {
		p = (char *)array_address + (number_of_elements * element_size - element_size);
		for (; i < (size_t)number_of_elements; i++, p -= element_size) {
			try {
				dtor(p);
			} catch (...) {
				if (from_cleanup || rethrow_needed)
					std::terminate();
				rethrow_needed = TRUE;
			}
		}
	}
	if (deallocate) {
		void *block = (char *)array_address - header_size;

		size += header_size;
		if (dealloc == NULL)
			operator delete[](block);
		else if (dealloc_takes_size)
			((a_sized_deallocation_function_ptr)dealloc)(block, size);
		else
			dealloc(block);
	}
	if (rethrow_needed)
		throw;
}

extern "C" void __cxa_vec_dtor(void *array_address, size_t element_count,
	size_t element_size, a_destructor_ptr destructor)
{
	array_delete_general(array_address, element_count, element_size, 0, destructor,
		FALSE, NULL, FALSE, FALSE);
}

extern "C" void __cxa_vec_delete(void *array_address, size_t element_size,
	size_t padding_size, a_destructor_ptr destructor)
{
	array_delete_general(array_address, -1, element_size, padding_size, destructor,
		TRUE, NULL, FALSE, FALSE);
}

extern "C" void __cxa_vec_delete2(void *array_address, size_t element_size,
	size_t padding_size, a_destructor_ptr destructor, a_deallocation_function_ptr dealloc)
{
	array_delete_general(array_address, -1, element_size, padding_size, destructor,
		TRUE, dealloc, FALSE, FALSE);
}

extern "C" void __cxa_vec_delete3(void *array_address, size_t element_size,
	size_t padding_size, a_destructor_ptr destructor,
	a_sized_deallocation_function_ptr dealloc)
{
	array_delete_general(array_address, -1, element_size, padding_size, destructor,
		TRUE, (a_deallocation_function_ptr)dealloc, TRUE, FALSE);
}

extern "C" void __cxa_vec_cleanup(void *array_address, size_t element_count,
	size_t element_size, a_destructor_ptr destructor)
{
	if (destructor != NULL)
		array_delete_general(array_address, element_count, element_size, 0, destructor,
			FALSE, NULL, FALSE, TRUE);
}
