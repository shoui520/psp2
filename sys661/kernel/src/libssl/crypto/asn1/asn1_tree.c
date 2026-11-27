/*
 * RSA-COMMON ASN.1 BER/DER tree and item parser (0x0021E0..0x00347F).
 */
#include "libssl_internal.h"

/* 0x0021E0 */
int LIBSSL_NOINLINE sslAsn1NavigateElement(SslAsn1Element *outElem, const SslAsn1Element *rootElem,
                                           const u8 *navScript)
{
    int ret = 0;
    u32 remLen;
    const u8 *curPtr;

    if (rootElem != outElem) {
        outElem->length = rootElem->length;
        outElem->data = rootElem->data;
        outElem->tag = rootElem->tag;
        outElem->encoding = rootElem->encoding;
        outElem->headerLength = rootElem->headerLength;
        outElem->flags = rootElem->flags;
        outElem->prefix = rootElem->prefix;
    }

    remLen = rootElem->length;
    curPtr = rootElem->data;

    for (;;) {
        u8 opByte = *navScript++;
        u8 opClass = opByte & 0xC0U;
        u8 opLow = opByte & 0x1FU;

        if ((opByte & 0x20U) != 0) {
            if (opClass == 0 && opLow == 0) {
                return ret;
            }
            if (opClass == 0 && opLow == 1) {
                if ((outElem->encoding & 0x20U) == 0) {
                    return 4;
                }
                curPtr = outElem->data;
                if ((outElem->encoding & 2U) == 0) {
                    remLen = outElem->length;
                }
                ret = sceSsl_lib_C85F6FCE(outElem, curPtr, remLen);
                if (ret != 0) {
                    return ret;
                }
                if (remLen < outElem->length + (u32)outElem->headerLength) {
                    return 2;
                }
            } else {
                u32 targetTag = opLow;
                if (opLow == 31U) {
                    u32 accTag = 0;
                    u32 bitCount = 0;
                    for (;;) {
                        u8 nextByte = *navScript++;
                        bitCount += 7U;
                        targetTag = accTag | (u32)(nextByte & 0x7FU);
                        if ((nextByte & 0x80U) != 0) {
                            break;
                        }
                        accTag = targetTag << 7;
                        if (bitCount >= 33U) {
                            return 1;
                        }
                    }
                }
                while (outElem->tag != targetTag || opClass != (outElem->encoding & 0xC0U)) {
                    u32 stepLen = (u32)outElem->headerLength + outElem->length;
                    curPtr += stepLen;
                    if (remLen < stepLen) {
                        return 2;
                    }
                    remLen -= stepLen;
                    ret = sceSsl_lib_C85F6FCE(outElem, curPtr, remLen);
                    if (ret != 0) {
                        return ret;
                    }
                    if (remLen < outElem->length + (u32)outElem->headerLength) {
                        return 2;
                    }
                }
            }
        } else {
            int skipCount;
            int stepIdx;

            if ((opByte & 0x80U) != 0) {
                if ((outElem->encoding & 0x20U) == 0) {
                    return 4;
                }
                curPtr = outElem->data;
                if ((outElem->encoding & 2U) == 0) {
                    remLen = outElem->length;
                }
                ret = sceSsl_lib_C85F6FCE(outElem, curPtr, remLen);
                if (ret != 0) {
                    return ret;
                }
                if (remLen < outElem->length + (u32)outElem->headerLength) {
                    return 2;
                }
            }

            skipCount = (int)opLow + (((outElem->encoding & 0xC0U) != 0) ? 1 : 0);
            for (stepIdx = 0; stepIdx < skipCount; stepIdx++) {
                u32 stepLen = (u32)outElem->headerLength + outElem->length;
                curPtr += stepLen;
                if (remLen < stepLen) {
                    return 2;
                }
                remLen -= stepLen;
                ret = sceSsl_lib_C85F6FCE(outElem, curPtr, remLen);
                if (ret != 0) {
                    return ret;
                }
                if (remLen < outElem->length + (u32)outElem->headerLength) {
                    return 2;
                }
                if ((outElem->encoding & 0xC0U) != 0) {
                    skipCount++;
                }
            }
            if ((opClass & 0x40U) != 0) {
                return ret;
            }
        }
    }
}

/* 0x0024B4 */
int LIBSSL_NOINLINE sslAsn1InitTree(SslAsn1Tree *tree, SslAsn1Element *elements, u32 count, u32 capacity)
{
    tree->count = count;
    tree->capacity = capacity;
    if (elements != NULL) {
        tree->flags = 0;
        tree->elements = elements;
        return 0;
    }
    tree->flags = 2;
    tree->elements = NULL;
    return 2;
}

/* 0x0024D8 */
int LIBSSL_NOINLINE sslAsn1AppendElement(SslAsn1Tree *tree, u32 *outIndex)
{
    if (tree->count >= tree->capacity) {
        int err = sslAsn1ReserveTree(tree, tree->count * 2U + 4U);
        if (err != 0) {
            return err;
        }
    }
    *outIndex = tree->count;
    sslMemset(&tree->elements[tree->count], 0, sizeof(SslAsn1Element));
    tree->count++;
    return 0;
}

/* 0x002568 */
int LIBSSL_NOINLINE sslAsn1ReserveTree(SslAsn1Tree *tree, u32 newCap)
{
    SslAsn1Element *newElems;
    u32 byteSize;
    int didRealloc;

    if (tree->count >= newCap) {
        return 0;
    }
    if ((tree->flags & 2U) == 0) {
        return 6;
    }

    byteSize = newCap * sizeof(SslAsn1Element);
    if (tree->elements != NULL) {
        newElems = (SslAsn1Element *)sslCryptoRealloc(tree->elements, byteSize);
        didRealloc = 1;
        if (newElems != NULL) {
            sslMemset(&newElems[tree->capacity], 0, (newCap - tree->capacity) * sizeof(SslAsn1Element));
        }
    } else {
        newElems = (SslAsn1Element *)sceSsl_lib_0861D934(byteSize);
        didRealloc = 0;
        if (newElems != NULL) {
            sslMemset(newElems, 0, byteSize);
        }
    }
    if (newElems == NULL) {
        return 5;
    }

    if (didRealloc && tree->elements != newElems) {
        u32 oldBase = (u32)tree->elements;
        u32 newBase = (u32)newElems;
        u32 oldEnd = oldBase + tree->count * sizeof(SslAsn1Element);
        u32 idx;

        for (idx = 0; idx < tree->count; idx++) {
            u32 ptrVal = (u32)newElems[idx].parent;
            if (ptrVal >= oldBase && ptrVal <= oldEnd) {
                newElems[idx].parent = (SslAsn1Element *)(ptrVal + newBase - oldBase);
            }
            ptrVal = (u32)newElems[idx].next;
            if (ptrVal >= oldBase && ptrVal <= oldEnd) {
                newElems[idx].next = (SslAsn1Element *)(ptrVal + newBase - oldBase);
            }
            ptrVal = (u32)newElems[idx].child;
            if (ptrVal >= oldBase && ptrVal <= oldEnd) {
                newElems[idx].child = (SslAsn1Element *)(ptrVal + newBase - oldBase);
            }
        }
    }

    tree->elements = newElems;
    tree->capacity = newCap;
    return 0;
}

/* 0x00270C */
int LIBSSL_NOINLINE sslAsn1DestroyTree(SslAsn1Tree *tree)
{
    u32 idx;

    for (idx = 0; idx < tree->capacity; idx++) {
        SslAsn1Element *elem = &tree->elements[idx];
        if (elem->data != NULL && (elem->flags & 1U) != 0) {
            sceSsl_lib_5E5C873A((void *)elem->data);
            elem->data = NULL;
        }
    }
    if ((tree->flags & 2U) != 0) {
        sceSsl_lib_5E5C873A(tree->elements);
        tree->elements = NULL;
    }
    tree->count = 0;
    if ((tree->flags & 1U) != 0) {
        sceSsl_lib_5E5C873A(tree);
        return 1;
    }
    return 0;
}

/* 0x0027C8 */
int LIBSSL_NOINLINE sslAsn1DecodeFlatList(SslAsn1Tree *tree, const u8 *derBuf, u32 derLen)
{
    int prevIdx = -1;
    const u8 *curPtr = derBuf;
    const u8 *endPtr = derBuf + derLen;

    for (;;) {
        u32 elemIdx = 0;
        SslAsn1Element *elem;
        int ret;

        ret = sslAsn1AppendElement(tree, &elemIdx);
        if (ret != 0) {
            return ret;
        }
        elem = &tree->elements[elemIdx];
        ret = sceSsl_lib_C85F6FCE(elem, curPtr, (u32)(endPtr - curPtr));
        if (ret != 0) {
            return ret;
        }
        curPtr += (u32)elem->headerLength + elem->length;
        if (prevIdx != -1) {
            tree->elements[prevIdx].next = elem;
        }
        elem->next = NULL;
        elem->parent = NULL;
        prevIdx = (int)elemIdx;
        if (curPtr > endPtr) {
            return 2;
        }
        if (curPtr == endPtr) {
            return 0;
        }
    }
}

/* 0x0028B4 */
void LIBSSL_NOINLINE sslAsn1ClearTreeStruct(SslAsn1Tree *tree)
{
    sslMemset(tree, 0, sizeof(SslAsn1Tree));
}

/* 0x0028D4 */
int LIBSSL_NOINLINE sceSsl_lib_C85F6FCE(SslAsn1Element *elem, const u8 *derBuf, u32 derLen)
{
    u32 tagVal;
    u32 cursor;
    u32 lenVal;

    if (derLen < 2U) {
        return 2;
    }

    tagVal = (u32)(derBuf[0] & 0x1FU);
    elem->encoding = (u8)(derBuf[0] & 0xE0U);
    cursor = 1U;

    if (tagVal == 31U) {
        u32 accTag = 0;
        u32 bitCount = 0;
        for (;;) {
            u8 tagByte;
            bitCount += 7U;
            if (cursor >= derLen) {
                return 2;
            }
            tagByte = derBuf[cursor++];
            tagVal = accTag | (u32)(tagByte & 0x7FU);
            if ((tagByte & 0x80U) != 0) {
                break;
            }
            accTag = tagVal << 7;
            if (bitCount >= 33U) {
                return 1;
            }
        }
    }

    elem->tag = tagVal;
    if (cursor >= derLen) {
        return 2;
    }

    lenVal = derBuf[cursor++];
    if ((lenVal & 0x80U) != 0) {
        lenVal &= 0x7FU;
        if (lenVal != 0) {
            u32 lenBytes = lenVal;
            u32 accLen = 0;
            if (lenBytes >= 5U) {
                return 3;
            }
            if (cursor + lenBytes >= derLen) {
                return 2;
            }
            while (lenBytes > 0) {
                accLen = (accLen << 8) | derBuf[cursor++];
                lenBytes--;
            }
            lenVal = accLen;
        } else {
            elem->encoding |= 2U;
            elem->flags |= 8U;
        }
    }

    elem->headerLength = (u8)cursor;
    elem->length = lenVal;
    elem->data = &derBuf[cursor];
    return 0;
}

/* 0x0029F8 */
int LIBSSL_NOINLINE sslAsn1DecodeTree(SslAsn1Tree *tree, const u8 *derBuf, u32 derLen)
{
    SslAsn1Element *curElem = NULL;
    const u8 *curPtr = derBuf;
    const u8 *limitPtr = derBuf + derLen;
    const u8 *rootEnd = limitPtr;
    int stepMode = 1;
    int isChild = 1;
    int prevIdx = -1;
    u32 indefFlag = 0;

    for (;;) {
        u32 newIdx = 0;
        SslAsn1Element *elem;
        SslAsn1Element *anchor;
        u8 encByte;
        int ret;

        while (stepMode != 1) {
            SslAsn1Element *parentElem = curElem->parent;
            int idxDelta = (int)(parentElem - curElem);
            curElem = parentElem;
            isChild = 0;
            prevIdx += idxDelta;
            if (parentElem == NULL) {
                return 0;
            }
            indefFlag = 0;
            limitPtr = rootEnd;
            if (parentElem->parent != NULL) {
                SslAsn1Element *grandParent = parentElem->parent;
                indefFlag = grandParent->encoding & 2U;
                if (indefFlag == 0) {
                    if ((u32)grandParent->data + grandParent->length < (u32)grandParent->data) {
                        return 3;
                    }
                    limitPtr = grandParent->data + grandParent->length;
                }
            }
            if (limitPtr < curPtr) {
                return 2;
            }
            stepMode = (limitPtr != curPtr) ? 1 : 2;
        }

        ret = sslAsn1AppendElement(tree, &newIdx);
        if (ret != 0) {
            return ret;
        }
        elem = &tree->elements[newIdx];
        ret = sceSsl_lib_C85F6FCE(elem, curPtr, (u32)(limitPtr - curPtr));
        if (ret != 0) {
            return ret;
        }

        encByte = elem->encoding;
        curPtr += elem->headerLength;
        anchor = (prevIdx != -1) ? &tree->elements[prevIdx] : NULL;

        if (isChild) {
            elem->parent = anchor;
            if (anchor != NULL) {
                anchor->child = elem;
                if (indefFlag == 0) {
                    if ((u32)anchor->data + anchor->length < (u32)anchor->data) {
                        return 3;
                    }
                    limitPtr = anchor->data + anchor->length;
                }
                indefFlag = anchor->encoding & 2U;
            } else {
                indefFlag = 0;
            }
            elem->next = NULL;
        } else {
            elem->next = NULL;
            elem->parent = anchor->parent;
            anchor->next = elem;
        }

        curElem = elem;
        isChild = 0;
        prevIdx = (int)newIdx;

        if (elem->tag == 0 && elem->length == 0 && (elem->encoding & 0xC0U) == 0) {
            if (indefFlag == 0) {
                return 11;
            }
            stepMode = 2;
            elem->parent->length = (u32)(curPtr - elem->parent->data);
            continue;
        }

        if ((encByte & 0x20U) == 0) {
            curPtr += elem->length;
            if (curPtr < derBuf) {
                return 3;
            }
        } else if ((encByte & 2U) != 0) {
            indefFlag = encByte & 2U;
            isChild = 1;
        } else if (elem->length != 0) {
            indefFlag = 0;
            isChild = 1;
        } else {
            if (curPtr < derBuf) {
                return 3;
            }
        }

        if (limitPtr < curPtr) {
            return 2;
        }
        if ((encByte & 2U) != 0 && (encByte & 0x20U) == 0) {
            return 12;
        }
        stepMode = (curPtr != limitPtr) ? 1 : 2;
    }
}

/* 0x002CB0 */
int LIBSSL_NOINLINE sslAsn1ExtractFields(const SslAsn1Item *rootItem, SslAsn1Item **outItems,
                                         const SslAsn1FieldSpec *specs, u32 specCount, u32 flags)
{
    SslAsn1Element rootElem;
    SslAsn1Element foundElem;
    int ret;
    u32 idx;

    ret = sceSsl_lib_C85F6FCE(&rootElem, (const u8 *)rootItem->data, rootItem->length);
    if (ret != 0) {
        return ret;
    }

    if (rootElem.tag == ASN1_TAG_BIT_STRING && rootElem.length != 0) {
        rootElem.length--;
        rootElem.data++;
    }
    rootElem.encoding |= 0x20U;

    for (idx = 0; idx < specCount; idx++) {
        if (outItems[idx] != NULL) {
            outItems[idx]->itemType = 32U;
        }
    }

    for (idx = 0; idx < specCount; idx++) {
        const SslAsn1FieldSpec *spec = &specs[idx];
        if (rootItem->itemType != spec->expectedType) {
            return 101;
        }
        ret = sslAsn1NavigateElement(&foundElem, &rootElem, spec->navScript);
        if (ret != 0) {
            return ret;
        }
        if (spec->allowedTagMask != 0 && (((1U << foundElem.tag) & spec->allowedTagMask) == 0)) {
            return 102;
        }
        if (outItems[idx] != NULL) {
            u32 targetType = spec->targetType;
            const void *srcPtr;
            u32 srcLen;

            if (targetType == 32U) {
                targetType = foundElem.tag;
                srcPtr = foundElem.data;
                srcLen = foundElem.length;
            } else {
                srcPtr = foundElem.data - foundElem.headerLength;
                srcLen = foundElem.length + (u32)foundElem.headerLength;
            }
            ret = sslAsn1SetItemData(outItems[idx], targetType, srcPtr, srcLen, flags);
            if (ret != 0) {
                return ret;
            }
        }
    }

    return 0;
}

/* 0x002E84 */
int LIBSSL_NOINLINE sslAsn1DecodeItemList(const SslAsn1Item *srcItem, SslAsn1ItemList *outList, u32 flags)
{
    SslAsn1Element wrapperElem;
    SslAsn1Tree flatTree;
    const u8 *derPtr;
    u32 derLen;
    u32 idx;
    int ret;

    if ((flags & 0x1000U) != 0) {
        ret = sceSsl_lib_C85F6FCE(&wrapperElem, (const u8 *)srcItem->data, srcItem->length);
        if (ret != 0) {
            return ret;
        }
        derLen = wrapperElem.length;
        derPtr = wrapperElem.data;
    } else {
        derPtr = (const u8 *)srcItem->data;
        derLen = srcItem->length;
    }

    sslAsn1InitTree(&flatTree, NULL, 0, 0);
    ret = sslAsn1DecodeFlatList(&flatTree, derPtr, derLen);
    if (ret != 0) {
        return ret;
    }

    for (idx = 0; idx < flatTree.count; idx++) {
        u32 itemIdx = 0;
        SslAsn1Element *elem = &flatTree.elements[idx];
        ret = sslAsn1AppendItem(outList, &itemIdx);
        if (ret != 0) {
            return ret;
        }
        ret = sslAsn1SetItemData(&outList->items[itemIdx], elem->tag, elem->data, elem->length, flags);
        if (ret != 0) {
            return ret;
        }
    }

    sslAsn1DestroyTree(&flatTree);
    return 0;
}

/* 0x002FB4 */
void LIBSSL_NOINLINE sslAsn1ClearItem(SslAsn1Item *item)
{
    sslMemset(item, 0, sizeof(SslAsn1Item));
}

/* 0x002FD4 */
int LIBSSL_NOINLINE sslAsn1SetItemData(SslAsn1Item *item, u32 itemType, const void *srcData,
                                       u32 dataLen, u32 flags)
{
    int refOnly = (int)(flags & 1U);

    if ((item->flags & 0x400U) != 0) {
        return 103;
    }

    if (refOnly) {
        if (item->data != NULL && (item->flags & 0x200U) == 0) {
            sceSsl_lib_5E5C873A(item->data);
        }
    } else if (item->length < dataLen || (item->flags & 0x200U) != 0) {
        if (srcData != NULL) {
            void *newBuf = sceSsl_lib_0861D934(dataLen);
            void *oldBuf;
            if (newBuf == NULL) {
                return 104;
            }
            oldBuf = item->data;
            item->data = newBuf;
            item->flags &= ~0x200U;
            if (oldBuf != NULL) {
                sceSsl_lib_5E5C873A(oldBuf);
            }
        } else {
            if (itemType != 0) {
                item->itemType = itemType;
            }
            return 0;
        }
    }

    if (srcData != NULL) {
        if (refOnly) {
            item->data = (void *)srcData;
            item->flags |= 0x200U;
        } else {
            sslMemcpy(item->data, srcData, dataLen);
        }
        item->length = dataLen;
    }
    if (itemType != 0) {
        item->itemType = itemType;
    }
    return 0;
}

/* 0x0030EC */
int LIBSSL_NOINLINE sslAsn1DecodeSingleItem(SslAsn1Item *outItem, u32 *consumedOut, u32 itemType,
                                            const u8 *derBuf, u32 derLen, u32 flags)
{
    SslAsn1Element elem;
    int ret;
    u32 totalLen;

    ret = sceSsl_lib_C85F6FCE(&elem, derBuf, derLen);
    if (ret != 0) {
        return ret;
    }

    totalLen = elem.length + (u32)elem.headerLength;
    if (derLen < totalLen) {
        return 2;
    }

    *consumedOut = totalLen;
    if (outItem != NULL) {
        return sslAsn1SetItemData(outItem, itemType, derBuf, totalLen, flags);
    }
    return 0;
}

/* 0x0031A8 */
int LIBSSL_NOINLINE sslAsn1InitItemList(SslAsn1ItemList *list, SslAsn1Item *items, u32 count, u32 capacity)
{
    if (items != NULL) {
        list->items = items;
        list->count = count;
        list->capacity = capacity;
        list->flags = 0;
        return 0;
    }
    list->flags = 2;
    list->count = 0;
    list->items = NULL;
    list->capacity = 0;
    return 2;
}

/* 0x0031D8 */
int LIBSSL_NOINLINE sslAsn1AppendItem(SslAsn1ItemList *list, u32 *outIndex)
{
    if (list->count >= list->capacity) {
        int err = sslAsn1ReserveItemList(list, list->count * 2U + 4U);
        if (err != 0) {
            return err;
        }
    }
    *outIndex = list->count;
    sslMemset(&list->items[list->count], 0, sizeof(SslAsn1Item));
    list->count++;
    return 0;
}

/* 0x003268 */
int LIBSSL_NOINLINE sslAsn1ReserveItemList(SslAsn1ItemList *list, u32 newCap)
{
    SslAsn1Item *newItems;
    u32 byteSize;

    if (list->capacity >= newCap) {
        return 0;
    }
    if ((list->flags & 2U) == 0) {
        return 6;
    }

    byteSize = newCap * sizeof(SslAsn1Item);
    if (list->items != NULL) {
        newItems = (SslAsn1Item *)sslCryptoRealloc(list->items, byteSize);
        if (newItems == NULL) {
            return 5;
        }
        sslMemset(&newItems[list->capacity], 0, (newCap - list->capacity) * sizeof(SslAsn1Item));
    } else {
        newItems = (SslAsn1Item *)sceSsl_lib_0861D934(byteSize);
        if (newItems == NULL) {
            return 5;
        }
        sslMemset(newItems, 0, byteSize);
    }

    list->items = newItems;
    list->capacity = newCap;
    return 0;
}

/* 0x003368 */
void LIBSSL_NOINLINE sslAsn1DestroyItemList(SslAsn1ItemList *list)
{
    u32 idx;

    if (list == NULL) {
        return;
    }
    for (idx = 0; idx < list->capacity; idx++) {
        sslAsn1FreeItem(&list->items[idx]);
    }
    if ((list->flags & 2U) != 0 && list->items != NULL) {
        sceSsl_lib_5E5C873A(list->items);
        list->items = NULL;
    }
    list->count = 0;
    if ((list->flags & 1U) != 0) {
        sceSsl_lib_5E5C873A(list);
    }
}

/* 0x00340C */
void LIBSSL_NOINLINE sslAsn1FreeItem(SslAsn1Item *item)
{
    if (item == NULL) {
        return;
    }
    if (item->data != NULL && (item->flags & 0x200U) == 0) {
        sceSsl_lib_5E5C873A(item->data);
    }
    if ((item->flags & 0x100U) != 0) {
        sceSsl_lib_5E5C873A(item);
    } else {
        sslMemset(item, 0, sizeof(SslAsn1Item));
    }
}
