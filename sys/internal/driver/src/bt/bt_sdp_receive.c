/* SDP packet receiver; original source filename unavailable. */
#include <string.h>
#include "bt_internal.h"

extern const uint8_t gBtSdpBaseUuidSuffix[12];
extern const uint8_t gBtSdpServiceRecords[696];

static inline uint32_t BtSdpReadUuid(const uint8_t *data, int length)
{
	if (length == 2)
		return BtReadBe16(data);
	if (length == 4)
		return BtReadBe32(data);
	if (length == 16 && !memcmp(gBtSdpBaseUuidSuffix, data + 4, 12))
		return BtReadBe32(data);
	return 0;
}

void BtReceiveSdp(BtConnection *connection, BtChannel *channel,
		  uint8_t *packet, int length)
{
	BtStack *stack = connection->stack;
	BtSdpPacket *request = (BtSdpPacket *)packet;
	uint32_t uuids[16];
	uint16_t lower[16], upper[16];
	int pduId, transactionId, parameterLength;
	int uuidCount = 0, rangeCount = 0, handle = 0;
	int maximum, position, count, written, error = 4;
	uint8_t *cursor, *end, *output;
	BtSdpContinuation *continuation;

	if (length <= 4) {
		transactionId = 0;
		if (length > 2)
			BtUnpack(request->header.transactionIdBe, 2, "H",
				 &transactionId);
		goto fail;
	}
	BtUnpack(packet, sizeof(request->header), "1HH", &pduId,
		 &transactionId, &parameterLength);
	if (length <= parameterLength + 4)
		goto fail;
	memset(uuids, 255, sizeof(uuids));
	memset(upper, 255, sizeof(upper));
	memset(lower, 255, sizeof(lower));
	end = request->parameters + parameterLength;
	switch (pduId) {
	case 2: {
		BtSdpSearchResponse *response = (BtSdpSearchResponse *)stack->scratch;
		int total;

		cursor = BtReadSdpUuids(request->parameters, end, uuids, &uuidCount);
		if (!cursor || end - cursor <= 2)
			goto malformed;
		maximum = BtReadBe16(cursor);
		total = BtFindSdpServiceRecords(uuids, uuidCount,
					     response->handlesAndContinuation);
		cursor += 2;
		continuation = (BtSdpContinuation *)cursor;
		if (end - cursor <= 0 || end - cursor != continuation->length + 1)
			goto invalid_continuation;
		if (continuation->length == 1)
			position = continuation->data[0];
		else if (!continuation->length)
			position = 0;
		else if (continuation->length == 2) {
			BtUnpack(continuation->data, 2, "H", &position);
			if (position < 0)
				goto invalid_continuation;
		} else
			goto invalid_continuation;
		if (position > 0 && total <= position)
			goto invalid_continuation;
		output = response->handlesAndContinuation + 4 * position;
		count = total - position;
		written = 4 * count;
		if (written + 9 >= channel->mtu) {
			int limit = (channel->mtu - 11) >> 2;

			if (limit < maximum) {
				maximum = limit;
				if (limit < 0)
					maximum = 0;
			}
		}
		if (count > maximum) {
			written = 4 * maximum;
			continuation = (BtSdpContinuation *)(output + written);
			continuation->length = 1;
			continuation->data[0] = position + maximum;
			count = maximum;
		} else {
			continuation = (BtSdpContinuation *)(output + written);
			continuation->length = 0;
		}
		output -= sizeof(*response);
		BtPack(output, sizeof(*response), "HH", total, count);
		count = written + continuation->length + 5;
		BtSendL2capPacket(connection, channel->remoteCid, "1HH #", 3,
				 (uint16_t)transactionId, count, output, count);
		return;
	}
	case 3:
	case 5:
		return;
	case 4:
	case 6: {
		BtSdpAttributeResponse *response =
			(BtSdpAttributeResponse *)stack->scratch;
		uint8_t handles[128];
		const uint8_t *record = NULL;
		int recordCount = 0;

		if (pduId == 4) {
			BtUnpack(request->parameters, 6, "WH", &handle, &maximum);
			for (record = gBtSdpServiceRecords;
			     record < gBtSdpServiceRecords + sizeof(gBtSdpServiceRecords);
			     record += *record) {
				const uint8_t *attribute = record + 1;
				const uint8_t *recordEnd = record + *record;

				while (attribute < recordEnd) {
					if (*attribute == 8 && attribute[1] == 0 &&
					    attribute[2] == 0 && attribute[3] == 10 &&
					    handle == BtReadBe32(attribute + 4))
						goto found_record;
					attribute += *attribute;
				}
			}
			error = 2;
			goto fail;
		found_record:
			cursor = BtReadSdpAttributeRanges(request->parameters + 6,
						 end, lower, upper, &rangeCount);
		} else {
			cursor = BtReadSdpUuids(request->parameters, end, uuids,
					      &uuidCount);
			if (!cursor || end - cursor <= 4)
				goto malformed;
			maximum = BtReadBe16(cursor);
			recordCount = BtFindSdpServiceRecords(uuids, uuidCount, handles);
			cursor = BtReadSdpAttributeRanges(cursor + 2, end,
						 lower, upper, &rangeCount);
		}
		if (!cursor)
			goto malformed;
		continuation = (BtSdpContinuation *)cursor;
		if (end - cursor <= 0 || end - cursor != continuation->length + 1)
			goto invalid_continuation;
		if (continuation->length == 1)
			position = continuation->data[0];
		else if (!continuation->length)
			position = 0;
		else if (continuation->length == 2) {
			BtUnpack(continuation->data, 2, "H", &position);
			if (position < 0)
				goto invalid_continuation;
		} else
			goto invalid_continuation;
		output = response->attributesAndContinuation + 3;
		written = 0;
		for (int index = 0; index < (pduId == 4 ? 1 : recordCount); index++) {
			uint8_t *sequence = output;
			const uint8_t *attribute, *recordEnd;

			if (pduId == 6) {
				handle = BtReadBe32(handles + index * 4);
				written += 3;
				output += 3;
				for (record = gBtSdpServiceRecords;
				     record < gBtSdpServiceRecords + sizeof(gBtSdpServiceRecords);
				     record += *record) {
					attribute = record + 1;
					recordEnd = record + *record;
					while (attribute < recordEnd) {
						if (*attribute == 8 && attribute[1] == 0 &&
						    attribute[2] == 0 && attribute[3] == 10 &&
						    handle == BtReadBe32(attribute + 4))
							goto matched_record;
						attribute += *attribute;
					}
				}
				goto finish_record;
			}
		matched_record:
			for (int range = 0; range < rangeCount; range++) {
				attribute = record + 1;
				recordEnd = record + *record;
				while (attribute < recordEnd) {
					unsigned int attributeId = attribute[2] | attribute[1] << 8;

					if (attributeId >= lower[range] && attributeId <= upper[range]) {
						uint8_t size = *attribute - 1;

						written += BtPack(output, 4096 - written, "1#",
								  9, attribute + 1, size);
						output += size + 1;
					}
					attribute += *attribute;
				}
			}
		finish_record:
			if (pduId == 6)
				BtPack(sequence, 3, "1H", 54, output - sequence - 3);
		}
		if (position > written + 2)
			goto invalid_continuation;
		BtPack(response->attributesAndContinuation, 3, "1H", 54, written);
		count = written + 3 - position;
		if (count + 7 >= channel->mtu) {
			int limit = channel->mtu - 9;

			if (position + limit > 255)
				limit = channel->mtu - 10;
			if (limit < maximum)
				maximum = limit;
			if (maximum < 0)
				maximum = 0;
		}
		if (count > maximum) {
			int nextPosition = position + maximum;

			count = maximum;
			continuation = (BtSdpContinuation *)(response->attributesAndContinuation +
							 position + count);
			if (nextPosition > 255) {
				continuation->length = 2;
				BtPack(continuation->data, 2, "H", nextPosition);
			} else {
				continuation->length = 1;
				continuation->data[0] = nextPosition;
			}
		} else {
			continuation = (BtSdpContinuation *)output;
			continuation->length = 0;
		}
		output = stack->scratch + position;
		BtPack(output, 2, "H", count);
		count += continuation->length + 3;
		BtSendL2capPacket(connection, channel->remoteCid, "1HH #",
				 (uint8_t)(pduId + 1), (uint16_t)transactionId,
				 count, output, count);
		return;
	}
	case 7: {
		BtSdpReceiveBuffer *receive = &stack->sdpReceive;
		int bytes = BtReadBe16(request->parameters);
		int responseLength = bytes;
		int continuationLength;
		uint8_t type = 0;
		BtDevice *device;

		if (bytes >= parameterLength)
			return;
		continuationLength = request->parameters[bytes + 2];
		if (parameterLength != bytes + 3 + continuationLength)
			return;
		if ((intptr_t)request->parameters > 0 && parameterLength > 0) {
			int elementLength = parameterLength;

			bytes = BtReadBe16(request->parameters);
			if (bytes >= parameterLength - 2)
				bytes = parameterLength - 3;
			if (receive->length + bytes < 2048) {
				memcpy(receive->data + receive->length,
				       request->parameters + 2, bytes);
				receive->length += bytes;
			}
			if (request->parameters[bytes + 2])
				goto next_fragment;
			elementLength = receive->length;
			device = connection->device;
			if (elementLength > 0) {
				cursor = BtReadSdpElement(receive->data, &elementLength, &type);
				if (cursor && type == 0x30) {
					uint8_t *recordsEnd = cursor + elementLength;

					memcpy(stack->scratch, device, 768);
					device->eirIdentity.fields.vendorIdSource = 0;
					device->profiles = 0;
					device->flags &= ~0xc3u;
					device->primary = 0;
					device->version = 0;
					device->productId = 0;
					device->vendorId = 0;
					device->serialPortChannel = 0;
					device->imagingChannel = 0;
					device->objectPushChannel = 0;
					device->headsetAgChannel = 0;
					device->headsetChannel = 0;
					device->handsfreeAgChannel = 0;
					device->eirIdentity.fields.handsfreeChannel = 0;
					device->avrcpFeatures = 0;
					device->avrcpVersion = 0;
					device->hidDescriptorLength = 0;
					while (cursor < recordsEnd) {
						int recordLength = recordsEnd - cursor;
						uint8_t *record = BtReadSdpElement(cursor, &recordLength, &type);
						uint8_t *attribute;
						int remaining;

						if (!record)
							break;
						connection->currentServiceClass = 0;
						if (type != 0x30)
							goto next_record;
						attribute = record;
						remaining = recordLength;
						while (remaining > 2 && attribute[0] == 9) {
							int attributeId = BtReadBe16(attribute + 1);
							uint8_t *value = BtReadSdpElement(attribute + 3,
										 &remaining, &type);
							int valueLength = remaining;
							uint32_t service = connection->currentServiceClass;

							if (!value)
								break;
							if (attributeId == 1 || attributeId == 4 || attributeId == 9) {
								uint8_t nestedType = type;
								uint8_t *nested = value;
								int nestedRemaining = valueLength;

								if (type != 0x30)
									goto next_attribute;
								while (nestedRemaining > 0) {
									int nestedLength = nestedRemaining;
									uint8_t *item = BtReadSdpElement(nested,
												 &nestedLength, &nestedType);

									if (!item)
										break;
									if (attributeId == 1 && nestedType == 0x18) {
										uint32_t uuid = BtSdpReadUuid(item, nestedLength);
										uint32_t profile = 0;

										switch (uuid) {
										case 0x1112: profile = 0x20000; break;
										case 0x110a: profile = 4; break;
										case 0x110c: profile = 0x20; break;
										case 0x110e: profile = 0x10; break;
										case 0x110b: profile = 8; break;
										case 0x1105: profile = 0x8000; break;
										case 0x1108: profile = 0x10000; break;
										case 0x1101: profile = 0x80000; break;
										case 0x111b: profile = 0x2000; break;
										case 0x1116: profile = 0x200; break;
										case 0x1117: profile = 0x400; break;
										case 0x1115: profile = 0x100; break;
										case 0x111f: profile = 2; break;
										case 0x111e: profile = 1; break;
										case 0x1124: profile = 0x40; break;
										case 0x1200: break;
										default: goto next_nested;
										}
										device->profiles |= profile;
										connection->currentServiceClass = uuid;
									} else if (attributeId != 1 && nestedType == 0x30) {
										uint8_t *part = item;
										int partRemaining = nestedLength;
										uint32_t uuid = 0;

										while (partRemaining > 0) {
											int partLength = partRemaining;
											uint8_t partType;
											uint8_t *data = BtReadSdpElement(part,
													 &partLength, &partType);

											if (!data)
												break;
											if (partType == 0x18)
												uuid = BtSdpReadUuid(data, partLength);
											else if (attributeId == 4 && partType == 8 &&
												 partLength == 1 && uuid == 3) {
												uint8_t channelId = 2 * *data;

												switch (service) {
												case 0x1101: device->serialPortChannel = channelId; break;
												case 0x1105: device->objectPushChannel = channelId; break;
												case 0x1108: device->headsetChannel = channelId; break;
												case 0x1112: device->headsetAgChannel = channelId; break;
												case 0x111b: device->imagingChannel = channelId; break;
												case 0x111e: device->eirIdentity.fields.handsfreeChannel = channelId; break;
												case 0x111f: device->handsfreeAgChannel = channelId; break;
												}
											} else if (attributeId == 9 && partType == 8 &&
												   partLength == 2 && uuid == 0x110e &&
												   service == 0x110c)
												device->avrcpVersion = BtReadBe16(data);
											partRemaining = part + partRemaining - (data + partLength);
											part = data + partLength;
										}
									}
								next_nested:
									nestedRemaining = nested + nestedRemaining - (item + nestedLength);
									nested = item + nestedLength;
								}
							} else if (attributeId == 0x311 && type == 8 &&
								   valueLength == 2 && service == 0x110c)
								device->avrcpFeatures = BtReadBe16(value);
							else if (service == 0x1200 && type == 8 && valueLength == 2) {
								uint16_t identity = BtReadBe16(value);

								switch (attributeId) {
								case 0x201: device->vendorId = identity; break;
								case 0x202: device->productId = identity; break;
								case 0x203: device->version = identity; break;
								case 0x205: device->eirIdentity.fields.vendorIdSource = identity; break;
								}
							} else if (service == 0x1124) {
								if (attributeId == 0x202 && type == 8 && valueLength == 1) {
									if (*value & 0x80)
										device->flags |= 0x80;
									if (*value & 0x40)
										device->flags |= 0x40;
								} else if (attributeId == 0x203 && type == 8 && valueLength == 1)
										device->primary = *value;
								else if (attributeId == 0x204 && type == 0x28 && valueLength == 1 && *value)
									device->flags |= 2;
								else if (attributeId == 0x206 && type == 0x30) {
									int descriptorLength = valueLength;
									uint8_t descriptorType = type;
									uint8_t *descriptor = BtReadSdpElement(value,
												 &descriptorLength, &descriptorType);

									if (descriptor && (descriptorLength -= 2) >= 0 &&
									    descriptor[0] == 8 && descriptor[1] == 34) {
										descriptor = BtReadSdpElement(descriptor + 2,
											 &descriptorLength, &descriptorType);
										if (descriptor && descriptorType == 0x20)
											BtSetDeviceHidDescriptor(connection, descriptor,
													 (uint16_t)descriptorLength);
									}
								} else if (attributeId == 0x20d && type == 0x28 && valueLength == 1 && *value)
									device->flags |= 1;
							}
						next_attribute:
							attribute = value + valueLength;
							remaining = record + recordLength - attribute;
						}
					next_record:
						cursor = record + recordLength;
					}
					if (device->profiles & 0x10000)
						device->profiles |= 0x20000000;
					else
						device->profiles &= ~0x20000000u;
				}
			}
			if (device->vendorProductId == 0x09cc054c)
				device->productId = 1476;
			receive->length = 0;
			if (memcmp(stack->scratch, device, 768)) {
				device->generation++;
				BtFlushConnectionDevice(connection);
			}
		}
	next_fragment:
		if (continuationLength) {
			connection->sdpTransactionId++;
			BtSendL2capPacket(connection, channel->remoteCid,
					 "1HH H1H H H1W #", 6,
					 connection->sdpTransactionId, continuationLength + 15,
					 0x3503, 25, 0x100, 0x800, 0x3505, 10, 0xffff,
					 request->parameters + responseLength + 2,
					 continuationLength + 1);
		} else
			BtSendL2capDisconnectRequest(connection, channel);
		return;
	}
	default:
		return;
	}
malformed:
	error = 3;
	goto fail;
invalid_continuation:
	error = 5;
fail:
	BtSendL2capPacket(connection, channel->remoteCid, "1HH H", 1,
			 (uint16_t)transactionId, 2, error);
}
