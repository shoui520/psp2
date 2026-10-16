/* SceNgs patch routing. Internal Sony helper names are unavailable. */

int ngsGetPatchIndex(int nPatchesPerOutput, int nSourceOutputIndex,
		     int nSourceOutputSubIndex)
{
	return nSourceOutputSubIndex + nPatchesPerOutput * nSourceOutputIndex;
}
