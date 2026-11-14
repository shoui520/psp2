// unwind_pr.cpp: the C++ personality routine common to the EHABI routines (pr0, pr1 and pr2,
// whose descriptors list cleanups, catches and exception specifications) and to SNC's own
// (__snc_personality_v0, whose tables are call-site and action tables of ULEB128s), and the
// interpreter of the frame unwinding instructions both use.
#include <exception>
#include <typeinfo>
#include "cxxabi_internal.h"

#define R_SP	13
#define R_LR	14
#define R_PC	15

#define UNWIND_FINISH	0xb0	/* the instruction that ends a frame's unwinding */

extern "C" _Unwind_Reason_Code __ARM_unwind_cpp_prcommon(_Unwind_State state,
	_Unwind_Control_Block *ucbp, _Unwind_Context *context, unsigned int idx);

		// THE TABLES
struct uwrm_state
	{	// the unwinding instructions still to read
	unsigned int data;	// the current word, its next byte at the top
	unsigned int *next;	// the next word
	unsigned char bytes_left;	// in data
	unsigned char words_left;	// from next on
	};

static inline unsigned int read_uleb128(const unsigned char **p)
	{	// read an unsigned LEB128 number
	unsigned int value = 0;
	unsigned int shift = 0;
	unsigned char byte;

	do
		{	// seven bits at a time, low first
		byte = *(*p)++;
		value |= (byte & 0x7f) << shift;
		shift += 7;
		} while (byte & 0x80);
	return (value);
	}

static inline int read_sleb128(const unsigned char **p)
	{	// read a signed LEB128 number
	unsigned int value = 0;
	unsigned int shift = 0;
	unsigned char byte;

	do
		{	// seven bits at a time, low first
		byte = *(*p)++;
		value |= (byte & 0x7f) << shift;
		shift += 7;
		} while (byte & 0x80);
	if (shift < 32 && (byte & 0x40) != 0)
		value |= -(1 << shift);	// extend the sign
	return ((int)value);
	}

static inline unsigned int selfrel_offset31(const unsigned int *p)
	{	// the address a place-relative 31-bit offset names
	return ((unsigned int)p + ((int)(*p << 1) >> 1));
	}

static inline unsigned int get_core_register(_Unwind_Context *context, unsigned int regno)
	{	// get a core register of the frame
	unsigned int value;

	_Unwind_VRS_Get(context, _UVRSC_CORE, regno, _UVRSD_UINT32, &value);
	return (value);
	}

static inline void set_core_register(_Unwind_Context *context, unsigned int regno,
	unsigned int value)
	{	// set a core register of the frame
	_Unwind_VRS_Set(context, _UVRSC_CORE, regno, _UVRSD_UINT32, &value);
	}

const unsigned char *snc_lsda_header(unsigned int *ehtp, const unsigned char **call_site_end,
	const unsigned char **types)
	{	// find SNC's call-site table and type table in an exception table entry
	const unsigned char *lsda = (const unsigned char *)(ehtp + 2 + (ehtp[1] >> 24));
	const unsigned char *p = lsda + 2;

	if (lsda[1] != 0)
		*types = 0;
	else
		{	// the type table follows
		unsigned int offset = read_uleb128(&p);

		*types = p + offset;
		}
	++p;	// the call-site encoding
	unsigned int length = read_uleb128(&p);

	*call_site_end = p + length;
	return (p);
	}

static inline unsigned int next_unwind_byte(uwrm_state *uws)
	{	// read the next unwinding instruction byte
	if (uws->bytes_left == 0)
		{	// load the next word
		if (uws->words_left == 0)
			return (UNWIND_FINISH);
		uws->data = *uws->next++;
		--uws->words_left;
		uws->bytes_left = 4;
		}
	--uws->bytes_left;

	unsigned int byte = uws->data >> 24;

	uws->data <<= 8;
	return (byte);
	}

#define UNWIND_FAIL(arg) \
	{	/* report the end of unwinding, and fail */ \
	_Unwind_Activity(ucbp, _UAACT_ENDING, (arg)); \
	return (_URC_FAILURE); \
	}

#define UNWIND_POP(regclass, discriminator, representation) \
	if (_Unwind_VRS_Pop(context, (regclass), (discriminator), (representation)) \
		!= _UVRSR_OK) \
		UNWIND_FAIL(_UAARG_ENDING_VRSFAILED)

static inline _Unwind_Reason_Code unwind_execute(_Unwind_Control_Block *ucbp,
	_Unwind_Context *context, uwrm_state *uws, bool *pc_from_lr)
	{	// unwind the frame by its unwinding instructions
	bool wrote_pc = false;
	bool wrote_lr = false;

	for (;;)
		{	// interpret one instruction
		unsigned int op = next_unwind_byte(uws);

		if (op == UNWIND_FINISH)
			break;
		if (op < 0x40)	// sp += (xxxxxx << 2) + 4
			set_core_register(context, R_SP,
				get_core_register(context, R_SP) + ((op & 0x3f) << 2) + 4);
		else if (op < 0x80)	// sp -= (xxxxxx << 2) + 4
			set_core_register(context, R_SP,
				get_core_register(context, R_SP) - (((op & 0x3f) << 2) + 4));
		else if (op < 0x90)
			{	// pop r4-r15 under a mask
			unsigned int mask = (((op & 0x0f) << 8) | next_unwind_byte(uws)) << 4;

			if (mask == 0)
				UNWIND_FAIL(_UAARG_ENDING_NOUNWIND);	// refuse to unwind
			UNWIND_POP(_UVRSC_CORE, mask, _UVRSD_UINT32);
			if (mask & (1 << R_PC))
				wrote_pc = true;
			if (mask & (1 << R_LR))
				wrote_lr = true;
			}
		else if (op < 0xa0)
			{	// sp = a register
			unsigned int reg = op & 0x0f;

			if (reg == R_SP || reg == R_PC)
				UNWIND_FAIL(_UAARG_ENDING_CPP_BADOPCODE);
			set_core_register(context, R_SP, get_core_register(context, reg));
			}
		else if (op < 0xb0)
			{	// pop r4-r[4+nnn], and lr
			unsigned int mask = ((1 << ((op & 7) + 1)) - 1) << 4;

			if (op & 8)
				{	// and lr
				mask |= 1 << R_LR;
				wrote_lr = true;
				}
			UNWIND_POP(_UVRSC_CORE, mask, _UVRSD_UINT32);
			}
		else if (op < 0xb8)
			switch (op)
				{	// the 0xb1-0xb7 group
			case 0xb1:
				{	// pop r0-r3 under a mask
				unsigned int mask = next_unwind_byte(uws);

				if (mask == 0 || 0x0f < mask)
					UNWIND_FAIL(_UAARG_ENDING_CPP_BADOPCODE);
				UNWIND_POP(_UVRSC_CORE, mask, _UVRSD_UINT32);
				break;
				}

			case 0xb2:
				{	// sp += 0x204 + (uleb128 << 2)
				unsigned int offset = 0;
				unsigned int shift = 0;
				unsigned int byte;

				do
					{	// seven bits at a time, low first
					byte = next_unwind_byte(uws);
					offset |= (byte & 0x7f) << shift;
					shift += 7;
					} while (byte & 0x80);
				set_core_register(context, R_SP,
					get_core_register(context, R_SP) + 0x204 + (offset << 2));
				break;
				}

			case 0xb3:
				{	// pop VFP double registers saved by FSTMFDX
				unsigned int reg = next_unwind_byte(uws);

				UNWIND_POP(_UVRSC_VFP, ((reg >> 4) << 16) | ((reg & 0x0f) + 1),
					_UVRSD_VFPX);
				break;
				}

			default:
				UNWIND_FAIL(_UAARG_ENDING_CPP_BADOPCODE);
				}
		else if (op < 0xc0)	// pop d8-d[8+nnn] saved by FSTMFDX
			UNWIND_POP(_UVRSC_VFP, (8 << 16) | ((op & 7) + 1), _UVRSD_VFPX)
		else if (op < 0xc8)
			{	// iWMMXt registers
			if (op == 0xc7)
				{	// pop iWMMXt control registers under a mask
				unsigned int mask = next_unwind_byte(uws);

				if (mask == 0 || 0x0f < mask)
					UNWIND_FAIL(_UAARG_ENDING_CPP_BADOPCODE);
				UNWIND_POP(_UVRSC_WMMXC, mask, _UVRSD_UINT32);
				}
			else if (op == 0xc6)
				{	// pop iWMMXt data registers
				unsigned int reg = next_unwind_byte(uws);

				UNWIND_POP(_UVRSC_WMMXD, ((reg >> 4) << 16) | ((reg & 0x0f) + 1),
					_UVRSD_UINT64);
				}
			else	// pop wR10-wR[10+nnn]
				UNWIND_POP(_UVRSC_WMMXD, (10 << 16) | ((op & 7) + 1), _UVRSD_UINT64)
			}
		else if (op == 0xc8 || op == 0xc9)
			{	// pop VFP double registers saved by FSTMFDD
			unsigned int reg = next_unwind_byte(uws);
			unsigned int discriminator = ((reg >> 4) << 16) | ((reg & 0x0f) + 1);

			if (op == 0xc8)
				discriminator += 16 << 16;	// d16-d31
			UNWIND_POP(_UVRSC_VFP, discriminator, _UVRSD_DOUBLE);
			}
		else if (op < 0xd0)
			UNWIND_FAIL(_UAARG_ENDING_CPP_BADOPCODE)
		else if (op < 0xd8)	// pop d8-d[8+nnn] saved by FSTMFDD
			UNWIND_POP(_UVRSC_VFP, (8 << 16) | ((op & 7) + 1), _UVRSD_DOUBLE)
		else
			UNWIND_FAIL(_UAARG_ENDING_CPP_BADOPCODE)
		}

	if (!wrote_pc)
		{	// return through lr
		if (!wrote_lr)
			UNWIND_FAIL(_UAARG_ENDING_TABLECORRUPT);	// nothing gives the return address
		set_core_register(context, R_PC, get_core_register(context, R_LR));
		*pc_from_lr = true;
		}
	return (_URC_OK);
	}

static inline _Unwind_Reason_Code enter_landing_pad(_Unwind_Control_Block *ucbp,
	_Unwind_Context *context, void *object, unsigned int landing_pad, int selector)
	{	// enter one of SNC's landing pads
	ucbp->barrier_cache.bitpattern[0] = (unsigned int)object;
	set_core_register(context, 0, (unsigned int)ucbp);
	set_core_register(context, R_PC, landing_pad);
	set_core_register(context, 1, (unsigned int)selector);
	return (_URC_INSTALL_CONTEXT);
	}

extern "C" _Unwind_Reason_Code __ARM_unwind_cpp_prcommon(_Unwind_State state,
	_Unwind_Control_Block *ucbp, _Unwind_Context *context, unsigned int idx)
	{	// the common personality routine: idx 0-2 EHABI's, 4 SNC's
	unsigned int *ehtp = ucbp->pr_cache.ehtp;
	bool phase2_call_unexpected_after_unwind = false;
	void *matched_object;

	if (state != _US_VIRTUAL_UNWIND_FRAME && state != _US_UNWIND_FRAME_STARTING
		&& state != _US_UNWIND_FRAME_RESUME)
		{	// not a state this routine knows
		_Unwind_Activity(ucbp, _UAACT_ENDING, _UAARG_ENDING_UNSPECIFIED);
		return (_URC_FAILURE);
		}

	if (idx == 4)
		{	// SNC's call-site table
		const unsigned char *call_site_end;
		const unsigned char *types;
		const unsigned char *p = snc_lsda_header(ehtp, &call_site_end, &types);
		unsigned int ip = get_core_register(context, R_PC) - 2;
		unsigned int fnstart = ucbp->pr_cache.fnstart;
		unsigned int landing_pad;
		unsigned int action;

		for (;;)
			{	// find the call site of ip
			if (call_site_end <= p)
				{	// ip is in no call site
				__cxa_call_terminate(ucbp);
				goto unwind;
				}

			unsigned int start = read_uleb128(&p);
			unsigned int length = read_uleb128(&p);

			landing_pad = read_uleb128(&p);
			action = read_uleb128(&p);
			if (ip < fnstart + start)
				__cxa_call_terminate(ucbp);	// passed it
			if (ip < fnstart + start + length)
				break;
			}

		if (landing_pad == 0)
			goto unwind;	// no landing pad: nothing to do here
		landing_pad += fnstart & ~1;
		if (action == 0)
			{	// a cleanup alone
			if (state != _US_VIRTUAL_UNWIND_FRAME)
				return (enter_landing_pad(ucbp, context, 0, landing_pad, 0));
			goto unwind;
			}

		bool saw_cleanup = false;
		const unsigned char *ap = call_site_end + action - 1;

		for (;;)
			{	// walk the action chain
			int filter = read_sleb128(&ap);
			const unsigned char *q = ap;
			int next = read_sleb128(&q);

			if (filter == 0)
				saw_cleanup = true;
			else if (filter < 0)
				{	// an exception specification
				const unsigned char *list = types + -filter * 4 - 4;
				const unsigned char *spec = list;
				unsigned int count = 0;

				for (; *(const int *)list != 0; list += 4, ++count)
					if (__cxa_type_match(ucbp,
						(const _STD type_info *)(list + *(const int *)list), false,
						&matched_object))
						goto next_action;	// allowed

				if (state == _US_UNWIND_FRAME_STARTING)
					{	// record the specification for __cxa_call_unexpected
					ucbp->barrier_cache.bitpattern[0] = 0;
					ucbp->barrier_cache.bitpattern[1] = count;
					ucbp->barrier_cache.bitpattern[2] = 0;
					ucbp->barrier_cache.bitpattern[3] = 4;
					ucbp->barrier_cache.bitpattern[4] = (unsigned int)spec;
					set_core_register(context, 0, (unsigned int)ucbp);
					set_core_register(context, R_PC, landing_pad);
					set_core_register(context, 1, (unsigned int)filter);
					return (_URC_INSTALL_CONTEXT);
					}
				return (_URC_HANDLER_FOUND);
				}
			else
				{	// a catch
				const unsigned char *entry = types - filter * 4;
				int offset = *(const int *)entry;

				if (offset != 0)
					{	// a typed catch
					__cxa_type_match_result result = __cxa_type_match(ucbp,
						(const _STD type_info *)(entry + offset), false, &matched_object);

					if (result == ctm_succeeded_with_ptr_to_base
						&& state == _US_UNWIND_FRAME_STARTING)
						{	// catch the base pointer through a temporary
						ucbp->barrier_cache.bitpattern[4] = (unsigned int)matched_object;
						matched_object = &ucbp->barrier_cache.bitpattern[4];
						}
					if (result == ctm_failed)
						goto next_action;
					}
				if (state != _US_VIRTUAL_UNWIND_FRAME)
					return (enter_landing_pad(ucbp, context, matched_object, landing_pad,
						filter));
				return (_URC_HANDLER_FOUND);
				}
next_action:
			if (next == 0)
				break;
			ap += next;
			}
		if (saw_cleanup && state != _US_VIRTUAL_UNWIND_FRAME)
			return (enter_landing_pad(ucbp, context, 0, landing_pad, 0));
		}
	else if ((ucbp->pr_cache.additional & 1) == 0)
		{	// EHABI's descriptors
		unsigned int *data;

		if (state == _US_UNWIND_FRAME_RESUME)
			data = (unsigned int *)ucbp->cleanup_cache.bitpattern[0];
		else
			data = ehtp + (idx != 0 ? (ehtp[0] >> 16) & 0xff : 0) + 1;

		for (;;)
			{	// process one descriptor
			unsigned int length;
			unsigned int offset;

			if (idx == 2)
				{	// 32-bit scope
				length = data[0];
				if (length == 0)
					break;
				offset = data[1];
				data += 2;
				}
			else
				{	// 16-bit scope
				length = ((unsigned short *)data)[0];
				if (length == 0)
					break;
				offset = ((unsigned short *)data)[1];
				data += 1;
				}

			switch (((offset & 1) << 1) | (length & 1))
				{	// the kind of descriptor
			case 0:	// a cleanup
				if (state != _US_VIRTUAL_UNWIND_FRAME)
					{	// run it if the frame is in its scope
					unsigned int start = ucbp->pr_cache.fnstart + offset;
					unsigned int pc = get_core_register(context, R_PC);

					if (start <= pc && pc < start + length)
						{	// in scope
						ucbp->cleanup_cache.bitpattern[0] = (unsigned int)(data + 1);
						if (!__cxa_begin_cleanup(ucbp))
							{	// the runtime refuses
							_Unwind_Activity(ucbp, _UAACT_ENDING,
								_UAARG_ENDING_UNSPECIFIED);
							return (_URC_FAILURE);
							}

						unsigned int landing_pad = selfrel_offset31(data);

						set_core_register(context, R_PC, landing_pad);
						_Unwind_Activity(ucbp, _UAACT_PADENTRY, landing_pad);
						return (_URC_INSTALL_CONTEXT);
						}
					}
				data += 1;
				break;

			case 1:	// a catch
				if (state == _US_VIRTUAL_UNWIND_FRAME)
					{	// look for a handler
					unsigned int start = ucbp->pr_cache.fnstart + offset;
					unsigned int pc = get_core_register(context, R_PC);

					if (start <= pc && pc < start + length - 1)	// length & ~1
						{	// in scope
						unsigned int rtti = data[1];
						__cxa_type_match_result result;

						if (rtti == (unsigned int)-2)
							{	// a no-throw region
							_Unwind_Activity(ucbp, _UAACT_ENDING, _UAARG_ENDING_NOUNWIND);
							return (_URC_FAILURE);
							}
						if (rtti == (unsigned int)-1)
							{	// catch (...)
							matched_object = ucbp + 1;
							result = ctm_succeeded;
							}
						else
							result = __cxa_type_match(ucbp, (const _STD type_info *)rtti,
								(data[0] & 0x80000000) == 0x80000000, &matched_object);
						if (result != ctm_failed)
							{	// a barrier
							if (result == ctm_succeeded_with_ptr_to_base)
								{	// catch the base pointer through a temporary
								ucbp->barrier_cache.sp = get_core_register(context, R_SP);
								ucbp->barrier_cache.bitpattern[1] =
									(unsigned int)matched_object;
								matched_object = &ucbp->barrier_cache.bitpattern[1];
								}
							else
								ucbp->barrier_cache.sp = get_core_register(context, R_SP);
							ucbp->barrier_cache.bitpattern[0] = (unsigned int)matched_object;
							ucbp->barrier_cache.bitpattern[2] = (unsigned int)data;
							_Unwind_Activity(ucbp, _UAACT_BARRIERFOUND,
								selfrel_offset31(data));
							_Unwind_Activity(ucbp, _UAACT_CPP_TYPEINFO, rtti);
							return (_URC_HANDLER_FOUND);
							}
						}
					}
				else if (ucbp->barrier_cache.sp == get_core_register(context, R_SP)
					&& ucbp->barrier_cache.bitpattern[2] == (unsigned int)data)
					{	// the barrier phase 1 found
					unsigned int landing_pad = selfrel_offset31(data);

					set_core_register(context, R_PC, landing_pad);
					set_core_register(context, 0, (unsigned int)ucbp);
					_Unwind_Activity(ucbp, _UAACT_PADENTRY, landing_pad);
					return (_URC_INSTALL_CONTEXT);
					}
				data += 2;
				break;

			case 2:	// an exception specification
				{	// test the exception against it
				unsigned int spec = data[0];	// the count, and whether a landing pad follows
				unsigned int rtti_count = spec & 0x7fffffff;

				if (state == _US_VIRTUAL_UNWIND_FRAME)
					{	// look for a violation
					unsigned int start = ucbp->pr_cache.fnstart + offset - 1;	// offset & ~1
					unsigned int pc = get_core_register(context, R_PC);

					if (start <= pc && pc < start + length)
						{	// in scope
						unsigned int i;

						for (i = 0; i < rtti_count; ++i)
							if (__cxa_type_match(ucbp,
								(const _STD type_info *)data[i + 1], false,
								&matched_object))
								break;
						if (i == rtti_count)
							{	// a barrier
							ucbp->barrier_cache.sp = get_core_register(context, R_SP);
							ucbp->barrier_cache.bitpattern[0] = 0;
							ucbp->barrier_cache.bitpattern[2] = (unsigned int)data;
							_Unwind_Activity(ucbp, _UAACT_BARRIERFOUND,
								(unsigned int)__cxa_call_unexpected);
							return (_URC_HANDLER_FOUND);
							}
						}
					}
				else if (ucbp->barrier_cache.sp == get_core_register(context, R_SP)
					&& ucbp->barrier_cache.bitpattern[2] == (unsigned int)data)
					{	// the barrier phase 1 found
					ucbp->barrier_cache.bitpattern[1] = rtti_count;
					ucbp->barrier_cache.bitpattern[2] = 0;
					ucbp->barrier_cache.bitpattern[3] = 4;
					ucbp->barrier_cache.bitpattern[4] = (unsigned int)(data + 1);
					if (spec != rtti_count)
						{	// its own landing pad
						unsigned int landing_pad = selfrel_offset31(data + rtti_count + 1);

						set_core_register(context, 0, (unsigned int)ucbp);
						set_core_register(context, R_PC, landing_pad);
						_Unwind_Activity(ucbp, _UAACT_PADENTRY,
							(unsigned int)__cxa_call_unexpected);
						return (_URC_INSTALL_CONTEXT);
						}
					phase2_call_unexpected_after_unwind = true;
					}
				data += rtti_count + 1 + (spec != rtti_count ? 1 : 0);
				break;
				}

			default:
				_Unwind_Activity(ucbp, _UAACT_ENDING, _UAARG_ENDING_TABLECORRUPT);
				return (_URC_FAILURE);
				}
			}
		}

unwind:
	uwrm_state uws;

	uws.next = ehtp + 1;
	if (idx == 4)
		{	// SNC's: the word after the routine's
		uws.data = ehtp[1] << 8;
		uws.words_left = ehtp[1] >> 24;
		uws.bytes_left = 3;
		uws.next = ehtp + 2;
		}
	else if (idx != 0)
		{	// pr1 and pr2: a word count and two bytes
		uws.words_left = (ehtp[0] >> 16) & 0xff;
		uws.data = ehtp[0] << 16;
		uws.bytes_left = 2;
		}
	else
		{	// pr0: three bytes
		uws.data = ehtp[0] << 8;
		uws.words_left = 0;
		uws.bytes_left = 3;
		}
	bool pc_from_lr = false;

	if (unwind_execute(ucbp, context, &uws, &pc_from_lr) != _URC_OK)
		return (_URC_FAILURE);

	if (phase2_call_unexpected_after_unwind)
		{	// enter __cxa_call_unexpected as if from the call site
		set_core_register(context, 0, (unsigned int)ucbp);
		if (!pc_from_lr)
			set_core_register(context, R_LR, get_core_register(context, R_PC));
		set_core_register(context, R_PC, (unsigned int)__cxa_call_unexpected);
		_Unwind_Activity(ucbp, _UAACT_PADENTRY, (unsigned int)__cxa_call_unexpected);
		return (_URC_INSTALL_CONTEXT);
		}
	return (_URC_CONTINUE_UNWIND);
	}
