//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// log_level.h
//
// Shared log severity.  Used by nyamy's log stream, by the ctrl stream that
// carries the current threshold to the scripter, and by the scripter's own
// output tagging.


#ifndef _LOG_LEVEL_H
#  define _LOG_LEVEL_H


#  include <cstdint>


/// Log severity.  Ordered so that a message is emitted when its level is
/// numerically <= the current threshold; Error therefore always passes.
enum class LogLevel : uint8_t {
	Error = 0,
	Warn  = 1,
	Info  = 2,	///< threshold when the "detail" box is unchecked
	Debug = 3,	///< threshold when the "detail" box is checked
};


/// Threshold in force while "detail" is off / on.
const LogLevel kLogLevelNormal = LogLevel::Info;
const LogLevel kLogLevelDetail = LogLevel::Debug;


/// One-character tag used in the log line and on the scripter's message pipe.
inline wchar_t logLevelChar(LogLevel i_level)
{
	switch (i_level) {
	case LogLevel::Error: return L'E';
	case LogLevel::Warn:  return L'W';
	case LogLevel::Debug: return L'D';
	default:              return L'I';
	}
}


/// Inverse of logLevelChar().  Returns false when the character is not a tag.
inline bool logLevelFromChar(wchar_t i_c, LogLevel *o_level)
{
	switch (i_c) {
	case L'E': *o_level = LogLevel::Error; return true;
	case L'W': *o_level = LogLevel::Warn;  return true;
	case L'I': *o_level = LogLevel::Info;  return true;
	case L'D': *o_level = LogLevel::Debug; return true;
	}
	return false;
}


/// Clamp an out-of-range wire value to a usable threshold.
inline LogLevel logLevelFromByte(uint8_t i_byte)
{
	return (i_byte > static_cast<uint8_t>(LogLevel::Debug))
	       ? LogLevel::Debug : static_cast<LogLevel>(i_byte);
}


#endif // !_LOG_LEVEL_H
