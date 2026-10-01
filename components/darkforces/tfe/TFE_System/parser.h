#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine System Library
// System functionality, such as timers and logging.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include <vector>
#include <string>

typedef std::vector<std::string> TokenList;
class FileStream;

class TFE_Parser
{
public:
	TFE_Parser();
	~TFE_Parser();

	void init(const char* buffer, size_t len);
#ifdef TFE_ESPBOX
	// Parse straight from an open file through a small sliding window instead of
	// a buffer holding the whole file (level files are up to ~1MB). The file must
	// stay open while the parser is used; readLine() keeps bufferPos valid across
	// window refills.
	bool initFromFile(FileStream* file);
#endif

	// Enable block comments of the form /*...*/
	void enableBlockComments();

	// Enable : as a seperator but do not remove it.
	void enableColonSeperator();

	// Add a string representing a comment, such as ";" "#" "//"
	void addCommentString(const char* comment);

	// Convert resulting strings to upper case, defaults to false.
	void convertToUpperCase(bool enable);

	// Read the next non-comment/whitespace line.
	const char* readLine(size_t& bufferPos, bool skipLeadingWhitespace = false, bool commentOnlyAtBeginning = false);
	// Split a line into tokens using space, comma or equals as separators.
	// Note strings with spaces still work, they need to be closed in quotes, which are removed upon tokenizing.
	void tokenizeLine(const char* line, TokenList& tokens);

private:
	const char* m_buffer;
	size_t m_bufferLen;
	TokenList m_commentStrings;
	bool m_enableBlockComments;
	bool m_blockComment;
	bool m_enableColorSeperator;
	bool m_convertToUppercase;

private:
	bool isComment(const char* buffer);
#ifdef TFE_ESPBOX
	void refillWindow(size_t& bufferPos);
	FileStream* m_file = nullptr;
	char* m_window = nullptr;
	bool m_eof = true;
	size_t m_fileSize = 0;
	size_t m_filePos = 0;	// bytes of the file read into the window so far
#endif
};
