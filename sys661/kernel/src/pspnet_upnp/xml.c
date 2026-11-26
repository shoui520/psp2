#include "upnp_int.h"

static s32 xmlParserMatchOpenTag(XmlParser *p, s32 *out_matched);
static s32 xmlParserMatchCloseTag(XmlParser *p, s32 *out_matched);

/* =========================================================================
 * Streaming XML parser (0x541c..0x5cc7)
 * ========================================================================= */

s32 xmlParserFeed(XmlParser *p, const char *data, s32 len)
{
	const char *cur = data;
	s32 state = p->state;
	s32 ret = 0;

	while ((s32)(cur - data) < len) {
		switch (state) {
		case 0:
			if ((u8)*cur == '<') {
				p->is_self_closing = 0;
				p->attr_flags = 0;
				p->buf_pos = 0;
				sceKernelMemset(p->tag_name, 0, 0xff);
				sceKernelMemset(p->text_val, 0, 0xff);
				state = 1;
			}
			cur++;
			break;

		case 1:
		case 4: {
			u8 ch = (u8)*cur;
			if (state == 1 && ch == '/' && p->buf_pos == 0) {
				state = 4;
				cur++;
				break;
			}
			if (p->buf_pos < 0xfe &&
			    ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' &&
			    ch != '/' && ch != '<' && ch != '>') {
				if (ch == ':') {
					sceKernelMemset(p->ns_prefix, 0, 0x40);
					sceKernelMemcpy(p->ns_prefix, p->tag_name, p->buf_pos);
					p->buf_pos = 0;
					sceKernelMemset(p->tag_name, 0, 0xff);
					cur++;
					break;
				}
				p->tag_name[p->buf_pos++] = (char)ch;
				cur++;
				break;
			}
			if (ch == '/')
				p->is_self_closing = 1;
			p->tag_name[p->buf_pos] = '\0';
			if (state == 1) {
				s32 matched = 0;
				ret = xmlParserMatchOpenTag(p, &matched);
				if (ret < 0)
					return ret;
				state = matched ? 2 : 0;
			} else if (state == 4) {
				s32 matched = 0;
				ret = xmlParserMatchCloseTag(p, &matched);
				if (ret < 0)
					return ret;
				if (matched == 1)
					p->depth--;
				state = 0;
				cur++;
			} else {
				cur++;
			}
			break;
		}

		case 2: {
			u8 ch = (u8)*cur;
			if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '/') {
				cur++;
			} else if (ch == '>') {
				s32 d = p->depth;
				p->depth = d + 1;
				if (p->is_self_closing == 1) {
					p->attr_flags = 0;
					state = 4;
				} else {
					const XmlTagEntry *ent = &p->schema->levels[d][p->match_idx[d]];
					if (ent->has_text == 1) {
						p->buf_pos = 0;
						sceKernelMemset(p->text_val, 0, 0xff);
						p->entity_state = 0;
						p->entity_len = 0;
						sceKernelMemset(p->entity_buf, 0, 6);
						state = 3;
						cur++;
					} else {
						state = 0;
						cur++;
					}
				}
			} else {
				p->buf_pos = 0;
				p->attr_flags = 0;
				sceKernelMemset(p->tag_name, 0, 0xff);
				sceKernelMemset(p->text_val, 0, 0xff);
				state = 5;
			}
			break;
		}

		case 3:
			if (p->buf_pos < 0xff && (u8)*cur != '<') {
				p->text_val[p->buf_pos++] = *cur++;
			} else {
				p->text_val[p->buf_pos] = '\0';
				p->buf_pos = 0;
				sceKernelMemset(p->tag_name, 0, 0xff);
				p->is_self_closing = 0;
				p->attr_flags = 0;
				state = 1;
				cur++;
			}
			break;

		case 5: {
			if (p->buf_pos < 0xfe) {
				if (p->attr_flags != 1) {
					u8 ch = (u8)*cur;
					if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' &&
					    ch != '/' && ch != '=' && ch != '>') {
						p->tag_name[p->buf_pos++] = (char)ch;
						cur++;
						break;
					}
					p->tag_name[p->buf_pos] = '\0';
					p->attr_flags = 1;
				}
				u8 ch = (u8)*cur;
				if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
					cur++;
					break;
				}
				if (ch == '/') {
					p->is_self_closing = 1;
					cur++;
					break;
				}
				if (ch == '=') {
					p->buf_pos = 0;
					p->attr_flags = 0;
					state = 6;
					cur++;
					break;
				}
			}
			ret = p->cb(p, 4, p->depth, p->match_idx[p->depth], p->tag_name, p->text_val, p->user_arg);
			if (ret < 0)
				return ret;
			state = 2;
			break;
		}

		case 6: {
			if (p->buf_pos < 0xfe) {
				u8 ch = (u8)*cur;
				if ((p->attr_flags & 2) && ch != '"' && ch != '\'') {
					p->text_val[p->buf_pos++] = (char)ch;
					cur++;
					break;
				}
				if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
					if (!(p->attr_flags & 1)) {
						cur++;
						break;
					}
				} else if (ch == '"' || ch == '\'') {
					if (p->attr_flags & 2)
						p->attr_flags &= ~2u;
					else
						p->attr_flags |= 3u;
					cur++;
					break;
				} else if (ch != '/' && ch != '>') {
					p->attr_flags |= 1u;
					p->text_val[p->buf_pos++] = (char)ch;
					cur++;
					break;
				} else if (ch == '/') {
					p->is_self_closing = 1;
				}
			}
			p->text_val[p->buf_pos] = '\0';
			ret = p->cb(p, 4, p->depth, p->match_idx[p->depth], p->tag_name, p->text_val, p->user_arg);
			if (ret < 0)
				return ret;
			state = 2;
			break;
		}

		default:
			p->state = state;
			return ret;
		}
	}

	p->state = state;
	return 0;
}

s32 xmlParserInit(XmlParser *p, const XmlSchema *schema, XmlCallback cb, void *arg)
{
	sceKernelMemset(p, 0, sizeof(XmlParser));
	p->schema = schema;
	p->cb = cb;
	p->user_arg = arg;
	return 0;
}

static s32 xmlParserMatchOpenTag(XmlParser *p, s32 *out_matched)
{
	const XmlSchema *schema = p->schema;
	s32 depth = p->depth;

	if ((u32)depth >= schema->num_levels) {
		*out_matched = 0;
		return 0;
	}

	const XmlTagEntry *level = schema->levels[depth];
	s32 idx = 0;
	while (sceNetStrlen(level[idx].name) != 0) {
		if (sceNetStrcasecmp(level[idx].name, p->tag_name) == 0)
			break;
		idx++;
	}

	if (sceNetStrlen(level[idx].name) == 0) {
		*out_matched = 0;
		return 0;
	}

	*out_matched = 1;
	p->match_idx[depth] = idx;
	return p->cb(p, 1, depth, idx, NULL, NULL, p->user_arg);
}

static s32 xmlParserMatchCloseTag(XmlParser *p, s32 *out_matched)
{
	s32 depth = p->depth;
	if (depth == 0) {
		*out_matched = 0;
		return 0;
	}

	const XmlSchema *schema = p->schema;
	if ((u32)depth > schema->num_levels) {
		*out_matched = 0;
		return 0;
	}

	const XmlTagEntry *level = schema->levels[depth - 1];
	s32 idx = p->match_idx[depth - 1];
	if (p->is_self_closing != 1) {
		if (sceNetStrcasecmp(level[idx].name, p->tag_name) != 0) {
			*out_matched = 0;
			return 0;
		}
	}

	*out_matched = 1;
	if (level[idx].has_text == 1)
		return p->cb(p, 3, depth - 1, idx, p->text_val, NULL, p->user_arg);
	return p->cb(p, 2, depth - 1, idx, NULL, NULL, p->user_arg);
}

