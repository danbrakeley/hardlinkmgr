#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include "core/MatchPairing.h"

// The checked-rows sanity check behind "Link Selected Matches": the same file
// appearing twice among the checked rows would make later jobs act on a path
// that an earlier job already replaced or renamed. Returns the offending
// paths structurally; the panel does the tr() formatting.
namespace matchconflicts {

struct Conflicts
{
    // One entry per extra occurrence of a secondary, in row order.
    QStringList duplicateSecondaries;
    // Paths that are the primary of one checked row and the secondary of
    // another, in first-seen row order.
    QStringList primaryAndSecondary;

    bool isEmpty() const
    {
        return duplicateSecondaries.isEmpty() && primaryAndSecondary.isEmpty();
    }
};

inline Conflicts find(const QList<matchpairing::Match> &checkedMatches)
{
    Conflicts conflicts;
    QSet<QString> primaries;
    QSet<QString> secondaries;
    for (const matchpairing::Match &match : checkedMatches) {
        if (secondaries.contains(match.secondaryPath)) {
            conflicts.duplicateSecondaries.append(match.secondaryPath);
        }
        primaries.insert(match.primaryPath);
        secondaries.insert(match.secondaryPath);
    }
    for (const matchpairing::Match &match : checkedMatches) {
        if (primaries.contains(match.secondaryPath)
            && !conflicts.primaryAndSecondary.contains(match.secondaryPath)) {
            conflicts.primaryAndSecondary.append(match.secondaryPath);
        }
    }
    return conflicts;
}

} // namespace matchconflicts
