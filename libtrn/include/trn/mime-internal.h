#ifndef TRN_MIME_INTERNAL_H
#define TRN_MIME_INTERNAL_H

#include <trn/mime.h>

#include <functional>

// Internal entry points for testing purposes.

using MimeExecutor = std::function<int(std::string_view shell, std::string_view cmd)>;
void mime_set_executor(MimeExecutor executor);

#endif
