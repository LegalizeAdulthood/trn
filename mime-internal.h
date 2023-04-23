#ifndef TRN_MIME_INTERNAL_H
#define TRN_MIME_INTERNAL_H

#include "mime.h"
#include <functional>

// Internal entry points for testing purposes.

using MimeExecutor = std::function<int(const char *shell, const char *cmd)>;
void mime_set_executor(MimeExecutor executor);

#endif
