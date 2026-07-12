/* trn/OptionApplier.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_OPTION_APPLIER_H
#define TRN_OPTION_APPLIER_H

#include <trn/opt.h>

class IniSectionValues;
class OptionDraft;

class OptionApplier
{
public:
    using ApplyOne = void (*)(OptionIndex option, const char *value);

    OptionApplier();
    explicit OptionApplier(ApplyOne apply_one);

    void apply(const IniSectionValues &values) const;
    void apply(const OptionDraft &draft) const;
    void apply(OptionIndex option, const char *value) const;

private:
    ApplyOne m_apply_one;
};

#endif
