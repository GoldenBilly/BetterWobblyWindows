/*
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "BetterWobblyWindows/EmbeddedBlurMath.h"
#include "TestSupport.h"

#include <QMatrix4x4>
#include <QVector4D>

int main()
{
    using namespace BetterWobblyWindowsTest;
    using namespace KWin::BetterWobblyWindows;

    const QVector4D source(0.8f, 0.6f, 0.4f, 1.0f);

    const QVector4D unchanged = source * BuildColorTransformMatrix(1.0, 1.0, 1.0);
    CHECK(NearlyEqual(unchanged.x(), source.x()));
    CHECK(NearlyEqual(unchanged.y(), source.y()));
    CHECK(NearlyEqual(unchanged.z(), source.z()));
    CHECK(NearlyEqual(unchanged.w(), source.w()));

    const QVector4D darkened = source * BuildColorTransformMatrix(1.0, 1.0, 0.5);
    CHECK(NearlyEqual(darkened.x(), 0.4f));
    CHECK(NearlyEqual(darkened.y(), 0.3f));
    CHECK(NearlyEqual(darkened.z(), 0.2f));
    CHECK(NearlyEqual(darkened.w(), 1.0f));

    const QVector4D brightened = source * BuildColorTransformMatrix(1.0, 1.0, 1.25);
    CHECK(NearlyEqual(brightened.x(), 1.0f));
    CHECK(NearlyEqual(brightened.y(), 0.75f));
    CHECK(NearlyEqual(brightened.z(), 0.5f));
    CHECK(NearlyEqual(brightened.w(), 1.0f));

    const QVector4D reducedContrast = source * BuildColorTransformMatrix(1.0, 0.5, 1.0);
    CHECK(NearlyEqual(reducedContrast.x(), 0.65f));
    CHECK(NearlyEqual(reducedContrast.y(), 0.55f));
    CHECK(NearlyEqual(reducedContrast.z(), 0.45f));
    CHECK(NearlyEqual(reducedContrast.w(), 1.0f));

    const QVector4D increasedContrast = source * BuildColorTransformMatrix(1.0, 2.0, 1.0);
    CHECK(NearlyEqual(increasedContrast.x(), 1.1f));
    CHECK(NearlyEqual(increasedContrast.y(), 0.7f));
    CHECK(NearlyEqual(increasedContrast.z(), 0.3f));
    CHECK(NearlyEqual(increasedContrast.w(), 1.0f));

    const QVector4D grayscale = source * BuildColorTransformMatrix(0.0, 1.0, 1.0);
    const float expectedLuma = 0.8f * 0.2126f + 0.6f * 0.7152f + 0.4f * 0.0722f;
    CHECK(NearlyEqual(grayscale.x(), expectedLuma));
    CHECK(NearlyEqual(grayscale.y(), expectedLuma));
    CHECK(NearlyEqual(grayscale.z(), expectedLuma));
    CHECK(NearlyEqual(grayscale.w(), 1.0f));

    const QVector4D combined = source * BuildColorTransformMatrix(0.0, 0.5, 0.5);
    const float expectedCombined = (expectedLuma * 0.5f + 0.25f) * 0.5f;
    CHECK(NearlyEqual(combined.x(), expectedCombined));
    CHECK(NearlyEqual(combined.y(), expectedCombined));
    CHECK(NearlyEqual(combined.z(), expectedCombined));
    CHECK(NearlyEqual(combined.w(), 1.0f));

    return 0;
}
