/*
 * Query command tokenizer and dispatcher (BeCommandEntry, BeCommandParser).
 */

#include "be_isobmf_internal.h"

BeCommandEntry::~BeCommandEntry()
{
}

void BeCommandParser::tokenize(const char *cmdLine)
{
	m_tokenCount = 0;
	char ch = *cmdLine;
	if (ch == '\0')
		return;

	if (m_tokens[0] == NULL)
		return;

	int charPos = 0;
	int tokIndex = 0;
	const char *cursor = cmdLine;
	do {
		m_tokens[tokIndex][charPos] = ch;
		if (m_tokens[tokIndex][charPos] != ' ' && m_tokens[tokIndex][charPos] != '\0') {
			charPos++;
		} else {
			m_tokens[tokIndex][charPos] = '\0';
			tokIndex++;
			charPos = 0;
			m_tokenCount++;
		}
		ch = *++cursor;
	} while (ch != '\0');

	m_tokens[tokIndex][charPos] = '\0';
}

int BeCommandParser::registerCommand(const char *name, int argCount)
{
	m_commands.moveFirst();
	do {
		BeCommandEntry *entry = static_cast<BeCommandEntry *>(m_commands.getCurrent());
		if (entry == NULL)
			break;
		if (beStrncmp(entry->m_name, name, sizeof(entry->m_name)) == 0)
			return 0;
	} while (m_commands.moveNext() == 1);

	BeCommandEntry *entry = new BeCommandEntry();
	beStrncpy(entry->m_name, name, sizeof(entry->m_name));
	entry->m_argCount = argCount;
	m_commands.append(entry);
	return 1;
}

BeCommandParser::BeCommandParser()
{
	for (int i = 0; i < 16; i++)
		m_tokens[i] = static_cast<char *>(BeMemObject::operator new[](128));
	registerCommand("get", 3);
}

BeCommandParser::~BeCommandParser()
{
	for (int i = 0; i < 16; i++)
		BeMemObject::operator delete[](m_tokens[i]);
}

int BeCommandParser::executeCommand(const char *cmdLine)
{
	tokenize(cmdLine);
	if (m_tokenCount == 0)
		return 0;

	const char *cmdName = m_tokens[0];
	m_commands.moveFirst();
	BeCommandEntry *matched = NULL;
	for (;;) {
		BeCommandEntry *entry = static_cast<BeCommandEntry *>(m_commands.getCurrent());
		if (entry == NULL)
			return 0;
		if (beStrncmp(entry->m_name, cmdName, sizeof(entry->m_name)) == 0) {
			matched = entry;
			break;
		}
		if (m_commands.moveNext() != 1)
			return 0;
	}

	if (matched->m_argCount == m_tokenCount)
		return onCommand(m_tokenCount, const_cast<const char **>(m_tokens));
	return 0;
}

int BeCommandParser::onCommand(int argc, const char **argv)
{
	(void)argc;
	(void)argv;
	return 0;
}
