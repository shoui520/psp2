/*
 * SceLibPvf's GSUB reader: a face's glyph substitution table copied into the library's memory,
 * and the one query libpvf makes of it, a glyph's vertical form (the 'vert' and 'vrt2'
 * features' single substitutions).
 *
 * Every reader takes the object it fills, or NULL: without one it walks the table and keeps
 * nothing (so a failed allocation leaves the rest of the table read and unkept).
 *
 * No Sony record names this file or its functions; the names are ours, OpenType's where it
 * names the thing (agent/namecheck).
 */

#include <scetypes.h>
#include <font/libpvf.h>
#include "pvf_gsub.h"

/* the features whose single substitutions give a glyph's vertical form, in that order */
#define PVF_GSUB_TAG_VERT	0x76657274	/* 'vert' */
#define PVF_GSUB_TAG_VRT2	0x76727432	/* 'vrt2' */

#define PVF_GSUB_LOOKUP_SINGLE		1
#define PVF_GSUB_LOOKUP_MULTIPLE	2
#define PVF_GSUB_LOOKUP_ALTERNATE	3
#define PVF_GSUB_LOOKUP_LIGATURE	4
#define PVF_GSUB_LOOKUP_CONTEXT		5
#define PVF_GSUB_LOOKUP_CHAINING	6
#define PVF_GSUB_LOOKUP_EXTENSION	7
#define PVF_GSUB_LOOKUP_REVERSE		8

static const SceUInt32 pvfGsubVertFeatures[] = { PVF_GSUB_TAG_VERT, PVF_GSUB_TAG_VRT2 };

/* the table's big-endian fields */
static inline SceInt32 pvfGsubU16(const SceUInt8 *p)
{
	return p[0] << 8 | p[1];
}

static inline SceUInt32 pvfGsubU32(const SceUInt8 *p)
{
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

static inline void *pvfGsubAlloc(PvfGsub *gsub, SceInt32 size)
{
	return gsub->alloc(gsub->user, size);
}

/* count 16-bit values at data, into *values (a new array, NULL for none) when values is given */
static inline void pvfGsubLoadValues(PvfGsub *gsub, SceInt32 **values, SceInt32 count,
	const SceUInt8 *data)
{
	SceInt32 *array;
	SceInt32 i;

	array = NULL;
	if (values != NULL) {
		if (count == 0) {
			*values = NULL;
			return;
		}
		array = pvfGsubAlloc(gsub, count * sizeof(*array));
		*values = array;
	}
	if (count != 0 && array != NULL) {
		for (i = 0; i < count; i++)
			array[i] = pvfGsubU16(data + 2 * i);
	}
}

/* an input sequence: count - 1 values, none for a count of 0 or 1 */
static inline void pvfGsubLoadInput(PvfGsub *gsub, SceInt32 **values, SceInt32 count,
	const SceUInt8 *data)
{
	SceInt32 *array;
	SceInt32 i;

	array = NULL;
	if (values != NULL) {
		if (count > 1)
			array = pvfGsubAlloc(gsub, (count - 1) * sizeof(*array));
		*values = array;
	}
	if (count - 1 > 0 && array != NULL) {
		for (i = 0; i < count - 1; i++)
			array[i] = pvfGsubU16(data + 2 * i);
	}
}

/* an array of count SubstLookupRecords, into *records when given */
static inline PvfGsubLookupRecord *pvfGsubAllocLookupRecords(PvfGsub *gsub,
	PvfGsubLookupRecord **records, SceInt32 count)
{
	PvfGsubLookupRecord *array;

	array = NULL;
	if (records != NULL) {
		if (count != 0)
			array = pvfGsubAlloc(gsub, count * sizeof(*array));
		*records = array;
	}
	return array;
}

/* count SubstLookupRecords at data, into array (or nowhere) */
static inline void pvfGsubCopyLookupRecords(PvfGsubLookupRecord *array, SceInt32 count,
	const SceUInt8 *data)
{
	PvfGsubLookupRecord *record;
	SceInt32 i;

	if (count == 0 || array == NULL)
		return;
	for (i = 0; i < count; i++) {
		record = &array[i];
		if (record != NULL) {
			record->sequenceIndex = pvfGsubU16(data + 4 * i);
			record->lookupListIndex = pvfGsubU16(data + 4 * i + 2);
		}
	}
}

static inline void pvfGsubLoadLookupRecords(PvfGsub *gsub, PvfGsubLookupRecord **records,
	SceInt32 count, const SceUInt8 *data)
{
	pvfGsubCopyLookupRecords(pvfGsubAllocLookupRecords(gsub, records, count), count, data);
}

/*
 * SCE: the sorted flag is stored whether or not a coverage was given: walking a table into
 * nothing writes it at address 4.
 */
static __attribute__((noinline, noclone)) void pvfGsubLoadCoverage(PvfGsub *gsub, PvfGsubCoverage *coverage,
	const SceUInt8 *data)
{
	PvfGsubRangeRecord *ranges;
	SceInt32 *glyphs;
	SceInt32 format, count, sorted, glyph, start, end, prev, i;

	format = pvfGsubU16(data);
	if (coverage != NULL) {
		coverage->format = format;
		coverage->sorted = 0;
	}
	if (format == 1) {
		count = pvfGsubU16(data + 2);
		glyphs = NULL;
		if (coverage != NULL) {
			coverage->count = count;
			if (count != 0)
				glyphs = pvfGsubAlloc(gsub, count * sizeof(*glyphs));
			coverage->table = glyphs;
		}
		sorted = 1;
		prev = -1;
		for (i = 0; i < count; i++) {
			glyph = pvfGsubU16(data + 4 + 2 * i);
			if (glyphs != NULL)
				glyphs[i] = glyph;
			if (glyph < prev)
				sorted = 0;
			prev = glyph;
		}
		coverage->sorted = sorted;
	} else if (format == 2) {
		count = pvfGsubU16(data + 2);
		ranges = NULL;
		if (coverage != NULL) {
			coverage->count = count;
			if (count != 0)
				ranges = pvfGsubAlloc(gsub, count * sizeof(*ranges));
			coverage->table = ranges;
		}
		sorted = 1;
		prev = -1;
		for (i = 0; i < count; i++) {
			start = pvfGsubU16(data + 4 + 6 * i);
			end = pvfGsubU16(data + 4 + 6 * i + 2);
			if (ranges != NULL) {
				ranges[i].start = start;
				ranges[i].end = end;
				ranges[i].startCoverageIndex = pvfGsubU16(data + 4 + 6 * i + 4);
				ranges[i].reserved = 0;
			}
			if (end < start)
				sorted = 0;
			if (prev >= end)
				sorted = 0;
			prev = start;
		}
		coverage->sorted = sorted;
	}
}

/* an array of count Coverage tables, into *coverages when given */
static inline PvfGsubCoverage *pvfGsubAllocCoverages(PvfGsub *gsub, PvfGsubCoverage **coverages,
	SceInt32 count)
{
	PvfGsubCoverage *array;

	array = NULL;
	if (coverages != NULL) {
		if (count != 0)
			array = pvfGsubAlloc(gsub, count * sizeof(*array));
		*coverages = array;
	}
	return array;
}

/* count Coverage tables, at offsets from base read at data, into array (or nowhere) */
static inline void pvfGsubLoadCoverageArray(PvfGsub *gsub, PvfGsubCoverage *array,
	SceInt32 count, const SceUInt8 *base, const SceUInt8 *data)
{
	SceInt32 i;

	for (i = 0; i < count; i++)
		pvfGsubLoadCoverage(gsub, array != NULL ? &array[i] : NULL,
			base + pvfGsubU16(data + 2 * i));
}

static inline void pvfGsubLoadCoverages(PvfGsub *gsub, PvfGsubCoverage **coverages,
	SceInt32 count, const SceUInt8 *base, const SceUInt8 *data)
{
	pvfGsubLoadCoverageArray(gsub, pvfGsubAllocCoverages(gsub, coverages, count), count, base,
		data);
}

static inline void pvfGsubLoadLangSys(PvfGsub *gsub, PvfGsubLangSys *langSys,
	const SceUInt8 *data)
{
	SceInt32 featureCount;

	featureCount = pvfGsubU16(data + 4);
	if (langSys != NULL) {
		langSys->lookupOrder = pvfGsubU16(data);
		langSys->reqFeatureIndex = pvfGsubU16(data + 2);
		langSys->featureCount = featureCount;
	}
	pvfGsubLoadValues(gsub, langSys != NULL ? &langSys->featureIndices : NULL, featureCount,
		data + 6);
}

static __attribute__((noinline, noclone)) void pvfGsubLoadScriptList(PvfGsub *gsub,
	const SceUInt8 *data)
{
	PvfGsubLangSysRecord *records, *record;
	PvfGsubScript *scripts, *script;
	const SceUInt8 *scriptTable, *langSysRecord;
	SceInt32 count, langSysCount, i, j;

	count = pvfGsubU16(data);
	scripts = NULL;
	if (gsub != NULL) {
		gsub->scriptCount = count;
		if (count == 0) {
			gsub->scripts = NULL;
			return;
		}
		scripts = pvfGsubAlloc(gsub, count * sizeof(*scripts));
		gsub->scripts = scripts;
	}
	for (i = 0; i < count; i++) {
		script = scripts != NULL ? &scripts[i] : NULL;
		if (script != NULL)
			script->tag = pvfGsubU32(data + 2 + 6 * i);
		scriptTable = data + pvfGsubU16(data + 2 + 6 * i + 4);
		langSysCount = pvfGsubU16(scriptTable + 2);
		records = NULL;
		if (script != NULL) {
			script->langSysCount = langSysCount;
			if (langSysCount != 0)
				records = pvfGsubAlloc(gsub, langSysCount * sizeof(*records));
			script->langSys = records;
		}
		pvfGsubLoadLangSys(gsub, script != NULL ? &script->defaultLangSys : NULL,
			scriptTable + pvfGsubU16(scriptTable));
		/* SCE: a LangSysRecord read as a 16-bit tag and its offset, 4 bytes apart */
		langSysRecord = scriptTable + 4;
		for (j = 0; j < langSysCount; j++) {
			record = records != NULL ? &records[j] : NULL;
			if (record != NULL)
				record->tag = pvfGsubU16(langSysRecord);
			pvfGsubLoadLangSys(gsub, record != NULL ? &record->langSys : NULL,
				scriptTable + pvfGsubU16(langSysRecord + 2));
			langSysRecord += 4;
		}
	}
}

static __attribute__((noinline, noclone)) void pvfGsubLoadFeatureList(PvfGsub *gsub,
	const SceUInt8 *data)
{
	PvfGsubFeature *features, *feature;
	const SceUInt8 *featureTable;
	SceInt32 count, lookupCount, i, j;

	count = pvfGsubU16(data);
	features = NULL;
	if (gsub != NULL) {
		gsub->featureCount = count;
		if (count == 0) {
			gsub->features = NULL;
			return;
		}
		features = pvfGsubAlloc(gsub, count * sizeof(*features));
		gsub->features = features;
	}
	for (i = 0; i < count; i++) {
		feature = features != NULL ? &features[i] : NULL;
		if (feature != NULL)
			feature->tag = pvfGsubU32(data + 2 + 6 * i);
		featureTable = data + pvfGsubU16(data + 2 + 6 * i + 4);
		lookupCount = pvfGsubU16(featureTable + 2);
		if (feature == NULL)
			continue;
		feature->featureParams = pvfGsubU16(featureTable);
		feature->lookupCount = lookupCount;
		if (lookupCount == 0) {
			feature->lookupListIndices = NULL;
			continue;
		}
		feature->lookupListIndices = pvfGsubAlloc(gsub,
			lookupCount * sizeof(*feature->lookupListIndices));
		/* SCE: copied without checking the allocation */
		for (j = 0; j < lookupCount; j++)
			feature->lookupListIndices[j] = pvfGsubU16(featureTable + 4 + 2 * j);
	}
}

/* count Sequences or AlternateSets: arrays of glyphs */
static inline void pvfGsubLoadGlyphSets(PvfGsub *gsub, PvfGsubSet *sets, SceInt32 count,
	const SceUInt8 *base, const SceUInt8 *data)
{
	const SceUInt8 *set;
	SceInt32 i;

	for (i = 0; i < count; i++) {
		set = base + pvfGsubU16(data + 2 * i);
		if (sets != NULL && &sets[i] != NULL) {
			sets[i].count = pvfGsubU16(set);
			pvfGsubLoadValues(gsub, (SceInt32 **)&sets[i].items, sets[i].count, set + 2);
		}
	}
}

static inline void pvfGsubLoadLigatureSets(PvfGsub *gsub, PvfGsubSet *sets, SceInt32 count,
	const SceUInt8 *base, const SceUInt8 *data)
{
	PvfGsubLigature *ligatures, *ligature;
	const SceUInt8 *setTable, *table;
	PvfGsubSet *set;
	SceInt32 ligatureCount, compCount, i, j;

	for (i = 0; i < count; i++) {
		setTable = base + pvfGsubU16(data + 2 * i);
		ligatureCount = pvfGsubU16(setTable);
		ligatures = NULL;
		set = sets != NULL ? &sets[i] : NULL;
		if (set != NULL) {
			set->count = ligatureCount;
			if (ligatureCount != 0)
				ligatures = pvfGsubAlloc(gsub, ligatureCount * sizeof(*ligatures));
			set->items = ligatures;
		}
		for (j = 0; j < ligatureCount; j++) {
			table = setTable + pvfGsubU16(setTable + 2 + 2 * j);
			ligature = ligatures != NULL ? &ligatures[j] : NULL;
			compCount = pvfGsubU16(table + 2);
			if (ligature != NULL) {
				ligature->ligGlyph = pvfGsubU16(table);
				ligature->compCount = compCount;
				pvfGsubLoadInput(gsub, &ligature->components, compCount, table + 4);
			}
		}
	}
}

/* count SubRuleSets or SubClassSets */
static inline void pvfGsubLoadRuleSets(PvfGsub *gsub, PvfGsubSet *sets, SceInt32 count,
	const SceUInt8 *base, const SceUInt8 *data)
{
	PvfGsubRule *rules, *rule;
	const SceUInt8 *setTable, *table;
	PvfGsubSet *set;
	SceInt32 ruleCount, glyphCount, substCount, i, j;

	for (i = 0; i < count; i++) {
		setTable = base + pvfGsubU16(data + 2 * i);
		ruleCount = pvfGsubU16(setTable);
		rules = NULL;
		set = sets != NULL ? &sets[i] : NULL;
		if (set != NULL) {
			set->count = ruleCount;
			if (ruleCount != 0)
				rules = pvfGsubAlloc(gsub, ruleCount * sizeof(*rules));
			set->items = rules;
		}
		for (j = 0; j < ruleCount; j++) {
			table = setTable + pvfGsubU16(setTable + 2 + 2 * j);
			rule = rules != NULL ? &rules[j] : NULL;
			glyphCount = pvfGsubU16(table);
			substCount = pvfGsubU16(table + 2);
			if (rule != NULL) {
				rule->glyphCount = glyphCount;
				rule->substCount = substCount;
			}
			pvfGsubLoadInput(gsub, rule != NULL ? &rule->input : NULL, glyphCount,
				table + 4);
			pvfGsubLoadLookupRecords(gsub, rule != NULL ? &rule->substLookupRecords : NULL,
				substCount, table + 4 + (glyphCount > 1 ? 2 * (glyphCount - 1) : 0));
		}
	}
}

/* count ChainSubRuleSets or ChainSubClassSets */
static inline void pvfGsubLoadChainRuleSets(PvfGsub *gsub, PvfGsubSet *sets, SceInt32 count,
	const SceUInt8 *base, const SceUInt8 *data)
{
	PvfGsubChainRule *rules, *rule;
	const SceUInt8 *setTable, *p;
	PvfGsubSet *set;
	SceInt32 ruleCount, n, i, j;

	for (i = 0; i < count; i++) {
		setTable = base + pvfGsubU16(data + 2 * i);
		ruleCount = pvfGsubU16(setTable);
		rules = NULL;
		set = sets != NULL ? &sets[i] : NULL;
		if (set != NULL) {
			set->count = ruleCount;
			if (ruleCount != 0)
				rules = pvfGsubAlloc(gsub, ruleCount * sizeof(*rules));
			set->items = rules;
		}
		for (j = 0; j < ruleCount; j++) {
			p = setTable + pvfGsubU16(setTable + 2 + 2 * j);
			rule = rules != NULL ? &rules[j] : NULL;
			n = pvfGsubU16(p);
			if (rule != NULL)
				rule->backtrackCount = n;
			pvfGsubLoadValues(gsub, rule != NULL ? &rule->backtrack : NULL, n, p + 2);
			p += 2 + 2 * n;
			n = pvfGsubU16(p);
			if (rule != NULL)
				rule->inputCount = n;
			pvfGsubLoadInput(gsub, rule != NULL ? &rule->input : NULL, n, p + 2);
			p += 2 + (n > 1 ? 2 * (n - 1) : 0);
			n = pvfGsubU16(p);
			if (rule != NULL)
				rule->lookaheadCount = n;
			pvfGsubLoadValues(gsub, rule != NULL ? &rule->lookahead : NULL, n, p + 2);
			p += 2 + 2 * n;
			n = pvfGsubU16(p);
			if (rule != NULL)
				rule->substCount = n;
			pvfGsubLoadLookupRecords(gsub, rule != NULL ? &rule->substLookupRecords : NULL,
				n, p + 2);
		}
	}
}

/* a subtable with a coverage and count sets of a kind */
static inline PvfGsubSet *pvfGsubLoadSets(PvfGsub *gsub, PvfGsubCoverage *coverage,
	SceInt32 *setCount, PvfGsubSet **sets, SceInt32 count, const SceUInt8 *coverageTable)
{
	PvfGsubSet *array;

	array = NULL;
	if (sets != NULL) {
		*setCount = count;
		if (count != 0)
			array = pvfGsubAlloc(gsub, count * sizeof(*array));
		*sets = array;
	}
	pvfGsubLoadCoverage(gsub, coverage, coverageTable);
	return array;
}

static __attribute__((noinline, noclone)) void pvfGsubLoadLookup(PvfGsub *gsub, PvfGsubLookup *lookup,
	const SceUInt8 *data)
{
	PvfGsubSubTable **subTables, *sub;
	PvfGsubLookupRecord *records;
	PvfGsubCoverage *coverages;
	const SceUInt8 *table, *p;
	PvfGsubSet *sets;
	SceInt32 lookupType, subTableCount, format, count, substCount, i;

	lookupType = pvfGsubU16(data);
	subTableCount = pvfGsubU16(data + 4);
	subTables = NULL;
	if (lookup != NULL) {
		lookup->lookupType = lookupType;
		lookup->lookupFlag = pvfGsubU16(data + 2);
		lookup->subTableCount = subTableCount;
		if (subTableCount == 0) {
			lookup->subTables = NULL;
			lookup->markFilteringSet = pvfGsubU16(data + 6);
			return;
		}
		subTables = pvfGsubAlloc(gsub, subTableCount * sizeof(*subTables));
		lookup->subTables = subTables;
	}
	for (i = 0; i < subTableCount; i++) {
		table = data + pvfGsubU16(data + 6 + 2 * i);
		sub = NULL;
		if (subTables != NULL) {
			sub = pvfGsubAlloc(gsub, sizeof(*sub));
			subTables[i] = sub;
		}
		format = pvfGsubU16(table);
		if (sub != NULL)
			sub->format = format;
		switch (lookupType) {
		case PVF_GSUB_LOOKUP_SINGLE:
			if (format == 1) {
				if (sub != NULL)
					sub->u.single1.deltaGlyphID = pvfGsubU16(table + 4);
				pvfGsubLoadCoverage(gsub, sub != NULL ? &sub->u.single1.coverage : NULL,
					table + pvfGsubU16(table + 2));
			} else if (format == 2) {
				count = pvfGsubU16(table + 4);
				if (sub != NULL)
					sub->u.single2.glyphCount = count;
				pvfGsubLoadValues(gsub, sub != NULL ? &sub->u.single2.substitutes : NULL,
					count, table + 6);
				pvfGsubLoadCoverage(gsub, sub != NULL ? &sub->u.single2.coverage : NULL,
					table + pvfGsubU16(table + 2));
			}
			break;
		case PVF_GSUB_LOOKUP_MULTIPLE:
		case PVF_GSUB_LOOKUP_ALTERNATE:
			count = pvfGsubU16(table + 4);
			sets = pvfGsubLoadSets(gsub, sub != NULL ? &sub->u.sets.coverage : NULL,
				sub != NULL ? &sub->u.sets.setCount : NULL,
				sub != NULL ? &sub->u.sets.sets : NULL, count,
				table + pvfGsubU16(table + 2));
			if (sets != NULL)
				pvfGsubLoadGlyphSets(gsub, sets, count, table, table + 6);
			break;
		case PVF_GSUB_LOOKUP_LIGATURE:
			count = pvfGsubU16(table + 4);
			sets = pvfGsubLoadSets(gsub, sub != NULL ? &sub->u.sets.coverage : NULL,
				sub != NULL ? &sub->u.sets.setCount : NULL,
				sub != NULL ? &sub->u.sets.sets : NULL, count,
				table + pvfGsubU16(table + 2));
			pvfGsubLoadLigatureSets(gsub, sets, count, table, table + 6);
			break;
		case PVF_GSUB_LOOKUP_CONTEXT:
			if (format == 1) {
				count = pvfGsubU16(table + 4);
				sets = pvfGsubLoadSets(gsub,
					sub != NULL ? &sub->u.sets.coverage : NULL,
					sub != NULL ? &sub->u.sets.setCount : NULL,
					sub != NULL ? &sub->u.sets.sets : NULL, count,
					table + pvfGsubU16(table + 2));
				pvfGsubLoadRuleSets(gsub, sets, count, table, table + 6);
			} else if (format == 2) {
				count = pvfGsubU16(table + 6);
				if (sub != NULL)
					sub->u.context2.classDef = pvfGsubU16(table + 4);
				sets = pvfGsubLoadSets(gsub,
					sub != NULL ? &sub->u.context2.coverage : NULL,
					sub != NULL ? &sub->u.context2.setCount : NULL,
					sub != NULL ? &sub->u.context2.sets : NULL, count,
					table + pvfGsubU16(table + 2));
				pvfGsubLoadRuleSets(gsub, sets, count, table, table + 8);
			} else if (format == 3) {
				/* both arrays first, then the coverages read into the first */
				count = pvfGsubU16(table + 2);
				substCount = pvfGsubU16(table + 4);
				if (sub != NULL) {
					sub->u.context3.glyphCount = count;
					sub->u.context3.substCount = substCount;
				}
				coverages = pvfGsubAllocCoverages(gsub,
					sub != NULL ? &sub->u.context3.coverages : NULL, count);
				records = pvfGsubAllocLookupRecords(gsub,
					sub != NULL ? &sub->u.context3.substLookupRecords : NULL,
					substCount);
				pvfGsubLoadCoverageArray(gsub, coverages, count, table, table + 6);
				pvfGsubCopyLookupRecords(records, substCount, table + 6 + 2 * count);
			}
			break;
		case PVF_GSUB_LOOKUP_CHAINING:
			if (format == 1) {
				count = pvfGsubU16(table + 4);
				sets = pvfGsubLoadSets(gsub,
					sub != NULL ? &sub->u.sets.coverage : NULL,
					sub != NULL ? &sub->u.sets.setCount : NULL,
					sub != NULL ? &sub->u.sets.sets : NULL, count,
					table + pvfGsubU16(table + 2));
				pvfGsubLoadChainRuleSets(gsub, sets, count, table, table + 6);
			} else if (format == 2) {
				count = pvfGsubU16(table + 10);
				if (sub != NULL) {
					sub->u.chain2.backtrackClassDef = pvfGsubU16(table + 4);
					sub->u.chain2.inputClassDef = pvfGsubU16(table + 6);
					sub->u.chain2.lookaheadClassDef = pvfGsubU16(table + 8);
				}
				sets = pvfGsubLoadSets(gsub,
					sub != NULL ? &sub->u.chain2.coverage : NULL,
					sub != NULL ? &sub->u.chain2.setCount : NULL,
					sub != NULL ? &sub->u.chain2.sets : NULL, count,
					table + pvfGsubU16(table + 2));
				pvfGsubLoadChainRuleSets(gsub, sets, count, table, table + 12);
			} else if (format == 3) {
				p = table + 2;
				count = pvfGsubU16(p);
				if (sub != NULL)
					sub->u.chain3.backtrackCount = count;
				pvfGsubLoadCoverages(gsub, sub != NULL ? &sub->u.chain3.backtrack : NULL,
					count, table, p + 2);
				p += 2 + 2 * count;
				count = pvfGsubU16(p);
				if (sub != NULL)
					sub->u.chain3.inputCount = count;
				pvfGsubLoadCoverages(gsub, sub != NULL ? &sub->u.chain3.input : NULL,
					count, table, p + 2);
				p += 2 + 2 * count;
				count = pvfGsubU16(p);
				if (sub != NULL)
					sub->u.chain3.lookaheadCount = count;
				pvfGsubLoadCoverages(gsub, sub != NULL ? &sub->u.chain3.lookahead : NULL,
					count, table, p + 2);
				p += 2 + 2 * count;
				count = pvfGsubU16(p);
				if (sub != NULL)
					sub->u.chain3.substCount = count;
				pvfGsubLoadLookupRecords(gsub,
					sub != NULL ? &sub->u.chain3.substLookupRecords : NULL, count,
					p + 2);
			}
			break;
		case PVF_GSUB_LOOKUP_EXTENSION:
			if (sub != NULL) {
				sub->u.extension.extensionLookupType = pvfGsubU16(table + 2);
				sub->u.extension.extensionOffset = pvfGsubU32(table + 4);
			}
			break;
		case PVF_GSUB_LOOKUP_REVERSE:
			p = table + 4;
			count = pvfGsubU16(p);
			if (sub != NULL)
				sub->u.reverse.backtrackCount = count;
			/* the backtrack array, the coverage, then the backtrack coverages */
			coverages = pvfGsubAllocCoverages(gsub,
				sub != NULL ? &sub->u.reverse.backtrack : NULL, count);
			pvfGsubLoadCoverage(gsub, sub != NULL ? &sub->u.reverse.coverage : NULL,
				table + pvfGsubU16(table + 2));
			pvfGsubLoadCoverageArray(gsub, coverages, count, table, p + 2);
			p += 2 + 2 * count;
			count = pvfGsubU16(p);
			if (sub != NULL)
				sub->u.reverse.lookaheadCount = count;
			pvfGsubLoadCoverages(gsub, sub != NULL ? &sub->u.reverse.lookahead : NULL,
				count, table, p + 2);
			p += 2 + 2 * count;
			count = pvfGsubU16(p);
			if (sub != NULL)
				sub->u.reverse.glyphCount = count;
			pvfGsubLoadValues(gsub, sub != NULL ? &sub->u.reverse.substitutes : NULL,
				count, p + 2);
			break;
		default:
			break;
		}
	}
	if (lookup != NULL)
		lookup->markFilteringSet = pvfGsubU16(data + 6 + 2 * subTableCount);
}

PvfGsub *PvfGsubNew(const PvfGsubMemory *memory, FT_Bytes table)
{
	const SceUInt8 *scriptList, *featureList, *lookupList;
	PvfGsubLookup **lookups;
	PvfGsub *gsub;
	SceInt32 count, i;

	if (table == NULL)
		return NULL;
	if (memory == NULL || memory->alloc == NULL || memory->free == NULL)
		return NULL;
	gsub = memory->alloc(memory->user, sizeof(*gsub));
	if (gsub == NULL)
		return NULL;
	gsub->reserved = NULL;
	gsub->alloc = memory->alloc;
	gsub->free = memory->free;
	gsub->user = memory->user;
	scriptList = table + pvfGsubU16(table + 4);
	featureList = table + pvfGsubU16(table + 6);
	lookupList = table + pvfGsubU16(table + 8);
	gsub->version = pvfGsubU32(table);
	pvfGsubLoadScriptList(gsub, scriptList);
	pvfGsubLoadFeatureList(gsub, featureList);
	count = pvfGsubU16(lookupList);
	gsub->lookupCount = count;
	if (count == 0) {
		gsub->lookups = NULL;
		return gsub;
	}
	lookups = pvfGsubAlloc(gsub, count * sizeof(*lookups));
	gsub->lookups = lookups;
	for (i = 0; i < count; i++) {
		if (lookups != NULL) {
			lookups[i] = pvfGsubAlloc(gsub, sizeof(**lookups));
			pvfGsubLoadLookup(gsub, lookups[i], lookupList + pvfGsubU16(lookupList + 2 + 2 * i));
		} else {
			pvfGsubLoadLookup(gsub, NULL, lookupList + pvfGsubU16(lookupList + 2 + 2 * i));
		}
	}
	return gsub;
}

static inline void pvfGsubFreeCoverage(PvfGsub *gsub, PvfGsubCoverage *coverage)
{
	if ((coverage->format == 1 || coverage->format == 2) && coverage->table != NULL)
		gsub->free(gsub->user, coverage->table);
}

static inline void pvfGsubFreeCoverages(PvfGsub *gsub, PvfGsubCoverage *coverages,
	SceInt32 count)
{
	SceInt32 i;

	if (coverages == NULL)
		return;
	for (i = 0; i < count; i++)
		pvfGsubFreeCoverage(gsub, &coverages[i]);
	gsub->free(gsub->user, coverages);
}

static inline void pvfGsubFreeRuleSets(PvfGsub *gsub, PvfGsubSet *sets, SceInt32 count)
{
	PvfGsubRule *rules;
	SceInt32 i, j;

	for (i = 0; i < count; i++) {
		rules = sets[i].items;
		if (rules == NULL)
			continue;
		for (j = 0; j < sets[i].count; j++) {
			if (rules[j].input != NULL)
				gsub->free(gsub->user, rules[j].input);
			if (rules[j].substLookupRecords != NULL)
				gsub->free(gsub->user, rules[j].substLookupRecords);
		}
		gsub->free(gsub->user, sets[i].items);
	}
}

static inline void pvfGsubFreeChainRuleSets(PvfGsub *gsub, PvfGsubSet *sets, SceInt32 count)
{
	PvfGsubChainRule *rules;
	SceInt32 i, j;

	for (i = 0; i < count; i++) {
		rules = sets[i].items;
		if (rules == NULL)
			continue;
		for (j = 0; j < sets[i].count; j++) {
			if (rules[j].backtrack != NULL)
				gsub->free(gsub->user, rules[j].backtrack);
			if (rules[j].input != NULL)
				gsub->free(gsub->user, rules[j].input);
			if (rules[j].lookahead != NULL)
				gsub->free(gsub->user, rules[j].lookahead);
			if (rules[j].substLookupRecords != NULL)
				gsub->free(gsub->user, rules[j].substLookupRecords);
		}
		gsub->free(gsub->user, sets[i].items);
	}
}

static inline void pvfGsubFreeSubTable(PvfGsub *gsub, SceInt32 lookupType, PvfGsubSubTable *sub)
{
	PvfGsubLigature *ligatures;
	PvfGsubSet *sets;
	void *last;
	SceInt32 i, j;

	last = NULL;
	switch (lookupType) {
	case PVF_GSUB_LOOKUP_SINGLE:
		if (sub->format == 1) {
			pvfGsubFreeCoverage(gsub, &sub->u.single1.coverage);
		} else if (sub->format == 2) {
			pvfGsubFreeCoverage(gsub, &sub->u.single2.coverage);
			last = sub->u.single2.substitutes;
		}
		break;
	case PVF_GSUB_LOOKUP_MULTIPLE:
	case PVF_GSUB_LOOKUP_ALTERNATE:
		pvfGsubFreeCoverage(gsub, &sub->u.sets.coverage);
		sets = sub->u.sets.sets;
		if (sets != NULL) {
			for (i = 0; i < sub->u.sets.setCount; i++) {
				if (sets[i].items != NULL)
					gsub->free(gsub->user, sets[i].items);
			}
		}
		last = sets;
		break;
	case PVF_GSUB_LOOKUP_LIGATURE:
		pvfGsubFreeCoverage(gsub, &sub->u.sets.coverage);
		sets = sub->u.sets.sets;
		if (sets != NULL) {
			for (i = 0; i < sub->u.sets.setCount; i++) {
				ligatures = sets[i].items;
				if (ligatures == NULL)
					continue;
				for (j = 0; j < sets[i].count; j++) {
					if (ligatures[j].components != NULL)
						gsub->free(gsub->user, ligatures[j].components);
				}
				gsub->free(gsub->user, sets[i].items);
			}
		}
		last = sets;
		break;
	case PVF_GSUB_LOOKUP_CONTEXT:
		if (sub->format == 1) {
			pvfGsubFreeCoverage(gsub, &sub->u.sets.coverage);
			if (sub->u.sets.sets != NULL)
				pvfGsubFreeRuleSets(gsub, sub->u.sets.sets, sub->u.sets.setCount);
			last = sub->u.sets.sets;
		} else if (sub->format == 2) {
			pvfGsubFreeCoverage(gsub, &sub->u.context2.coverage);
			if (sub->u.context2.sets != NULL)
				pvfGsubFreeRuleSets(gsub, sub->u.context2.sets,
					sub->u.context2.setCount);
			last = sub->u.context2.sets;
		} else if (sub->format == 3) {
			pvfGsubFreeCoverages(gsub, sub->u.context3.coverages,
				sub->u.context3.glyphCount);
			last = sub->u.context3.substLookupRecords;
		}
		break;
	case PVF_GSUB_LOOKUP_CHAINING:
		if (sub->format == 1) {
			pvfGsubFreeCoverage(gsub, &sub->u.sets.coverage);
			if (sub->u.sets.sets != NULL)
				pvfGsubFreeChainRuleSets(gsub, sub->u.sets.sets,
					sub->u.sets.setCount);
			last = sub->u.sets.sets;
		} else if (sub->format == 2) {
			pvfGsubFreeCoverage(gsub, &sub->u.chain2.coverage);
			if (sub->u.chain2.sets != NULL)
				pvfGsubFreeChainRuleSets(gsub, sub->u.chain2.sets,
					sub->u.chain2.setCount);
			last = sub->u.chain2.sets;
		} else if (sub->format == 3) {
			pvfGsubFreeCoverages(gsub, sub->u.chain3.backtrack,
				sub->u.chain3.backtrackCount);
			pvfGsubFreeCoverages(gsub, sub->u.chain3.input, sub->u.chain3.inputCount);
			pvfGsubFreeCoverages(gsub, sub->u.chain3.lookahead,
				sub->u.chain3.lookaheadCount);
			last = sub->u.chain3.substLookupRecords;
		}
		break;
	case PVF_GSUB_LOOKUP_REVERSE:
		pvfGsubFreeCoverages(gsub, sub->u.reverse.backtrack, sub->u.reverse.backtrackCount);
		pvfGsubFreeCoverages(gsub, sub->u.reverse.lookahead, sub->u.reverse.lookaheadCount);
		last = sub->u.reverse.substitutes;
		break;
	default:
		break;
	}
	if (last != NULL)
		gsub->free(gsub->user, last);
}

void PvfGsubDelete(PvfGsub *gsub)
{
	PvfGsubLookup *lookup;
	PvfGsubScript *script;
	SceInt32 i, j;

	if (gsub == NULL)
		return;
	if (gsub->lookups != NULL) {
		for (i = 0; i < gsub->lookupCount; i++) {
			lookup = gsub->lookups[i];
			if (lookup == NULL)
				continue;
			if (lookup->subTables != NULL) {
				for (j = 0; j < lookup->subTableCount; j++) {
					if (lookup->subTables[j] == NULL)
						continue;
					pvfGsubFreeSubTable(gsub, lookup->lookupType,
						lookup->subTables[j]);
					gsub->free(gsub->user, lookup->subTables[j]);
				}
				gsub->free(gsub->user, lookup->subTables);
			}
			gsub->free(gsub->user, gsub->lookups[i]);
		}
		gsub->free(gsub->user, gsub->lookups);
	}
	for (i = 0; i < gsub->featureCount; i++) {
		if (gsub->features[i].lookupCount != 0 &&
		    gsub->features[i].lookupListIndices != NULL)
			gsub->free(gsub->user, gsub->features[i].lookupListIndices);
	}
	if (gsub->features != NULL)
		gsub->free(gsub->user, gsub->features);
	for (i = 0; i < gsub->scriptCount; i++) {
		script = &gsub->scripts[i];
		if (script->defaultLangSys.featureIndices != NULL)
			gsub->free(gsub->user, script->defaultLangSys.featureIndices);
		for (j = 0; j < script->langSysCount; j++) {
			if (script->langSys[j].langSys.featureIndices != NULL)
				gsub->free(gsub->user, script->langSys[j].langSys.featureIndices);
		}
		if (script->langSys != NULL)
			gsub->free(gsub->user, script->langSys);
	}
	if (gsub->scripts != NULL)
		gsub->free(gsub->user, gsub->scripts);
	gsub->free(gsub->user, gsub);
}

/* the index of glyph in a coverage, or -1 */
static inline SceInt32 pvfGsubCoverageIndex(const PvfGsubCoverage *coverage, SceInt32 glyph)
{
	const PvfGsubRangeRecord *ranges;
	const SceInt32 *glyphs;
	SceInt32 low, high, mid, diff, i;

	if (coverage->format == 1) {
		glyphs = coverage->table;
		if (glyphs == NULL)
			return -1;
		if (!coverage->sorted) {
			for (i = 0; i < coverage->count; i++) {
				if (glyphs[i] == glyph)
					return i;
			}
			return -1;
		}
		if (coverage->count == 0)
			return -1;
		low = 0;
		high = coverage->count - 1;
		while (low <= high) {
			mid = (low + high) >> 1;
			diff = glyph - glyphs[mid];
			if (diff == 0)
				return mid;
			if (diff >= 0)
				low = mid + 1;
			else
				high = mid - 1;
		}
		return -1;
	}
	if (coverage->format == 2) {
		ranges = coverage->table;
		if (ranges == NULL)
			return -1;
		if (!coverage->sorted) {
			for (i = 0; i < coverage->count; i++) {
				if (glyph >= ranges[i].start && glyph <= ranges[i].end)
					return ranges[i].startCoverageIndex + glyph - ranges[i].start;
			}
			return -1;
		}
		if (coverage->count == 0)
			return -1;
		low = 0;
		high = coverage->count - 1;
		while (low <= high) {
			mid = (low + high) >> 1;
			if (glyph < ranges[mid].start)
				high = mid - 1;
			else if (glyph <= ranges[mid].end)
				return ranges[mid].startCoverageIndex + glyph - ranges[mid].start;
			else
				low = mid + 1;
		}
	}
	return -1;
}

FT_Int PvfGsubGetVertGlyph(PvfGsub *gsub, FT_UInt glyph)
{
	const PvfGsubFeature *feature;
	const PvfGsubLookup *lookup;
	const PvfGsubSubTable *sub;
	SceInt32 index, lookupIndex, f, i, j, t;

	if (gsub == NULL || gsub->lookups == NULL || gsub->features == NULL)
		return -1;
	for (t = 0; t < 2; t++) {
		for (f = 0; f < gsub->featureCount; f++) {
			feature = &gsub->features[f];
			if (feature->tag != pvfGsubVertFeatures[t] ||
			    feature->lookupListIndices == NULL)
				continue;
			for (i = 0; i < feature->lookupCount; i++) {
				lookupIndex = feature->lookupListIndices[i];
				if (lookupIndex < 0 || lookupIndex >= gsub->lookupCount)
					continue;
				lookup = gsub->lookups[lookupIndex];
				if (lookup->lookupType != PVF_GSUB_LOOKUP_SINGLE)
					continue;
				for (j = 0; j < lookup->subTableCount; j++) {
					sub = lookup->subTables[j];
					if (sub == NULL)
						continue;
					if (sub->format == 1) {
						if (sub->u.single1.coverage.table == NULL)
							continue;
						if (pvfGsubCoverageIndex(&sub->u.single1.coverage,
						    glyph) >= 0)
							return sub->u.single1.deltaGlyphID + glyph;
					} else if (sub->format == 2) {
						if (sub->u.single2.coverage.table == NULL)
							continue;
						index = pvfGsubCoverageIndex(
							&sub->u.single2.coverage, glyph);
						/* SCE: one past the substitutes is taken too */
						if (index >= 0 && sub->u.single2.glyphCount >= index)
							return sub->u.single2.substitutes[index];
					}
				}
			}
		}
	}
	return -1;
}
