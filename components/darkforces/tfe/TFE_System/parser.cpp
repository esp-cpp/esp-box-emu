#include <cstring>

#include "parser.h"
#include <TFE_System/espboxShared.h>
#include <algorithm>
#ifdef TFE_ESPBOX
#include <TFE_FileSystem/filestream.h>
#include <cstdlib>
#endif

namespace
{
#ifdef TFE_ESPBOX
	static char* s_line = nullptr;	// 4096 bytes (shared memory)
#else
	static char s_line[4096];
#endif
	bool isWhitespace(const char c)
	{
		if (c > 32 && c < 127)
		{
			return false;
		}
		return true;
	}

	bool isSeparator(const char c)
	{
		if (c == '=' || c == ',')
		{
			return true;
		}
		return false;
	}
}

TFE_Parser::TFE_Parser() : m_buffer(nullptr), m_bufferLen(0u), m_enableBlockComments(false), m_blockComment(false), m_enableColorSeperator(false), m_convertToUppercase(false) {}
#ifdef TFE_ESPBOX
TFE_Parser::~TFE_Parser()
{
	free(m_window);
}

namespace
{
	// Window size and the amount kept available ahead of the read position (must
	// exceed the longest line, which s_line limits to 4096 bytes).
	constexpr size_t WINDOW_SIZE = 64 * 1024;
	constexpr size_t WINDOW_RESERVE = 8 * 1024;
}

bool TFE_Parser::initFromFile(FileStream* file)
{
	if (!m_window)
	{
		m_window = (char*)malloc(WINDOW_SIZE + 1);
		if (!m_window) { return false; }
	}
	m_file = file;
	m_buffer = m_window;
	m_bufferLen = 0;
	m_fileSize = file->getSize();
	m_filePos = 0;
	m_eof = (m_fileSize == 0);
	size_t pos = 0;
	refillWindow(pos);
	return true;
}

// Keep at least WINDOW_RESERVE bytes available after bufferPos (until the end of
// the file): drop what has been consumed and read the next part of the file.
void TFE_Parser::refillWindow(size_t& bufferPos)
{
	if (!m_file || m_eof || m_bufferLen - bufferPos >= WINDOW_RESERVE) { return; }
	if (bufferPos > 0)
	{
		memmove(m_window, m_window + bufferPos, m_bufferLen - bufferPos);
		m_bufferLen -= bufferPos;
		bufferPos = 0;
	}
	// The loaders open other files while parsing (textures, palettes, ...), some of
	// them in the same archive, which moves the archive's current file and
	// position: select this file again and seek to where the window ended. The
	// read is also clamped to the file (archive reads are not).
	size_t want = WINDOW_SIZE - m_bufferLen;
	if (want > m_fileSize - m_filePos) { want = m_fileSize - m_filePos; }
	m_file->reselect();
	m_file->seek((s32)m_filePos);
	// Read byte-wise elements: a single element of `want` bytes would read nothing
	// at all once fewer bytes than that remain in the file.
	const size_t got = want ? m_file->readBuffer(m_window + m_bufferLen, 1, (u32)want) : 0;
	m_bufferLen += got;
	m_filePos += got;
	m_window[m_bufferLen] = 0;	// the block comment check looks one byte ahead.
	if (got < want || m_filePos >= m_fileSize) { m_eof = true; }
}
#else
TFE_Parser::~TFE_Parser() {}
#endif

void TFE_Parser::init(const char* buffer, size_t len)
{
	m_buffer = buffer;
	m_bufferLen = len;
#ifdef TFE_ESPBOX
	m_file = nullptr;
	m_eof = true;
#endif
}

// Enable block comments of the form /*...*/
void TFE_Parser::enableBlockComments()
{
	m_enableBlockComments = true;
}

// Enable : as a seperator but do not remove it.
void TFE_Parser::enableColonSeperator()
{
	m_enableColorSeperator = true;
}

// Add a string representing a comment, such as ";" "#" "//"
void TFE_Parser::addCommentString(const char* comment)
{
	m_commentStrings.push_back(comment);
}

// Convert resulting strings to upper case, defaults to false.
void TFE_Parser::convertToUpperCase(bool enable)
{
	m_convertToUppercase = enable;
}

bool TFE_Parser::isComment(const char* buffer)
{
	const size_t commentCount = m_commentStrings.size();
	const std::string* comments = m_commentStrings.data();
	for (size_t c = 0; c < commentCount; c++)
	{
		if (strncmp(comments[c].c_str(), buffer, comments[c].length()) == 0)
		{
			return true;
		}
	}
	return false;
}

// Read the next non-comment/whitespace line.
const char* TFE_Parser::readLine(size_t& bufferPos, bool skipLeadingWhitespace, bool commentOnlyAtBeginning)
{
#ifdef TFE_ESPBOX
	refillWindow(bufferPos);
#endif
	if (bufferPos >= m_bufferLen || m_bufferLen < 1) { return nullptr; }

	// Keep reading lines until either one has real content or we reach the end of the buffer.
	bool lineHasContent = false;
	s32 skip = -1;
	while (!lineHasContent)
	{
#ifdef TFE_ESPBOX
		// Refill before testing for the end: the window may have been consumed exactly
		// up to its end by the previous line while the file still has more.
		refillWindow(bufferPos);
#endif
		if (bufferPos >= m_bufferLen) { break; }
		s_line[0] = 0;
		size_t linePos = 0;
		bool inComment = false;
		for (size_t i = bufferPos; i < m_bufferLen; i++)
		{
			bufferPos = i + 1;

			if (m_enableBlockComments && i > 0 && m_buffer[i-1] == '*' && m_buffer[i] == '/')
			{
				m_blockComment = false;
			}
			else if (m_enableBlockComments && m_buffer[i] == '/' && m_buffer[i+1] == '*')
			{
				m_blockComment = true;
			}
			else if (m_buffer[i] == '\n' || m_buffer[i] == '\r')
			{
				// search for the next valid character, then end it.
				for (size_t ii = i + 1; ii < m_bufferLen; ii++)
				{
					if (m_buffer[ii] != '\n' && m_buffer[ii] != '\r')
					{
						bufferPos = ii;
						break;
					}
				}
				break;
			}
			else if (!inComment && !m_blockComment)
			{
				// is this the beginning of a comment?
				if (!commentOnlyAtBeginning)
				{
					inComment = isComment(m_buffer + i);
				}

				// if not in a comment, go ahead and add to the line.
				if (!inComment)
				{
					if (m_convertToUppercase)
					{
						s_line[linePos++] = toupper(m_buffer[i]);
					}
					else
					{
						s_line[linePos++] = m_buffer[i];
					}
				}
			}
		}
		s_line[linePos] = 0;

		// check to see if the line has any content.
		lineHasContent = false;
		skip = -1;
		for (size_t i = 0; i < linePos; i++)
		{
			// Content is any non-white space character.
			if (!isWhitespace(s_line[i]))
			{
				// Is this a comment?
				if (commentOnlyAtBeginning && isComment(&s_line[i]))
				{
					break;
				}
				if (skip < 0) { skip = s32(i); }
				lineHasContent = true;
				break;
			}
		}
	}

	if (lineHasContent && skipLeadingWhitespace && skip > 0)
	{
		return &s_line[skip];
	}

	return s_line[0]!=0 ? s_line : nullptr;
}

// Split a line into tokens using space, comma or equals as separators.
// Note strings with spaces still work, they need to be closed in quotes, which are removed upon tokenizing.
void TFE_Parser::tokenizeLine(const char* line, TokenList& tokens)
{
	tokens.clear();

	const size_t len = strlen(line);
	// first move past leading whitespace and ending white space.
	size_t start = 0, end = 0;
	for (size_t c = 0; c < len; c++)
	{
		if (!isWhitespace(line[c]))
		{
			if (start == 0 && end == 0) { start = c; }
			end = c + 1;
		}
	}

	// next start reading tokens.
	bool inQuote = false;
	char curToken[1024];
	size_t curTokenPos = 0;
	// TODO: Add an option to allow white space in tokens when not in quotes, but still remove trailing/ending whitespace.
	// This is useful for names.
	for (size_t c = start; c < end; c++)
	{
		if (line[c] == '"')
		{
			if (inQuote && curTokenPos == 0)
			{
				tokens.push_back("");
			}
			inQuote = !inQuote;
		}
		else if (!inQuote && (isWhitespace(line[c]) || isSeparator(line[c])))
		{
			curToken[curTokenPos] = 0;
			if (curTokenPos)
			{
				tokens.push_back(curToken);
			}

			curTokenPos = 0;
			curToken[0] = 0;
		}
		else if (!inQuote && m_enableColorSeperator && line[c] == ':')
		{
			curToken[curTokenPos++] = line[c];

			curToken[curTokenPos] = 0;
			if (curTokenPos)
			{
				tokens.push_back(curToken);
			}

			curTokenPos = 0;
			curToken[0] = 0;
		}
		else
		{
			curToken[curTokenPos++] = line[c];
		}
	}

	if (curTokenPos)
	{
		curToken[curTokenPos] = 0;
		tokens.push_back(curToken);
	}
}

#ifdef TFE_ESPBOX
void espbox_shared_parser(bool alloc)
{
	ESPBOX_SHARED_ALLOC(s_line, 4096);
}
#endif
