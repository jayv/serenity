/*
 * Copyright (c) 2020, the SerenityOS developers.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Cell.h"
#include "Spreadsheet.h"
#include <AK/StringBuilder.h>

namespace Spreadsheet {

void Cell::set_data(String new_data)
{
    if (data == new_data)
        return;

    if (new_data.starts_with("=")) {
        new_data = new_data.substring(1, new_data.length() - 1);
        kind = Formula;
    } else {
        kind = LiteralString;
    }

    data = move(new_data);
    dirty = true;
    evaluated_externally = false;
}

void Cell::set_data(JS::Value new_data)
{
    dirty = true;
    evaluated_externally = true;

    StringBuilder builder;

    builder.append(new_data.to_string_without_side_effects());
    data = builder.build();

    evaluated_data = move(new_data);
}

void Cell::update_data()
{
    TemporaryChange cell_change { sheet->current_evaluated_cell(), this };
    if (!dirty)
        return;

    dirty = false;
    if (kind == Formula) {
        if (!evaluated_externally)
            evaluated_data = sheet->evaluate(data, this);
    }

    for (auto& ref : referencing_cells) {
        if (ref) {
            ref->dirty = true;
            ref->update();
        }
    }
}

void Cell::update()
{
    sheet->update(*this);
}

JS::Value Cell::js_data()
{
    if (dirty)
        update();

    if (kind == Formula)
        return evaluated_data;

    return JS::js_string(sheet->interpreter(), data);
}

String Cell::source() const
{
    StringBuilder builder;
    if (kind == Formula)
        builder.append('=');
    builder.append(data);
    return builder.to_string();
}

// FIXME: Find a better way to figure out dependencies
void Cell::reference_from(Cell* other)
{
    if (!other || other == this)
        return;

    if (!referencing_cells.find([other](auto& ptr) { return ptr.ptr() == other; }).is_end())
        return;

    referencing_cells.append(other->make_weak_ptr());
}

}