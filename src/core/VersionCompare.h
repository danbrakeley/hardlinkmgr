#pragma once

#include <QRegularExpression>
#include <QString>

// Compares GitHub release tags ("v0.3.0") against APP_VERSION strings, which
// may carry a "-{short_hash}"/"-dev" build suffix for untagged/dirty builds
// (see cmake/GenerateBuildInfo.cmake) that isn't part of the comparison.
namespace versioncompare {

struct Version
{
    int major = 0;
    int minor = 0;
    int patch = 0;

    friend bool operator<(const Version &a, const Version &b)
    {
        if (a.major != b.major) return a.major < b.major;
        if (a.minor != b.minor) return a.minor < b.minor;
        return a.patch < b.patch;
    }
};

// Extracts the leading "MAJOR.MINOR.PATCH" run from a version/tag string,
// ignoring any "v" prefix or "-..." suffix. Unparseable input (e.g. an empty
// string from a malformed API response) parses as 0.0.0.
inline Version parseVersion(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("(\\d+)\\.(\\d+)\\.(\\d+)"));
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch()) {
        return Version{};
    }
    return Version{m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()};
}

// True when `latestTag` (a GitHub release tag) names a strictly newer release
// than `currentVersion` (APP_VERSION).
inline bool isNewer(const QString &latestTag, const QString &currentVersion)
{
    return parseVersion(currentVersion) < parseVersion(latestTag);
}

} // namespace versioncompare
