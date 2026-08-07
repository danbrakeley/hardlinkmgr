#pragma once

#include <QComboBox>
#include <QSpinBox>
#include <QStringList>

#include <limits>

// The byte-size spinbox + unit dropdown pair used by both MatchFinderPanel and
// LinkFinderPanel's "Size Min" (and Match Finder's "Size Difference") controls.
namespace sizeunits {

inline QComboBox *makeUnitCombo(QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->addItems({QStringLiteral("bytes"), QStringLiteral("KiB"),
                     QStringLiteral("MiB"), QStringLiteral("GiB")});
    return combo;
}

inline QSpinBox *makeSizeSpin(QWidget *parent)
{
    auto *spin = new QSpinBox(parent);
    spin->setRange(0, std::numeric_limits<int>::max());
    return spin;
}

inline quint64 byteValue(const QSpinBox *value, const QComboBox *unit)
{
    return quint64(value->value()) << (10 * unit->currentIndex());
}

} // namespace sizeunits
