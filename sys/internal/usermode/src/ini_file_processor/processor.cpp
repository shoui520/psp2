/*
 * SceIniFileProcessor: the processor. It reads an INI file into entries (a key and its value), answers for
 * them, takes changes, and writes the file again through a temporary file.
 */

#include "ini_internal.h"

static const char g_modeRead[] = "r";		/* 0x81004158 */
static const char g_modeWrite[] = "rw";		/* 0x8100415c */
static const char g_equals[] = "=";		/* 0x81004160 */
static const char g_lineEnd[] = "\r\n";		/* 0x81004164 */
static const char g_bom[] = "\xef\xbb\xbf";	/* 0x810041e8 */

/* a key is fine when it ends within 128 characters, does not start with a space, a tab, ';' or '[', and has no
 * '=' or line end in it */
__attribute__((noinline)) int iniKeyValid(const char *key)
{
	char c = *key;
	SceSize i;

	if (c == ';' || c == '[' || c == ' ' || *key == 0 || c == '\t')
		return 0;
	i = 0;
	do {
		c = *key;
		if (c == '=' || c == '\n' || c == '\r')
			break;
		if (*key == 0)
			return 1;
		i++;
		key++;
	} while (i < 0x80);
	return 0;
}

/* the allocator's functions and context, or the library's own */
static void iniAllocatorOf(IniAllocatorRef *allocator, IniAllocatorOps **ops, void **context)
{
	if (allocator != NULL) {
		*ops = allocator->ops;
		*context = allocator->context;
	} else {
		*ops = iniDefaultAllocator();
		*context = iniDefaultAllocator();
	}
}

IniProcessor::IniProcessor(IniAllocatorRef *allocator, const IniConfig *config)
{
	IniAllocatorOps *ops;
	void *context;
	void *memory;

	m_config.poolSize = 0;
	m_config.reserved = 0;
	m_config.context = NULL;
	m_config.ops = NULL;
	m_file = NULL;
	m_temp = NULL;
	iniAllocatorOf(allocator, &ops, &context);
	m_head.key.data = g_iniEmptyString;
	m_head.key.length = 0;
	m_head.key.ops = ops;
	m_head.key.context = context;
	iniAllocatorOf(allocator, &ops, &context);
	m_head.value.data = g_iniEmptyString;
	m_head.value.length = 0;
	m_head.value.ops = ops;
	m_head.value.context = context;
	m_head.next = &m_head;
	m_head.prev = &m_head;
	m_count = 0;
	m_entryAllocator = allocator;
	m_current = NULL;
	m_mode = INI_MODE_UNOPENED;
	m_allocator = allocator;
	m_text = NULL;
	m_textLength = 0;
	m_dirty = false;
	m_textValid = false;
	m_config.poolSize = config->poolSize;
	m_config.reserved = config->reserved;
	m_config.ops = config->ops;
	m_config.context = config->context;

	memory = iniAlloc(allocator, sizeof(IniFile));
	m_file = memory != NULL ? new (memory) IniFile(m_allocator) : NULL;
	if (m_config.reserved != 0) {
		memory = iniAlloc(allocator, sizeof(IniBufferedFile));
		m_temp = memory != NULL ? new (memory) IniBufferedFile(m_allocator, m_config.reserved) : NULL;
	} else {
		memory = iniAlloc(allocator, sizeof(IniTempFile));
		m_temp = memory != NULL ? new (memory) IniTempFile(m_allocator) : NULL;
	}
}

int IniProcessor::setConfig(const IniConfig *config)
{
	SceSize size = config->poolSize;

	if (size != 0 && (size < 0x80 || size > 0x6400000))
		return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	m_config.poolSize = size;
	m_config.context = config->context;
	m_config.reserved = config->reserved;
	m_config.ops = config->ops;
	m_entryAllocator = m_allocator;
	return 0;
}

/* gives back every entry and the text made of them */
int IniProcessor::clear()
{
	IniAllocatorRef *allocator = m_allocator;

	do {
		IniEntry *entry = m_head.prev;

		if (entry == &m_head)
			break;
		entry->prev->next = entry->next;
		entry->next->prev = entry->prev;
		if (entry->value.data != g_iniEmptyString)
			entry->value.ops->free(entry->value.data, entry->value.context);
		if (entry->key.data != g_iniEmptyString)
			entry->key.ops->free(entry->key.data, entry->key.context);
		iniFree(m_entryAllocator, entry);
		--m_count;
	} while (m_count != 0);
	if (m_text != NULL)
		iniFree(allocator, m_text);
	m_textValid = false;
	m_text = NULL;
	m_textLength = 0;
	return 0;
}

void IniProcessor::removeAll()
{
	do {
		IniEntry *entry = m_head.prev;

		if (entry == &m_head)
			break;
		entry->prev->next = entry->next;
		entry->next->prev = entry->prev;
		if (entry->value.data != g_iniEmptyString)
			entry->value.ops->free(entry->value.data, entry->value.context);
		if (entry->key.data != g_iniEmptyString)
			entry->key.ops->free(entry->key.data, entry->key.context);
		iniFree(m_entryAllocator, entry);
		--m_count;
	} while (m_count != 0);
}

/* puts an entry with a copy of the key and of the value at the end of the ring */
void IniProcessor::addEntry(const IniString *key, const IniString *value)
{
	IniAllocatorOps *ops;
	void *context;
	IniEntry *entry = (IniEntry *)iniAlloc(m_entryAllocator, sizeof(IniEntry));

	if (entry == NULL)
		return;
	iniAllocatorOf(m_entryAllocator, &ops, &context);
	entry->key.data = g_iniEmptyString;
	entry->key.length = 0;
	entry->key.ops = ops;
	entry->key.context = context;
	iniAllocatorOf(m_entryAllocator, &ops, &context);
	entry->value.data = g_iniEmptyString;
	entry->value.length = 0;
	entry->value.ops = ops;
	entry->value.context = context;
	entry->prev = entry;
	entry->next = entry;
	iniStringAssign(&entry->key, key);
	iniStringAssign(&entry->value, value);
	entry->next = &m_head;
	entry->prev = m_head.prev;
	m_head.prev->next = entry;
	m_head.prev = entry;
	++m_count;
}

/* the entry of a key (the head when there is none); the entry found becomes the current one */
IniEntry *IniProcessor::find(const char *key, const IniString *keyString)
{
	IniEntry *entry;

	if (m_count == 0)
		return &m_head;
	for (entry = m_head.next; entry != &m_head; entry = entry->next) {
		if (*entry->key.data == *keyString->data && sceClibStrcmp(entry->key.data, keyString->data) == 0) {
			m_current = entry;
			return entry;
		}
	}
	return &m_head;
}

/* reads the file: its lines of the form key=value become entries */
int IniProcessor::load()
{
	IniInput input;
	IniToken token;
	IniAllocatorRef *allocator = m_allocator;
	char *buffer = NULL;
	SceSSize got;
	SceOff size;
	SceSize readSize;
	SceSize bufferSize;
	int result;

	removeAll();
	m_current = &m_head;
	m_reserved = 0;
	size = m_file->size();
	if (size == 0) {
		result = SCE_INI_FILE_PROCESSOR_ERROR_EMPTY;
		goto done;
	}
	readSize = size > 3 ? (SceSize)size : 3;
	bufferSize = size > 4 ? (SceSize)size + 1 : 4;
	buffer = (char *)iniAlloc(allocator, bufferSize);
	if (buffer == NULL) {
		result = SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY;
		goto done;
	}
	iniInputInit(&input);
	input.data = buffer;
	input.size = bufferSize;
	input.checkBom = true;
	iniTokenReset(&token);
	*buffer = 0;
	result = m_file->read(buffer, readSize, &got);
	input.size = got;
	if (result == INI_RESULT_STREAM_END) {
		result = 0;
		goto done;
	}
	if (result == 0)
		result = scanBuffer(&input, (SceSize)size);
done:
	iniFree(m_allocator, buffer);
	return result;
}

/* opens a file to read ("r") or to read and change ("rw") and reads its entries */
int IniProcessor::open(const char *path, const char *mode, int flags)
{
	int fileMode = 2;
	int result;

	if (m_file == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	m_mode = 0;
	if (mode != NULL) {
		if (sceClibStrcmp(mode, g_modeRead) != 0 && sceClibStrcmp(mode, g_modeWrite) != 0)
			return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
		if (sceClibStrchr(mode, 'w') != NULL) {
			fileMode = 3;
			m_mode |= INI_MODE_WRITABLE;
		}
	}
	result = m_file->open(path, fileMode, flags);
	if (result != 0) {
		if (result != SCE_INI_FILE_PROCESSOR_ERROR_NO_FILE_NAME) {
			if (result != -2138832885)
				result = -2138832886;
			m_mode &= ~INI_MODE_WRITABLE;
		}
	} else {
		result = load();
		if ((result != SCE_INI_FILE_PROCESSOR_ERROR_EMPTY || (m_mode & INI_MODE_WRITABLE) != 0) &&
		    (result == 0 || result == SCE_INI_FILE_PROCESSOR_ERROR_EMPTY))
			return 0;
	}
	return result;
}

/* makes a file (mode 7: also an existing one is made empty) */
int IniProcessor::create(const char *path)
{
	int result;

	if (m_file == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	m_mode = 0;
	result = m_file->open(path, 7, 0);
	if (result != 0)
		return -2138832885;
	m_mode |= INI_MODE_WRITABLE;
	return result;
}

int IniProcessor::rewind()
{
	int result;

	if (m_file == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	result = m_file->rewind();
	m_current = m_head.next;
	return result;
}

int IniProcessor::unused14()
{
	return 1;
}

/* writes the entries to the temporary file (a byte order mark, then a line "key=value" for each) and puts it in
 * the place of the real file */
int IniProcessor::save()
{
	IniFileBase *file = m_file;
	IniFileBase *temp = m_temp;
	int result;

	if (m_mode == INI_MODE_UNOPENED)
		return SCE_INI_FILE_PROCESSOR_ERROR_TOO_MANY;
	if (file == NULL || temp == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	result = temp->open(file->path()->data, 0, file->m_flags | 2);
	if (result != 0)
		return result;
	result = temp->write(g_bom, 3);
	if (result != 0)
		return result;
	for (IniEntry *entry = m_head.next; entry != &m_head; entry = entry->next) {
		result = temp->write(entry->key.data, entry->key.length);
		if (result != 0)
			return result;
		result = temp->write(g_equals, 1);
		if (result != 0)
			return result;
		if (entry->value.length != 0) {
			result = temp->write(entry->value.data, entry->value.length);
			if (result != 0)
				return result;
		}
		result = temp->write(g_lineEnd, 2);
		if (result != 0)
			return result;
	}
	result = temp->sync();
	if (result == 0) {
		result = file->replace(temp);
		if (result == 0)
			m_dirty = false;
	}
	return result;
}

int IniProcessor::flush()
{
	if (m_file == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	if (m_dirty)
		save();
	return m_file->sync();
}

int IniProcessor::close()
{
	int saved = 0;
	int closed;

	if (m_file == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	m_mode &= ~INI_MODE_WRITABLE;
	if (m_dirty)
		saved = save();
	closed = m_file->close();
	if (saved == 0 && closed != 0)
		return closed;
	return saved;
}

/* the value of a key, from an offset, into the buffer (cut to it, with an error when it did not fit) */
int IniProcessor::get(const char *key, char *buf, SceSize size, SceSize offset)
{
	IniAllocatorOps *ops;
	void *context;
	IniString name;
	IniEntry *entry;
	int result = 0;

	if (key == NULL || buf == NULL || size == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	if (iniKeyValid(key) == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_BAD_KEY;
	iniAllocatorOf(m_allocator, &ops, &context);
	name.data = g_iniEmptyString;
	name.length = 0;
	name.ops = ops;
	name.context = context;
	iniStringAssignChars(&name, key, sceClibStrnlen(key, 0x100000));
	entry = find(key, &name);
	if (entry != &m_head) {
		SceSize length = entry->value.length;

		if (offset != 0) {
			if (offset > length) {
				result = SCE_INI_FILE_PROCESSOR_ERROR_NO_OFFSET;
			} else {
				SceSize rest = length - offset;
				const char *source = entry->value.data + offset;

				if (rest >= size)
					result = SCE_INI_FILE_PROCESSOR_ERROR_BUFFER_TOO_SMALL;
				if (buf != NULL && source != NULL && size != 0) {
					SceSize n = rest;

					if (rest == (SceSize)-2)
						n = sceClibStrnlen(source, size);
					if (n >= size)
						n = size - 1;
					sceClibMemcpy_safe(buf, source, n);
					buf[n] = 0;
				}
			}
		} else {
			const char *source = entry->value.data;

			if (length >= size)
				result = SCE_INI_FILE_PROCESSOR_ERROR_BUFFER_TOO_SMALL;
			if (buf != NULL && source != NULL && size != 0) {
				SceSize n = sceClibStrnlen(source, size);

				if (n >= size)
					n = size - 1;
				sceClibMemcpy_safe(buf, source, n);
				buf[n] = 0;
			}
		}
	} else {
		result = SCE_INI_FILE_PROCESSOR_ERROR_NO_SUCH_KEY;
	}
	iniStringDestroy(&name);
	return result;
}

/* the key and value of the current entry; goes on to the next one */
int IniProcessor::next(char *key, char *value, SceSize size)
{
	IniEntry *entry;
	const char *source;
	int result = 0;

	if (key != NULL && value != NULL && size != 0) {
		if (m_count == 0) {
			*key = 0;
			*value = 0;
			return SCE_INI_FILE_PROCESSOR_ERROR_EMPTY;
		}
		entry = m_current;
		if (entry == &m_head)
			return INI_RESULT_STREAM_END;
		if (entry->key.length != 0) {
			source = entry->key.data;
			if (source != NULL) {
				SceSize n = sceClibStrnlen(source, 0x80);

				if (n >= 0x80)
					n = 127;
				sceClibMemcpy_safe(key, source, n);
				key[n] = 0;
				entry = m_current;
			}
			if (entry->value.length != 0) {
				if (size < entry->value.length)
					result = SCE_INI_FILE_PROCESSOR_ERROR_BUFFER_TOO_SMALL;
				source = entry->value.data;
				if (source != NULL) {
					SceSize n = sceClibStrnlen(source, size);

					if (n >= size)
						n = size - 1;
					sceClibMemcpy_safe(value, source, n);
					value[n] = 0;
					entry = m_current;
				}
			} else {
				*value = 0;
				entry = m_current;
			}
		}
	} else {
		result = SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
		entry = m_current;
	}
	m_current = entry->next;
	return result;
}

/* gives a key a value (makes the key when it is new) */
int IniProcessor::set(const char *key, const char *value)
{
	IniAllocatorOps *ops;
	void *context;
	IniString name;
	IniEntry *entry;
	int result = 0;

	if (m_mode != INI_MODE_UNOPENED && (m_mode & INI_MODE_WRITABLE) == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_WRITABLE;
	if (key == NULL || value == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	if (iniKeyValid(key) == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_BAD_KEY;
	iniAllocatorOf(m_allocator, &ops, &context);
	name.data = g_iniEmptyString;
	name.length = 0;
	name.ops = ops;
	name.context = context;
	iniStringAssignChars(&name, key, sceClibStrnlen(key, 0x100000));
	entry = find(key, &name);
	if (entry == &m_head) {
		result = add(key, value);
	} else {
		SceSize length = 0;

		if (value != NULL)
			length = sceClibStrnlen(value, 0x100000);
		iniStringAssignChars(&entry->value, value, length);
		m_dirty = true;
	}
	iniStringDestroy(&name);
	return result;
}

/* makes an entry */
int IniProcessor::add(const char *key, const char *value)
{
	IniAllocatorOps *ops;
	void *context;
	IniString name;
	IniString text;

	if (m_mode != INI_MODE_UNOPENED && (m_mode & INI_MODE_WRITABLE) == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_WRITABLE;
	if (key == NULL || value == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	if (iniKeyValid(key) == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_BAD_KEY;
	iniAllocatorOf(m_allocator, &ops, &context);
	name.data = g_iniEmptyString;
	name.length = 0;
	name.ops = ops;
	name.context = context;
	iniStringAssignChars(&name, key, sceClibStrnlen(key, 0x100000));
	iniAllocatorOf(m_allocator, &ops, &context);
	text.data = g_iniEmptyString;
	text.length = 0;
	text.ops = ops;
	text.context = context;
	iniStringAssignChars(&text, value, sceClibStrnlen(value, 0x100000));
	addEntry(&name, &text);
	m_current = m_head.prev;
	m_dirty = true;
	iniStringDestroy(&text);
	iniStringDestroy(&name);
	return 0;
}

/* takes an entry away */
int IniProcessor::remove(const char *key)
{
	IniAllocatorOps *ops;
	void *context;
	IniString name;
	IniEntry *entry;
	int result = 0;

	if ((m_mode & INI_MODE_WRITABLE) == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_WRITABLE;
	if (key == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	if (iniKeyValid(key) == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_BAD_KEY;
	iniAllocatorOf(m_allocator, &ops, &context);
	name.data = g_iniEmptyString;
	name.length = 0;
	name.ops = ops;
	name.context = context;
	iniStringAssignChars(&name, key, sceClibStrnlen(key, 0x100000));
	entry = find(key, &name);
	if (entry == &m_head) {
		result = SCE_INI_FILE_PROCESSOR_ERROR_NO_SUCH_KEY;
	} else {
		IniEntry *prev = entry->prev;
		IniEntry *following = entry->next;

		following->prev = prev;
		prev->next = following;
		if (entry->value.data != g_iniEmptyString)
			entry->value.ops->free(entry->value.data, entry->value.context);
		if (entry->key.data != g_iniEmptyString)
			entry->key.ops->free(entry->key.data, entry->key.context);
		iniFree(m_entryAllocator, entry);
		m_current = following;
		--m_count;
		m_dirty = true;
	}
	iniStringDestroy(&name);
	return result;
}

/* the last error of the file, or else of the temporary file */
int IniProcessor::lastError()
{
	int result = m_file->errorCode();

	if (result == 0)
		return m_temp->errorCode();
	return result;
}

SceSize IniProcessor::count()
{
	return m_count;
}

/* the size of the text of the entries, and when a buffer is given the text itself */
int IniProcessor::measure(char *buf, int *length)
{
	int result = 0;
	SceSize size = 0;

	if (length == NULL) {
		result = SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	} else {
		size = 3;
		if (buf != NULL) {
			sceClibMemcpy_safe(buf, g_bom, 3);
			size = 3;
		}
		if (m_count != 0) {
			for (IniEntry *entry = m_head.next; entry != &m_head; entry = entry->next) {
				SceSize keyLength = entry->key.length;
				SceSize valueLength;

				if (buf != NULL)
					sceClibMemcpy_safe(buf + size, entry->key.data, keyLength);
				size += keyLength;
				if (buf != NULL)
					buf[size] = '=';
				size++;
				valueLength = entry->value.length;
				if (valueLength != 0) {
					if (buf != NULL)
						sceClibMemcpy_safe(buf + size, entry->value.data, valueLength);
					size += valueLength;
				}
				if (buf != NULL) {
					buf[size] = 13;
					buf[size + 1] = 10;
				}
				size += 2;
			}
		}
	}
	if (length != NULL)
		*length = size;
	return result;
}

/* the text of the entries, kept until it is given back */
int IniProcessor::text(void **buf, int *length)
{
	IniAllocatorRef *allocator = m_allocator;
	int size = 0;
	int result;

	if (buf == NULL || length == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	if (!m_dirty && m_textValid) {
		*buf = m_text;
		*length = m_textLength;
	}
	result = measure(NULL, &size);
	if (result != 0)
		return result;
	if (size == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_EMPTY;
	if (m_text != NULL)
		iniFree(allocator, m_text);
	m_text = (char *)iniAlloc(allocator, size + 32);
	result = measure(m_text, &size);
	if (result == 0) {
		*buf = m_text;
		m_textLength = size;
		*length = size;
		m_textValid = true;
	}
	return result;
}

int IniProcessor::freeText()
{
	IniAllocatorRef *allocator = m_allocator;

	if (m_text != NULL)
		iniFree(allocator, m_text);
	m_text = NULL;
	return 0;
}

/* reads the lines of the input and makes entries of the ones of the form key=value */
int IniProcessor::scanBuffer(IniInput *input, SceSize limit)
{
	IniScanner scanner;
	IniToken token;
	SceSize position;
	bool finished = false;
	int result;

	iniTokenReset(&token);
	position = token.position;
	for (;;) {
		input->offset = position;
		result = scanner.scan(input, &token);
		if (result == INI_RESULT_END) {
			if (token.error != 2)
				goto finish;
			finished = true;
		} else if (result == INI_RESULT_TEXT_TOO_SHORT) {
			return INI_RESULT_BAD_ENCODING;
		} else if (result != 0) {
			goto finish;
		}
		if (input->checkBom)
			input->checkBom = false;
		if (token.line == INI_LINE_PAIR) {
			IniAllocatorOps *ops;
			void *context;
			IniString key;
			IniString value;

			iniAllocatorOf(m_allocator, &ops, &context);
			key.data = g_iniEmptyString;
			key.length = 0;
			key.ops = ops;
			key.context = context;
			iniAllocatorOf(m_allocator, &ops, &context);
			value.data = g_iniEmptyString;
			value.length = 0;
			value.ops = ops;
			value.context = context;
			iniStringInsert(&key, 0, token.key, token.keyLength);
			iniStringInsert(&value, 0, token.value, token.valueLength);
			addEntry(&key, &value);
			iniStringDestroy(&value);
			iniStringDestroy(&key);
		}
		if (!finished) {
			position = token.position;
			if (position < limit)
				continue;
		}
		break;
	}
finish:
	if (result == INI_RESULT_END || result == 0) {
		if (m_count != 0)
			m_current = m_head.next;
		result = 0;
	}
	return result;
}

/* makes the entries of a text */
int IniProcessor::parse(const char *buf, SceSize size)
{
	IniInput input;
	int result;

	removeAll();
	m_current = &m_head;
	m_reserved = 0;
	if (size == 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_EMPTY;
	iniInputInit(&input);
	input.data = buf;
	input.size = size > 4 ? size + 1 : 4;
	input.checkBom = true;
	result = scanBuffer(&input, size);
	return result;
}

IniProcessor::~IniProcessor()
{
	IniAllocatorRef *allocator = m_allocator;

	if (m_file != NULL) {
		m_file->~IniFileBase();
		iniFree(m_allocator, m_file);
	}
	m_file = NULL;
	if (m_temp != NULL) {
		m_temp->~IniFileBase();
		iniFree(allocator, m_temp);
	}
	m_temp = NULL;
	clear();
	removeAll();
	if (m_head.value.data != g_iniEmptyString)
		m_head.value.ops->free(m_head.value.data, m_head.value.context);
	if (m_head.key.data != g_iniEmptyString)
		m_head.key.ops->free(m_head.key.data, m_head.key.context);
}
