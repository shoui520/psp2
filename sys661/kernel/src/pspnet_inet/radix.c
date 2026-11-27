#include "pspnet_inet_internal.h"

#define RN_F_NORMAL 0x01
#define RN_F_ROOT   0x02
#define RN_F_ACTIVE 0x04

static const u8 g_normal_chars[9] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF };

static s32 g_last_zeroed;
static u8 *g_addmask_key;
static u8 *g_rn_zeros;
static u8 *g_rn_ones;
static InetRadixNodeHead *g_mask_rnhead;
static InetRadixMask *g_rn_mkfreelist;
static s32 g_max_keylen;

static InetRadixNode *inetRnSearch(const u8 *key, InetRadixNode *head)
{
    InetRadixNode *cur = head;
    while (cur->rn_bit >= 0) {
        if ((cur->rn_bmask & key[cur->rn_u.rn_node.rn_offset]) != 0) {
            cur = cur->rn_u.rn_node.rn_right;
        } else {
            cur = cur->rn_u.rn_node.rn_left;
        }
    }
    return cur;
}

static InetRadixNode *inetRnSearchM(const u8 *key, InetRadixNode *head, const u8 *mask)
{
    InetRadixNode *cur = head;
    while (cur->rn_bit >= 0) {
        if ((cur->rn_bmask & mask[cur->rn_u.rn_node.rn_offset]) != 0 &&
            (cur->rn_bmask & key[cur->rn_u.rn_node.rn_offset]) != 0) {
            cur = cur->rn_u.rn_node.rn_right;
        } else {
            cur = cur->rn_u.rn_node.rn_left;
        }
    }
    return cur;
}

static int inetRnSatisfiesLeaf(const u8 *trial, InetRadixNode *leaf, u32 skip)
{
    const u8 *key = leaf->rn_u.rn_leaf.rn_key;
    const u8 *mask = leaf->rn_u.rn_leaf.rn_mask;
    u32 len = trial[0];
    u32 pos;

    if (len > key[0]) {
        len = key[0];
    }
    if (mask != NULL) {
        if (len > mask[0]) {
            len = mask[0];
        }
    } else {
        mask = g_rn_ones;
    }
    for (pos = skip; pos < len; pos++) {
        if (((trial[pos] ^ key[pos]) & mask[pos]) != 0) {
            return 0;
        }
    }
    return 1;
}

static int inetRnLexobetter(const u8 *lhs, const u8 *rhs)
{
    u32 lhsLen = lhs[0];
    u32 rhsLen = rhs[0];
    u32 pos;
    if (lhsLen > rhsLen) {
        return 1;
    }
    if (lhsLen < rhsLen) {
        return 0;
    }
    for (pos = 0; pos < rhsLen; pos++) {
        if (lhs[pos] < rhs[pos]) {
            return 0;
        }
    }
    return 1;
}

static InetRadixMask *inetRnNewRadixMask(InetRadixNode *leaf, InetRadixMask *next)
{
    InetRadixMask *mk;
    if (g_rn_mkfreelist != NULL) {
        mk = g_rn_mkfreelist;
        g_rn_mkfreelist = mk->rm_mklist;
    } else {
        mk = (InetRadixMask *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetRadixMask));
    }
    if (mk == NULL) {
        return NULL;
    }
    sceNet_lib_8B615F52(mk, sizeof(InetRadixMask));
    mk->rm_bit = leaf->rn_bit;
    mk->rm_flags = leaf->rn_flags;
    if ((leaf->rn_flags & RN_F_NORMAL) != 0) {
        mk->rm_u.rmu_leaf = leaf;
    } else {
        mk->rm_u.rmu_mask = leaf->rn_u.rn_leaf.rn_mask;
    }
    mk->rm_mklist = next;
    leaf->rn_mklist = mk;
    return mk;
}

/* [166] 0x00B518 */
INET_NOINLINE InetRadixNode *inetRnMatch(void *vArg, InetRadixNodeHead *head)
{
    const u8 *key = (const u8 *)vArg;
    InetRadixNode *top = head->rnh_treetop;
    u32 off = (u32)top->rn_u.rn_node.rn_offset;
    InetRadixNode *leaf = inetRnSearch(key, top);
    const u8 *leafKey = leaf->rn_u.rn_leaf.rn_key;
    const u8 *leafMask = leaf->rn_u.rn_leaf.rn_mask;
    u32 checkLen = (leafMask != NULL) ? leafMask[0] : key[0];
    u32 pos = off;

    while (pos < checkLen) {
        if (key[pos] != leafKey[pos]) {
            break;
        }
        pos++;
    }

    if (pos >= checkLen) {
        if ((leaf->rn_flags & RN_F_ROOT) != 0 && leaf->rn_u.rn_leaf.rn_dupedkey != NULL) {
            return leaf->rn_u.rn_leaf.rn_dupedkey;
        }
        return leaf;
    }

    {
        u32 diffByte = (u32)(key[pos] ^ leafKey[pos]);
        s32 bitInByte = 7;
        u32 shifted = (diffByte >> 1) & 0x7F;
        while (shifted != 0) {
            shifted >>= 1;
            bitInByte--;
        }
        {
            s32 matchedBit = ~(s32)(bitInByte + 8 * pos);
            InetRadixNode *savedLeaf = leaf;
            InetRadixNode *dup = (leafMask == NULL) ? leaf->rn_u.rn_leaf.rn_dupedkey : leaf;
            while (dup != NULL) {
                if ((dup->rn_flags & RN_F_NORMAL) != 0) {
                    if (dup->rn_bit >= matchedBit) {
                        return dup;
                    }
                } else if (inetRnSatisfiesLeaf(key, dup, pos)) {
                    return dup;
                }
                dup = dup->rn_u.rn_leaf.rn_dupedkey;
            }

            {
                InetRadixNode *anc = savedLeaf->rn_parent;
                for (;;) {
                    InetRadixMask *mk = anc->rn_mklist;
                    while (mk != NULL) {
                        if ((mk->rm_flags & RN_F_NORMAL) != 0) {
                            if (mk->rm_bit >= matchedBit) {
                                return mk->rm_u.rmu_leaf;
                            }
                        } else {
                            u32 skipOff = ((u32)anc->rn_u.rn_node.rn_offset < pos) ?
                                (u32)anc->rn_u.rn_node.rn_offset : pos;
                            const u8 *rmMask = mk->rm_u.rmu_mask;
                            InetRadixNode *cand = inetRnSearchM(key, anc, rmMask);
                            while (cand != NULL && cand->rn_u.rn_leaf.rn_mask != rmMask) {
                                cand = cand->rn_u.rn_leaf.rn_dupedkey;
                            }
                            if (cand != NULL && inetRnSatisfiesLeaf(key, cand, skipOff)) {
                                return cand;
                            }
                        }
                        mk = mk->rm_mklist;
                    }
                    if (anc == top) {
                        return NULL;
                    }
                    anc = anc->rn_parent;
                }
            }
        }
    }
}

/* [167] 0x00B844 */
INET_NOINLINE InetRadixNode *inetRnInsert(const u8 *key, InetRadixNodeHead *head,
                                          int *dupEntry, InetRadixNode *nodes)
{
    InetRadixNode *top = head->rnh_treetop;
    u32 off = (u32)top->rn_u.rn_node.rn_offset;
    u32 keyLen = key[0];
    InetRadixNode *leaf = inetRnSearch(key, top);
    const u8 *leafKey = leaf->rn_u.rn_leaf.rn_key;
    u32 pos = off;

    while (pos < keyLen) {
        if (key[pos] != leafKey[pos]) {
            break;
        }
        pos++;
    }
    if (pos >= keyLen) {
        *dupEntry = 1;
        return leaf;
    }

    *dupEntry = 0;
    {
        u32 diffByte = (u32)(key[pos] ^ leafKey[pos]);
        s32 bitPos = (s32)(8 * (pos + 1));
        while (diffByte != 0) {
            diffByte >>= 1;
            bitPos--;
        }
        {
            InetRadixNode *cur = top;
            InetRadixNode *parent;
            do {
                parent = cur;
                if ((cur->rn_bmask & key[cur->rn_u.rn_node.rn_offset]) != 0) {
                    cur = cur->rn_u.rn_node.rn_right;
                } else {
                    cur = cur->rn_u.rn_node.rn_left;
                }
            } while ((u32)cur->rn_bit < (u32)bitPos);

            {
                InetRadixNode *newLeaf = &nodes[0];
                InetRadixNode *newBranch = &nodes[1];

                newLeaf->rn_bit = -1;
                newLeaf->rn_flags = RN_F_ACTIVE;
                newLeaf->rn_u.rn_leaf.rn_key = (u8 *)key;
                newLeaf->rn_parent = newBranch;

                newBranch->rn_bit = (s16)bitPos;
                newBranch->rn_bmask = (u8)(0x80 >> (bitPos & 7));
                newBranch->rn_flags = RN_F_ACTIVE;
                newBranch->rn_u.rn_node.rn_offset = bitPos >> 3;
                newBranch->rn_parent = parent;

                if ((parent->rn_bmask & key[parent->rn_u.rn_node.rn_offset]) != 0) {
                    parent->rn_u.rn_node.rn_right = newBranch;
                } else {
                    parent->rn_u.rn_node.rn_left = newBranch;
                }
                cur->rn_parent = newBranch;

                if ((newBranch->rn_bmask & key[newBranch->rn_u.rn_node.rn_offset]) != 0) {
                    newBranch->rn_u.rn_node.rn_left = cur;
                    newBranch->rn_u.rn_node.rn_right = newLeaf;
                } else {
                    newBranch->rn_u.rn_node.rn_left = newLeaf;
                    newBranch->rn_u.rn_node.rn_right = cur;
                }
                return newLeaf;
            }
        }
    }
}

/* [168] 0x00B9F0 */
INET_NOINLINE InetRadixNode *inetRnAddmask(const u8 *netmask, int searchOnly, int skip)
{
    int skipOff = (skip == 0) ? 1 : skip;
    s32 maskLen = (s32)netmask[0];
    s32 trimLen;
    InetRadixNode *found;
    int dupEntry = 0;

    if (maskLen > g_max_keylen) {
        maskLen = g_max_keylen;
    }
    if (skipOff >= maskLen) {
        return &g_mask_rnhead->rnh_nodes[0];
    }
    if (skipOff >= 2) {
        sceNet_lib_2F305274(g_rn_ones + 1, g_addmask_key + 1, skipOff - 1);
    }
    sceNet_lib_2F305274(netmask + skipOff, g_addmask_key + skipOff, maskLen - skipOff);

    trimLen = maskLen;
    while (trimLen > 0 && g_addmask_key[trimLen - 1] == 0) {
        trimLen--;
    }
    if (skipOff >= trimLen) {
        if (maskLen >= g_last_zeroed) {
            g_last_zeroed = trimLen;
        }
        return &g_mask_rnhead->rnh_nodes[0];
    }
    if (maskLen < g_last_zeroed) {
        sceNet_lib_8B615F52(g_addmask_key + maskLen, g_last_zeroed - maskLen);
    }
    g_last_zeroed = trimLen;
    g_addmask_key[0] = (u8)trimLen;

    found = inetRnSearch(g_addmask_key, g_mask_rnhead->rnh_treetop);
    if (trimLen != 0 && sceNet_lib_4BF83CF4(g_addmask_key, found->rn_u.rn_leaf.rn_key, trimLen) != 0) {
        found = NULL;
    }
    if (found != NULL || searchOnly != 0) {
        return found;
    }

    {
        u32 allocSize = (u32)(g_max_keylen + 48);
        u8 *block = (u8 *)sceNetIfhandle_lib_15CFE3C0(allocSize);
        if (block == NULL) {
            return NULL;
        }
        sceNet_lib_8B615F52(block, (int)allocSize);
        {
            InetRadixNode *nodes = (InetRadixNode *)block;
            u8 *savedMask = block + 48;
            InetRadixNode *inserted;
            s32 idx;
            s32 bitInByte = 0;
            int isNormal = 1;

            sceNet_lib_2F305274(g_addmask_key, savedMask, trimLen);
            inserted = inetRnInsert(savedMask, g_mask_rnhead, &dupEntry, nodes);
            if (dupEntry != 0) {
                sceNetIfhandle_lib_76BAD213(block);
                return inserted;
            }
            idx = skipOff;
            while (idx < trimLen && savedMask[idx] == 0xFF) {
                idx++;
            }
            if (idx < trimLen) {
                u8 byteVal = savedMask[idx];
                u32 bitMask = 0x80;
                while ((byteVal & bitMask) != 0) {
                    bitMask >>= 1;
                    bitInByte++;
                }
                if (byteVal != g_normal_chars[bitInByte] || idx != trimLen - 1) {
                    isNormal = 0;
                }
            }
            inserted->rn_bit = (s16)(~(bitInByte + 8 * idx));
            if (isNormal) {
                inserted->rn_flags |= RN_F_NORMAL;
            }
            return inserted;
        }
    }
}

/* [169] 0x00BD34 */
INET_NOINLINE InetRadixNode *inetRnAddroute(void *vArg, void *nArg,
                                            InetRadixNodeHead *head, InetRadixNode *treenodes)
{
    const u8 *key = (const u8 *)vArg;
    const u8 *netmask = (const u8 *)nArg;
    InetRadixNode *top = head->rnh_treetop;
    InetRadixNode *maskNode = NULL;
    s32 maskBit = 0;
    s32 negMaskBit = 0;
    int keyExisted = 0;
    InetRadixNode *leaf;
    InetRadixNode *savedLeaf;
    InetRadixNode *parent;

    if (netmask != NULL) {
        maskNode = inetRnAddmask(netmask, 0, top->rn_u.rn_node.rn_offset);
        if (maskNode == NULL) {
            return NULL;
        }
        netmask = maskNode->rn_u.rn_leaf.rn_key;
        maskBit = (s16)(~maskNode->rn_bit);
        negMaskBit = maskNode->rn_bit;
    }

    leaf = inetRnInsert(key, head, &keyExisted, treenodes);
    savedLeaf = leaf;

    if (keyExisted != 0) {
        InetRadixNode *cur = leaf;
        InetRadixNode *prev = leaf;
        while (cur != NULL) {
            const u8 *curMask = cur->rn_u.rn_leaf.rn_mask;
            if (curMask == netmask) {
                return NULL;
            }
            if (netmask == NULL) {
                break;
            }
            if (curMask != NULL) {
                if (negMaskBit < cur->rn_bit) {
                    break;
                }
                if (!inetRnRefines(netmask, curMask) && !inetRnLexobetter(curMask, netmask)) {
                    break;
                }
            }
            prev = cur;
            cur = cur->rn_u.rn_leaf.rn_dupedkey;
        }

        if (cur == leaf) {
            InetRadixNode *oldParent = prev->rn_parent;
            treenodes->rn_u.rn_leaf.rn_dupedkey = prev;
            treenodes->rn_parent = oldParent;
            treenodes->rn_flags = prev->rn_flags;
            prev->rn_parent = treenodes;
            if (oldParent->rn_u.rn_node.rn_left == prev) {
                oldParent->rn_u.rn_node.rn_left = treenodes;
            } else {
                oldParent->rn_u.rn_node.rn_right = treenodes;
            }
            savedLeaf = treenodes;
        } else {
            InetRadixNode *nextDup = prev->rn_u.rn_leaf.rn_dupedkey;
            treenodes->rn_parent = prev;
            treenodes->rn_u.rn_leaf.rn_dupedkey = nextDup;
            prev->rn_u.rn_leaf.rn_dupedkey = treenodes;
            if (nextDup != NULL) {
                nextDup->rn_parent = treenodes;
            }
        }
        leaf = treenodes;
        leaf->rn_flags = RN_F_ACTIVE;
        leaf->rn_u.rn_leaf.rn_key = (u8 *)key;
        leaf->rn_bit = -1;
    }

    if (netmask != NULL) {
        leaf->rn_bit = maskNode->rn_bit;
        leaf->rn_flags |= (u8)(maskNode->rn_flags & RN_F_NORMAL);
        leaf->rn_u.rn_leaf.rn_mask = (u8 *)netmask;
    }

    parent = savedLeaf->rn_parent;
    if (keyExisted == 0) {
        s32 parentBit = ~parent->rn_bit;
        InetRadixNode *sibling = (parent->rn_u.rn_node.rn_right == savedLeaf) ?
            parent->rn_u.rn_node.rn_left : parent->rn_u.rn_node.rn_right;

        if (sibling->rn_bit < 0) {
            InetRadixMask **mkSlot = &parent->rn_mklist;
            InetRadixNode *sibDup = sibling;
            while (sibDup != NULL) {
                if (sibDup->rn_u.rn_leaf.rn_mask != NULL &&
                    sibDup->rn_bit >= parentBit &&
                    sibDup->rn_mklist == NULL) {
                    InetRadixMask *newMk = inetRnNewRadixMask(sibDup, NULL);
                    *mkSlot = newMk;
                    if (newMk != NULL) {
                        mkSlot = &newMk->rm_mklist;
                    }
                }
                sibDup = sibDup->rn_u.rn_leaf.rn_dupedkey;
            }
        } else {
            InetRadixMask **sibSlot = &sibling->rn_mklist;
            InetRadixMask *mk = *sibSlot;
            if (mk != NULL) {
                while (mk != NULL && mk->rm_bit < parentBit) {
                    sibSlot = &mk->rm_mklist;
                    mk = mk->rm_mklist;
                }
                parent->rn_mklist = mk;
                *sibSlot = NULL;
            }
        }
    }

    if (netmask == NULL || parent->rn_bit < maskBit) {
        return leaf;
    }

    {
        InetRadixNode *attach = parent;
        InetRadixMask **mkSlot;
        InetRadixMask *mk;
        s32 targetBit = leaf->rn_bit;

        while (attach != top && attach->rn_parent->rn_bit >= maskBit) {
            attach = attach->rn_parent;
        }
        mkSlot = &attach->rn_mklist;
        mk = *mkSlot;
        while (mk != NULL) {
            if (mk->rm_bit < targetBit) {
                mkSlot = &mk->rm_mklist;
                mk = mk->rm_mklist;
                continue;
            }
            if (targetBit < mk->rm_bit) {
                break;
            }
            {
                const u8 *existingMask;
                if ((mk->rm_flags & RN_F_NORMAL) != 0) {
                    existingMask = mk->rm_u.rmu_leaf->rn_u.rn_leaf.rn_mask;
                    if ((leaf->rn_flags & RN_F_NORMAL) != 0) {
                        return leaf;
                    }
                } else {
                    existingMask = mk->rm_u.rmu_mask;
                }
                if (existingMask == netmask) {
                    leaf->rn_mklist = mk;
                    mk->rm_refs++;
                    return leaf;
                }
                if (!inetRnRefines(netmask, existingMask) && !inetRnLexobetter(existingMask, netmask)) {
                    break;
                }
            }
            mkSlot = &mk->rm_mklist;
            mk = mk->rm_mklist;
        }
        *mkSlot = inetRnNewRadixMask(leaf, *mkSlot);
        return leaf;
    }
}

/* [170] 0x00C3B4 */
INET_NOINLINE InetRadixNode *inetRnDelete(void *vArg, void *nArg, InetRadixNodeHead *head)
{
    const u8 *key = (const u8 *)vArg;
    const u8 *netmask = (const u8 *)nArg;
    InetRadixNode *top = head->rnh_treetop;
    u32 off = (u32)top->rn_u.rn_node.rn_offset;
    InetRadixNode *headLeaf = inetRnSearch(key, top);
    InetRadixNode *target = headLeaf;
    const u8 *targetMask;

    if (headLeaf == NULL) {
        return NULL;
    }
    if (key[0] != off &&
        sceNet_lib_4BF83CF4(key + off, headLeaf->rn_u.rn_leaf.rn_key + off, (int)(key[0] - off)) != 0) {
        return NULL;
    }

    if (netmask != NULL) {
        InetRadixNode *maskNode = inetRnAddmask(netmask, 1, (int)off);
        if (maskNode == NULL) {
            return NULL;
        }
        targetMask = maskNode->rn_u.rn_leaf.rn_key;
        while (target != NULL && target->rn_u.rn_leaf.rn_mask != targetMask) {
            target = target->rn_u.rn_leaf.rn_dupedkey;
        }
        if (target == NULL) {
            return NULL;
        }
    } else {
        targetMask = headLeaf->rn_u.rn_leaf.rn_mask;
    }

    if (targetMask != NULL && target->rn_mklist != NULL) {
        InetRadixMask *mk = target->rn_mklist;
        int removeFromParent = 0;

        if ((target->rn_flags & RN_F_NORMAL) != 0) {
            if (mk->rm_u.rmu_leaf != target) {
                return NULL;
            }
            if (mk->rm_refs > 0) {
                return NULL;
            }
            removeFromParent = 1;
        } else if (mk->rm_u.rmu_mask == targetMask) {
            mk->rm_refs--;
            if (mk->rm_refs < 0) {
                removeFromParent = 1;
            }
        }

        if (removeFromParent) {
            s32 maskBit = ~target->rn_bit;
            InetRadixNode *anc = headLeaf->rn_parent;
            if (anc->rn_bit >= maskBit) {
                InetRadixMask **mkSlot;
                while (anc != top && anc->rn_parent->rn_bit >= maskBit) {
                    anc = anc->rn_parent;
                }
                mkSlot = &anc->rn_mklist;
                while (*mkSlot != NULL && *mkSlot != mk) {
                    mkSlot = &(*mkSlot)->rm_mklist;
                }
                if (*mkSlot == mk) {
                    *mkSlot = mk->rm_mklist;
                    mk->rm_mklist = g_rn_mkfreelist;
                    g_rn_mkfreelist = mk;
                } else if ((target->rn_flags & RN_F_NORMAL) != 0) {
                    return NULL;
                }
            }
        }
    }

    if ((target->rn_flags & RN_F_ROOT) != 0) {
        return NULL;
    }

    {
        InetRadixNode *parent = target->rn_parent;
        InetRadixNode *dupNext = headLeaf->rn_u.rn_leaf.rn_dupedkey;

        if (dupNext != NULL) {
            InetRadixNode *survivor = headLeaf;
            if (target == headLeaf) {
                survivor = dupNext;
                dupNext->rn_parent = parent;
                if (parent->rn_u.rn_node.rn_left == target) {
                    parent->rn_u.rn_node.rn_left = dupNext;
                } else {
                    parent->rn_u.rn_node.rn_right = dupNext;
                }
            } else {
                InetRadixNode *prev = headLeaf;
                while (prev != NULL && prev->rn_u.rn_leaf.rn_dupedkey != target) {
                    prev = prev->rn_u.rn_leaf.rn_dupedkey;
                }
                if (prev != NULL) {
                    prev->rn_u.rn_leaf.rn_dupedkey = target->rn_u.rn_leaf.rn_dupedkey;
                    if (target->rn_u.rn_leaf.rn_dupedkey != NULL) {
                        target->rn_u.rn_leaf.rn_dupedkey->rn_parent = prev;
                    }
                }
            }
            {
                InetRadixNode *oldBranch = &target[1];
                if ((oldBranch->rn_flags & RN_F_ACTIVE) != 0) {
                    InetRadixNode *newBranch = &survivor[1];
                    InetRadixNode *branchParent = oldBranch->rn_parent;
                    *newBranch = *oldBranch;
                    if (branchParent->rn_u.rn_node.rn_left == oldBranch) {
                        branchParent->rn_u.rn_node.rn_left = newBranch;
                    } else {
                        branchParent->rn_u.rn_node.rn_right = newBranch;
                    }
                    newBranch->rn_u.rn_node.rn_left->rn_parent = newBranch;
                    newBranch->rn_u.rn_node.rn_right->rn_parent = newBranch;
                }
            }
        } else {
            InetRadixNode *sibling = (parent->rn_u.rn_node.rn_left == target) ?
                parent->rn_u.rn_node.rn_right : parent->rn_u.rn_node.rn_left;
            InetRadixNode *grandParent = parent->rn_parent;

            if (grandParent->rn_u.rn_node.rn_right == parent) {
                grandParent->rn_u.rn_node.rn_right = sibling;
            } else {
                grandParent->rn_u.rn_node.rn_left = sibling;
            }
            sibling->rn_parent = grandParent;

            if (parent->rn_mklist != NULL) {
                if (sibling->rn_bit < 0) {
                    InetRadixMask *mk = parent->rn_mklist;
                    InetRadixNode *sibScan = sibling;
                    while (mk != NULL && sibScan != NULL) {
                        if (sibScan->rn_mklist == mk) {
                            InetRadixMask *nextMk = mk->rm_mklist;
                            sibScan->rn_mklist = NULL;
                            mk->rm_refs--;
                            if (mk->rm_refs < 0) {
                                mk->rm_mklist = g_rn_mkfreelist;
                                g_rn_mkfreelist = mk;
                            }
                            mk = nextMk;
                        }
                        sibScan = sibScan->rn_u.rn_leaf.rn_dupedkey;
                    }
                } else {
                    InetRadixMask **tailSlot = &sibling->rn_mklist;
                    while (*tailSlot != NULL) {
                        tailSlot = &(*tailSlot)->rm_mklist;
                    }
                    *tailSlot = parent->rn_mklist;
                }
            }

            {
                InetRadixNode *oldBranch = &target[1];
                if (parent != oldBranch) {
                    InetRadixNode *branchParent = oldBranch->rn_parent;
                    *parent = *oldBranch;
                    parent->rn_u.rn_node.rn_left->rn_parent = parent;
                    parent->rn_u.rn_node.rn_right->rn_parent = parent;
                    if (branchParent->rn_u.rn_node.rn_left == oldBranch) {
                        branchParent->rn_u.rn_node.rn_left = parent;
                    } else {
                        branchParent->rn_u.rn_node.rn_right = parent;
                    }
                }
            }
        }
    }

    target->rn_flags &= (u8)~RN_F_ACTIVE;
    target[1].rn_flags &= (u8)~RN_F_ACTIVE;
    return target;
}

/* [171] 0x00C7F0 */
INET_NOINLINE int inetRnWalktree(InetRadixNodeHead *head,
                                 int (*cb)(InetRadixNode *, void *), void *arg)
{
    InetRadixNode *rn = head->rnh_treetop;
    while (rn->rn_bit >= 0) {
        rn = rn->rn_u.rn_node.rn_left;
    }

    for (;;) {
        InetRadixNode *base = rn;
        InetRadixNode *parent = rn->rn_parent;
        InetRadixNode *next;

        while (parent->rn_u.rn_node.rn_right == rn && (rn->rn_flags & RN_F_ROOT) == 0) {
            rn = parent;
            parent = parent->rn_parent;
        }
        next = parent->rn_u.rn_node.rn_right;
        while (next->rn_bit >= 0) {
            next = next->rn_u.rn_node.rn_left;
        }

        while (base != NULL) {
            InetRadixNode *cur = base;
            base = base->rn_u.rn_leaf.rn_dupedkey;
            if ((cur->rn_flags & RN_F_ROOT) == 0) {
                int err = cb(cur, arg);
                if (err != 0) {
                    return err;
                }
            }
        }

        rn = next;
        if ((rn->rn_flags & RN_F_ROOT) != 0) {
            return 0;
        }
    }
}

/* [172] 0x00C910 */
INET_NOINLINE int inetRnRefines(const u8 *mTest, const u8 *nBase)
{
    u32 nLen = nBase[0];
    u32 mLen = mTest[0];
    u32 commonLen = (nLen > mLen) ? mLen : nLen;
    int longerDiff = (int)nLen - (int)mLen;
    int masksEqual = 1;
    u32 idx;

    for (idx = 1; idx < commonLen; idx++) {
        u8 nByte = nBase[idx];
        u8 mByte = mTest[idx];
        if (nByte != mByte) {
            masksEqual = 0;
        }
        if ((nByte & (u8)(~mByte)) != 0) {
            return 0;
        }
    }
    for (idx = commonLen; idx < nLen; idx++) {
        if (nBase[idx] != 0) {
            return 0;
        }
    }
    if (masksEqual && longerDiff < 0) {
        for (idx = commonLen; idx < mLen; idx++) {
            if (mTest[idx] != 0) {
                return 1;
            }
        }
    }
    return !masksEqual;
}

/* [173] 0x00C9F0 */
INET_NOINLINE InetRadixNode *inetRnLookup(void *vArg, void *mArg, InetRadixNodeHead *head)
{
    const u8 *savedMask = NULL;
    InetRadixNode *match;

    if (mArg != NULL) {
        InetRadixNode *maskNode = inetRnAddmask((const u8 *)mArg, 1,
                                                head->rnh_treetop->rn_u.rn_node.rn_offset);
        if (maskNode == NULL) {
            return NULL;
        }
        savedMask = maskNode->rn_u.rn_leaf.rn_key;
    }

    match = inetRnMatch(vArg, head);
    if (match != NULL && savedMask != NULL) {
        while (match != NULL && match->rn_u.rn_leaf.rn_mask != savedMask) {
            match = match->rn_u.rn_leaf.rn_dupedkey;
        }
    }
    return match;
}

/* [174] 0x00CAA4 */
INET_NOINLINE int inetRnInithead(void **headOut, int offBits)
{
    InetRadixNodeHead *rnh;
    InetRadixNode *leftLeaf;
    InetRadixNode *rootNode;
    InetRadixNode *rightLeaf;

    if (*headOut != NULL) {
        return 1;
    }
    rnh = (InetRadixNodeHead *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetRadixNodeHead));
    if (rnh == NULL) {
        return 0;
    }
    sceNet_lib_8B615F52(rnh, sizeof(InetRadixNodeHead));
    *headOut = rnh;

    leftLeaf = &rnh->rnh_nodes[0];
    rootNode = &rnh->rnh_nodes[1];
    rightLeaf = &rnh->rnh_nodes[2];

    leftLeaf->rn_bit = (s16)(~offBits);
    leftLeaf->rn_flags = RN_F_ROOT | RN_F_ACTIVE;
    leftLeaf->rn_parent = rootNode;
    leftLeaf->rn_u.rn_leaf.rn_key = g_rn_zeros;

    rootNode->rn_bit = (s16)offBits;
    rootNode->rn_bmask = (u8)(0x80 >> (offBits & 7));
    rootNode->rn_flags = RN_F_ROOT | RN_F_ACTIVE;
    rootNode->rn_parent = rootNode;
    rootNode->rn_u.rn_node.rn_offset = offBits >> 3;
    rootNode->rn_u.rn_node.rn_left = leftLeaf;
    rootNode->rn_u.rn_node.rn_right = rightLeaf;

    *rightLeaf = *leftLeaf;
    rightLeaf->rn_u.rn_leaf.rn_key = g_rn_ones;

    rnh->rnh_treetop = rootNode;
    rnh->rnh_addaddr = inetRnAddroute;
    rnh->rnh_deladdr = inetRnDelete;
    rnh->rnh_matchaddr = inetRnMatch;
    rnh->rnh_lookup = inetRnLookup;
    rnh->rnh_walktree = inetRnWalktree;
    return 1;
}

/* [175] 0x00CC04 */
INET_NOINLINE void inetRnInit(void)
{
    InetDomain *dp = g_domains;
    s32 maxLen = 0;

    while (dp != NULL) {
        if (dp->dom_maxrtkey > maxLen) {
            maxLen = dp->dom_maxrtkey;
        }
        dp = dp->dom_next;
    }

    g_max_keylen = maxLen;
    g_rn_mkfreelist = NULL;
    g_last_zeroed = 0;
    g_mask_rnhead = NULL;

    if (maxLen != 0) {
        u32 totalBytes = (u32)(3 * maxLen);
        u8 *buf = (u8 *)sceNetIfhandle_lib_15CFE3C0(totalBytes);
        sceNet_lib_8B615F52(buf, (int)totalBytes);
        g_rn_zeros = buf;
        g_rn_ones = buf + maxLen;
        g_addmask_key = buf + 2 * maxLen;
        {
            s32 idx;
            for (idx = 0; idx < maxLen; idx++) {
                g_rn_ones[idx] = 0xFF;
            }
        }
        inetRnInithead((void **)&g_mask_rnhead, 0);
    }
}
