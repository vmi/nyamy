//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// adhoc_keyseq.h
//
// AdHocItem and AdHocKeySeq types for scheduling ad-hoc key sequences
// into Engine::m_inputQueue without storing them in a Setting.

#ifndef _ADHOC_KEYSEQ_H
#  define _ADHOC_KEYSEQ_H

#  include "trigger_info.h"
#  include <memory>
#  include <vector>

class KeySeq;  // forward declaration

/// Execution unit for an ad-hoc key sequence:
/// a materialized KeySeq paired with context for Current reconstruction.
struct AdHocItem {
	std::unique_ptr<KeySeq>              keySeq;
	std::vector<std::unique_ptr<KeySeq>> subKeySeqs;  // sub-sequences referenced by keySeq
	TriggerInfo                          context;
};
using AdHocKeySeq = std::unique_ptr<AdHocItem>;

#endif // !_ADHOC_KEYSEQ_H
