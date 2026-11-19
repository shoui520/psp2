/*
 * SceLibPvf's copy of a face's GSUB table (pvf_gsub.c): the table read into the library's
 * memory, word by word, so that a glyph's vertical form can be looked up. Laid out from the
 * firmware; the field names follow OpenType's (agent/namecheck).
 */
#ifndef LIBPVF_PVF_GSUB_H
#define LIBPVF_PVF_GSUB_H

#include "libpvf_internal.h"

/* a Coverage table (format 1: glyphs; format 2: ranges) */
typedef struct PvfGsubCoverage {
	SceInt32 format;
	SceInt32 sorted;		/* ascending: searched by halves */
	SceInt32 count;
	void *table;			/* SceInt32 glyphs[] or PvfGsubRangeRecord ranges[] */
} PvfGsubCoverage;

typedef struct PvfGsubRangeRecord {
	SceInt32 start;
	SceInt32 end;
	SceInt32 startCoverageIndex;
	SceInt32 reserved;		/* 0 */
} PvfGsubRangeRecord;

typedef struct PvfGsubLangSys {
	SceInt32 lookupOrder;
	SceInt32 reqFeatureIndex;
	SceInt32 featureCount;
	SceInt32 *featureIndices;
} PvfGsubLangSys;

typedef struct PvfGsubLangSysRecord {
	SceUInt32 tag;
	PvfGsubLangSys langSys;
} PvfGsubLangSysRecord;

typedef struct PvfGsubScript {
	SceUInt32 tag;
	PvfGsubLangSys defaultLangSys;
	SceInt32 langSysCount;
	PvfGsubLangSysRecord *langSys;
} PvfGsubScript;

typedef struct PvfGsubFeature {
	SceUInt32 tag;
	SceInt32 featureParams;
	SceInt32 lookupCount;
	SceInt32 *lookupListIndices;
} PvfGsubFeature;

/* a counted array: a Sequence, an AlternateSet, a LigatureSet or a (Chain)SubRuleSet */
typedef struct PvfGsubSet {
	SceInt32 count;
	void *items;
} PvfGsubSet;

typedef struct PvfGsubLigature {
	SceInt32 ligGlyph;
	SceInt32 compCount;
	SceInt32 *components;		/* compCount - 1 */
} PvfGsubLigature;

typedef struct PvfGsubLookupRecord {
	SceInt32 sequenceIndex;
	SceInt32 lookupListIndex;
} PvfGsubLookupRecord;

/* a SubRule or SubClassRule */
typedef struct PvfGsubRule {
	SceInt32 glyphCount;
	SceInt32 *input;		/* glyphCount - 1 */
	SceInt32 substCount;
	PvfGsubLookupRecord *substLookupRecords;
} PvfGsubRule;

/* a ChainSubRule or ChainSubClassRule */
typedef struct PvfGsubChainRule {
	SceInt32 backtrackCount;
	SceInt32 *backtrack;
	SceInt32 inputCount;
	SceInt32 *input;		/* inputCount - 1 */
	SceInt32 lookaheadCount;
	SceInt32 *lookahead;
	SceInt32 substCount;
	PvfGsubLookupRecord *substLookupRecords;
} PvfGsubChainRule;

/* a lookup subtable (44 bytes), by lookup type and format */
typedef struct PvfGsubSubTable {
	SceInt32 format;
	union {
		struct {			/* single substitution, format 1 */
			PvfGsubCoverage coverage;
			SceInt32 deltaGlyphID;
		} single1;
		struct {			/* single substitution, format 2 */
			PvfGsubCoverage coverage;
			SceInt32 glyphCount;
			SceInt32 *substitutes;
		} single2;
		struct {			/* multiple, alternate, ligature, context 1, chain 1 */
			PvfGsubCoverage coverage;
			SceInt32 setCount;
			PvfGsubSet *sets;
		} sets;
		struct {			/* context substitution, format 2 */
			PvfGsubCoverage coverage;
			SceInt32 classDef;	/* its offset, not read */
			SceInt32 setCount;
			PvfGsubSet *sets;
		} context2;
		struct {			/* context substitution, format 3 */
			SceInt32 glyphCount;
			PvfGsubCoverage *coverages;
			SceInt32 substCount;
			PvfGsubLookupRecord *substLookupRecords;
		} context3;
		struct {			/* chaining context substitution, format 2 */
			PvfGsubCoverage coverage;
			SceInt32 backtrackClassDef;	/* the offsets, not read */
			SceInt32 inputClassDef;
			SceInt32 lookaheadClassDef;
			SceInt32 setCount;
			PvfGsubSet *sets;
		} chain2;
		struct {			/* chaining context substitution, format 3 */
			SceInt32 backtrackCount;
			PvfGsubCoverage *backtrack;
			SceInt32 inputCount;
			PvfGsubCoverage *input;
			SceInt32 lookaheadCount;
			PvfGsubCoverage *lookahead;
			SceInt32 substCount;
			PvfGsubLookupRecord *substLookupRecords;
		} chain3;
		struct {			/* extension substitution, not followed */
			SceInt32 extensionLookupType;
			SceUInt32 extensionOffset;
		} extension;
		struct {			/* reverse chaining single substitution */
			PvfGsubCoverage coverage;
			SceInt32 backtrackCount;
			PvfGsubCoverage *backtrack;
			SceInt32 lookaheadCount;
			PvfGsubCoverage *lookahead;
			SceInt32 glyphCount;
			SceInt32 *substitutes;
		} reverse;
	} u;
} PvfGsubSubTable;

typedef struct PvfGsubLookup {
	SceInt32 lookupType;
	SceInt32 lookupFlag;
	SceInt32 subTableCount;
	PvfGsubSubTable **subTables;
	SceInt32 markFilteringSet;
} PvfGsubLookup;

typedef struct PvfGsub {
	void *(*alloc)(void *user, ScePvf_t_u32 size);
	void (*free)(void *user, void *ptr);
	void *user;
	SceUInt32 version;
	SceInt32 scriptCount;
	PvfGsubScript *scripts;
	SceInt32 featureCount;
	PvfGsubFeature *features;
	SceInt32 lookupCount;
	PvfGsubLookup **lookups;
	void *reserved;			/* NULL */
} PvfGsub;

_Static_assert(sizeof(PvfGsubSubTable) == 44, "PvfGsubSubTable size");
_Static_assert(sizeof(PvfGsub) == 44, "PvfGsub size");
_Static_assert(sizeof(PvfGsubScript) == 28, "PvfGsubScript size");
_Static_assert(sizeof(PvfGsubChainRule) == 32, "PvfGsubChainRule size");

#endif
