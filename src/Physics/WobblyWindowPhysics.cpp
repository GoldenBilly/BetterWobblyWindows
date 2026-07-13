/*
    KWin - the KDE window manager
    Portions of this file are adapted from the KDE KWin Wobbly Windows effect.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>
    SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126

    SPDX-License-Identifier: GPL-3.0-or-later

    Official KWin Wobbly Windows source:
    https://github.com/KDE/kwin/tree/master/src/plugins/wobblywindows
    Upstream authors are credited above according to the corresponding KDE files.
*/
#include "WobblyWindowsEffect_p.h"

#include <array>
#include <cmath>

namespace KWin
{

namespace
{
constexpr unsigned int GridDimension = 4;
}

void WobblyWindowsEffect::InitWobblyInfo(WindowWobblyInfos &wwi, RectF geometry) const
{
    wwi.count = GridDimension * GridDimension;
    wwi.width = GridDimension;
    wwi.height = GridDimension;

    wwi.origin.resize(wwi.count);
    wwi.position.resize(wwi.count);
    wwi.velocity.resize(wwi.count);
    wwi.acceleration.resize(wwi.count);
    wwi.buffer.resize(wwi.count);
    wwi.constraint.resize(wwi.count);

    wwi.status = Moving;

    const qreal x = geometry.x();
    const qreal y = geometry.y();
    const qreal width = geometry.width();
    const qreal height = geometry.height();

    Pair initValue = {x, y};
    constexpr Pair NullPair = {0.0, 0.0};

    const qreal xIncrement = width / (wwi.width - 1.0);
    const qreal yIncrement = height / (wwi.height - 1.0);

    for (unsigned int j = 0; j < GridDimension; ++j) {
        for (unsigned int i = 0; i < GridDimension; ++i) {
            const unsigned int index = j * GridDimension + i;
            wwi.origin[index] = initValue;
            wwi.position[index] = initValue;
            wwi.velocity[index] = NullPair;
            wwi.constraint[index] = false;
            if (i != GridDimension - 2) {
                initValue.x += xIncrement;
            } else {
                initValue.x = width + x;
            }
        }
        initValue.x = x;
        if (j != GridDimension - 2) {
            initValue.y += yIncrement;
        } else {
            initValue.y = height + y;
        }
    }
}

WobblyWindowsEffect::Pair WobblyWindowsEffect::ComputeBezierPoint(const WindowWobblyInfos &wwi, Pair point) const
{
    const qreal tx = point.x;
    const qreal ty = point.y;

    const qreal inverseTx = 1.0 - tx;
    const qreal inverseTy = 1.0 - ty;
    const std::array<qreal, GridDimension> px = {
        inverseTx * inverseTx * inverseTx,
        3.0 * inverseTx * inverseTx * tx,
        3.0 * inverseTx * tx * tx,
        tx * tx * tx,
    };
    const std::array<qreal, GridDimension> py = {
        inverseTy * inverseTy * inverseTy,
        3.0 * inverseTy * inverseTy * ty,
        3.0 * inverseTy * ty * ty,
        ty * ty * ty,
    };

    Pair res = {0.0, 0.0};

    for (unsigned int j = 0; j < GridDimension; ++j) {
        for (unsigned int i = 0; i < GridDimension; ++i) {
            res.x += px[i] * py[j] * wwi.position[i + j * wwi.width].x;
            res.y += px[i] * py[j] * wwi.position[i + j * wwi.width].y;
        }
    }

    return res;
}

namespace
{

void FixVectorBounds(WobblyWindowsEffect::Pair &vec, qreal min, qreal max)
{
    if (fabs(vec.x) < min) {
        vec.x = 0.0;
    } else if (fabs(vec.x) > max) {
        if (vec.x > 0.0) {
            vec.x = max;
        } else {
            vec.x = -max;
        }
    }

    if (fabs(vec.y) < min) {
        vec.y = 0.0;
    } else if (fabs(vec.y) > max) {
        if (vec.y > 0.0) {
            vec.y = max;
        } else {
            vec.y = -max;
        }
    }
}

#if defined COMPUTE_STATS
void ComputeVectorBounds(const WobblyWindowsEffect::Pair &vec, WobblyWindowsEffect::Pair &bound)
{
    if (fabs(vec.x) < bound.x) {
        bound.x = fabs(vec.x);
    } else if (fabs(vec.x) > bound.y) {
        bound.y = fabs(vec.x);
    }
    if (fabs(vec.y) < bound.x) {
        bound.x = fabs(vec.y);
    } else if (fabs(vec.y) > bound.y) {
        bound.y = fabs(vec.y);
    }
}
#endif

} // namespace

bool WobblyWindowsEffect::UpdateWindowWobblyDatas(EffectWindow *w, qreal time)
{
    RectF rect = w->frameGeometry();
    auto infoIt = _windows.find(w);
    if (infoIt == _windows.end()) {
        return false;
    }
    WindowWobblyInfos &wwi = infoIt.value();

    qreal x_length = rect.width() / (wwi.width - 1.0);
    qreal y_length = rect.height() / (wwi.height - 1.0);

#if defined VERBOSE_MODE
    qCDebug(KWIN_WOBBLYWINDOWS) << "time " << time;
    qCDebug(KWIN_WOBBLYWINDOWS) << "increment x " << x_length << " // y" << y_length;
#endif

    Pair origine = {rect.x(), rect.y()};

    for (unsigned int j = 0; j < wwi.height; ++j) {
        for (unsigned int i = 0; i < wwi.width; ++i) {
            wwi.origin[wwi.width * j + i] = origine;
            if (i != wwi.width - 2) {
                origine.x += x_length;
            } else {
                origine.x = rect.width() + rect.x();
            }
        }
        origine.x = rect.x();
        if (j != wwi.height - 2) {
            origine.y += y_length;
        } else {
            origine.y = rect.height() + rect.y();
        }
    }

    // Visual-only elastic counter-resize. SlotWindowStepUserMovedResized()
    // converts each real edge movement into a short displacement of the
    // opposite rendered edge. This local spring keeps relaxing even while the
    // mouse button is still held, and it continues after release until the mesh
    // reaches the exact real geometry.
    if (!_visualOppositePointFollowOnResize || !wwi.visualOppositePointAnchorValid) {
        wwi.visualOppositePointPendingImpulse = {0.0, 0.0};
    }

    wwi.visualOppositePointFollowOffset.x += wwi.visualOppositePointPendingImpulse.x;
    wwi.visualOppositePointFollowOffset.y += wwi.visualOppositePointPendingImpulse.y;
    wwi.visualOppositePointPendingImpulse = {0.0, 0.0};

    const qreal visualOffsetLength =
        std::hypot(wwi.visualOppositePointFollowOffset.x, wwi.visualOppositePointFollowOffset.y);
    if (visualOffsetLength > BetterWobblyWindows::OppositePointFollowMaximumPixels) {
        const qreal scale = BetterWobblyWindows::OppositePointFollowMaximumPixels / visualOffsetLength;
        wwi.visualOppositePointFollowOffset.x *= scale;
        wwi.visualOppositePointFollowOffset.y *= scale;
    }

    const qreal dtSeconds = std::clamp(time, static_cast<qreal>(0.0), static_cast<qreal>(32.0)) / 1000.0;
    const qreal omega =
        static_cast<qreal>(6.2831853071795864769) * BetterWobblyWindows::OppositePointFollowSpringFrequencyHz;
    const qreal damping = static_cast<qreal>(2.0) * BetterWobblyWindows::OppositePointFollowDampingRatio * omega;
    const qreal stiffness = omega * omega;

    const Pair visualSpringAcceleration = {
        -stiffness * wwi.visualOppositePointFollowOffset.x - damping * wwi.visualOppositePointFollowVelocity.x,
        -stiffness * wwi.visualOppositePointFollowOffset.y - damping * wwi.visualOppositePointFollowVelocity.y,
    };
    wwi.visualOppositePointFollowVelocity.x += visualSpringAcceleration.x * dtSeconds;
    wwi.visualOppositePointFollowVelocity.y += visualSpringAcceleration.y * dtSeconds;
    wwi.visualOppositePointFollowOffset.x += wwi.visualOppositePointFollowVelocity.x * dtSeconds;
    wwi.visualOppositePointFollowOffset.y += wwi.visualOppositePointFollowVelocity.y * dtSeconds;

    const qreal relaxedOffsetLength =
        std::hypot(wwi.visualOppositePointFollowOffset.x, wwi.visualOppositePointFollowOffset.y);
    const qreal relaxedVelocityLength =
        std::hypot(wwi.visualOppositePointFollowVelocity.x, wwi.visualOppositePointFollowVelocity.y);
    if (relaxedOffsetLength <= BetterWobblyWindows::OppositePointFollowStopPixels &&
        relaxedVelocityLength <= BetterWobblyWindows::OppositePointFollowStopVelocity) {
        wwi.visualOppositePointFollowOffset = {0.0, 0.0};
        wwi.visualOppositePointFollowVelocity = {0.0, 0.0};
    }

    Pair neighbors[4];
    Pair acceleration;

    qreal acc_sum = 0.0;
    qreal vel_sum = 0.0;

    if (wwi.constraint[0]) {
        Pair window_pos = wwi.origin[0];
        Pair current_pos = wwi.position[0];
        Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
        Pair accel = {move.x * _stiffness, move.y * _stiffness};
        wwi.acceleration[0] = accel;
    } else {
        Pair &pos = wwi.position[0];
        neighbors[0] = wwi.position[1];
        neighbors[1] = wwi.position[wwi.width];

        acceleration.x = ((neighbors[0].x - pos.x) - x_length) * _stiffness + (neighbors[1].x - pos.x) * _stiffness;
        acceleration.y = ((neighbors[1].y - pos.y) - y_length) * _stiffness + (neighbors[0].y - pos.y) * _stiffness;

        acceleration.x /= 2;
        acceleration.y /= 2;

        wwi.acceleration[0] = acceleration;
    }

    if (wwi.constraint[wwi.width - 1]) {
        Pair window_pos = wwi.origin[wwi.width - 1];
        Pair current_pos = wwi.position[wwi.width - 1];
        Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
        Pair accel = {move.x * _stiffness, move.y * _stiffness};
        wwi.acceleration[wwi.width - 1] = accel;
    } else {
        Pair &pos = wwi.position[wwi.width - 1];
        neighbors[0] = wwi.position[wwi.width - 2];
        neighbors[1] = wwi.position[2 * wwi.width - 1];

        acceleration.x = (x_length - (pos.x - neighbors[0].x)) * _stiffness + (neighbors[1].x - pos.x) * _stiffness;
        acceleration.y = ((neighbors[1].y - pos.y) - y_length) * _stiffness + (neighbors[0].y - pos.y) * _stiffness;

        acceleration.x /= 2;
        acceleration.y /= 2;

        wwi.acceleration[wwi.width - 1] = acceleration;
    }

    if (wwi.constraint[wwi.width * (wwi.height - 1)]) {
        Pair window_pos = wwi.origin[wwi.width * (wwi.height - 1)];
        Pair current_pos = wwi.position[wwi.width * (wwi.height - 1)];
        Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
        Pair accel = {move.x * _stiffness, move.y * _stiffness};
        wwi.acceleration[wwi.width * (wwi.height - 1)] = accel;
    } else {
        Pair &pos = wwi.position[wwi.width * (wwi.height - 1)];
        neighbors[0] = wwi.position[wwi.width * (wwi.height - 1) + 1];
        neighbors[1] = wwi.position[wwi.width * (wwi.height - 2)];

        acceleration.x = ((neighbors[0].x - pos.x) - x_length) * _stiffness + (neighbors[1].x - pos.x) * _stiffness;
        acceleration.y = (y_length - (pos.y - neighbors[1].y)) * _stiffness + (neighbors[0].y - pos.y) * _stiffness;

        acceleration.x /= 2;
        acceleration.y /= 2;

        wwi.acceleration[wwi.width * (wwi.height - 1)] = acceleration;
    }

    if (wwi.constraint[wwi.count - 1]) {
        Pair window_pos = wwi.origin[wwi.count - 1];
        Pair current_pos = wwi.position[wwi.count - 1];
        Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
        Pair accel = {move.x * _stiffness, move.y * _stiffness};
        wwi.acceleration[wwi.count - 1] = accel;
    } else {
        Pair &pos = wwi.position[wwi.count - 1];
        neighbors[0] = wwi.position[wwi.count - 2];
        neighbors[1] = wwi.position[wwi.width * (wwi.height - 1) - 1];

        acceleration.x = (x_length - (pos.x - neighbors[0].x)) * _stiffness + (neighbors[1].x - pos.x) * _stiffness;
        acceleration.y = (y_length - (pos.y - neighbors[1].y)) * _stiffness + (neighbors[0].y - pos.y) * _stiffness;

        acceleration.x /= 2;
        acceleration.y /= 2;

        wwi.acceleration[wwi.count - 1] = acceleration;
    }
    for (unsigned int i = 1; i < wwi.width - 1; ++i) {
        if (wwi.constraint[i]) {
            Pair window_pos = wwi.origin[i];
            Pair current_pos = wwi.position[i];
            Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
            Pair accel = {move.x * _stiffness, move.y * _stiffness};
            wwi.acceleration[i] = accel;
        } else {
            Pair &pos = wwi.position[i];
            neighbors[0] = wwi.position[i - 1];
            neighbors[1] = wwi.position[i + 1];
            neighbors[2] = wwi.position[i + wwi.width];

            acceleration.x = (x_length - (pos.x - neighbors[0].x)) * _stiffness +
                ((neighbors[1].x - pos.x) - x_length) * _stiffness + (neighbors[2].x - pos.x) * _stiffness;
            acceleration.y = ((neighbors[2].y - pos.y) - y_length) * _stiffness +
                (neighbors[0].y - pos.y) * _stiffness + (neighbors[1].y - pos.y) * _stiffness;

            acceleration.x /= 3;
            acceleration.y /= 3;

            wwi.acceleration[i] = acceleration;
        }
    }
    for (unsigned int i = wwi.width * (wwi.height - 1) + 1; i < wwi.count - 1; ++i) {
        if (wwi.constraint[i]) {
            Pair window_pos = wwi.origin[i];
            Pair current_pos = wwi.position[i];
            Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
            Pair accel = {move.x * _stiffness, move.y * _stiffness};
            wwi.acceleration[i] = accel;
        } else {
            Pair &pos = wwi.position[i];
            neighbors[0] = wwi.position[i - 1];
            neighbors[1] = wwi.position[i + 1];
            neighbors[2] = wwi.position[i - wwi.width];

            acceleration.x = (x_length - (pos.x - neighbors[0].x)) * _stiffness +
                ((neighbors[1].x - pos.x) - x_length) * _stiffness + (neighbors[2].x - pos.x) * _stiffness;
            acceleration.y = (y_length - (pos.y - neighbors[2].y)) * _stiffness +
                (neighbors[0].y - pos.y) * _stiffness + (neighbors[1].y - pos.y) * _stiffness;

            acceleration.x /= 3;
            acceleration.y /= 3;

            wwi.acceleration[i] = acceleration;
        }
    }
    for (unsigned int i = wwi.width; i < wwi.width * (wwi.height - 1); i += wwi.width) {
        if (wwi.constraint[i]) {
            Pair window_pos = wwi.origin[i];
            Pair current_pos = wwi.position[i];
            Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
            Pair accel = {move.x * _stiffness, move.y * _stiffness};
            wwi.acceleration[i] = accel;
        } else {
            Pair &pos = wwi.position[i];
            neighbors[0] = wwi.position[i + 1];
            neighbors[1] = wwi.position[i - wwi.width];
            neighbors[2] = wwi.position[i + wwi.width];

            acceleration.x = ((neighbors[0].x - pos.x) - x_length) * _stiffness +
                (neighbors[1].x - pos.x) * _stiffness + (neighbors[2].x - pos.x) * _stiffness;
            acceleration.y = (y_length - (pos.y - neighbors[1].y)) * _stiffness +
                ((neighbors[2].y - pos.y) - y_length) * _stiffness + (neighbors[0].y - pos.y) * _stiffness;

            acceleration.x /= 3;
            acceleration.y /= 3;

            wwi.acceleration[i] = acceleration;
        }
    }
    for (unsigned int i = 2 * wwi.width - 1; i < wwi.count - 1; i += wwi.width) {
        if (wwi.constraint[i]) {
            Pair window_pos = wwi.origin[i];
            Pair current_pos = wwi.position[i];
            Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
            Pair accel = {move.x * _stiffness, move.y * _stiffness};
            wwi.acceleration[i] = accel;
        } else {
            Pair &pos = wwi.position[i];
            neighbors[0] = wwi.position[i - 1];
            neighbors[1] = wwi.position[i - wwi.width];
            neighbors[2] = wwi.position[i + wwi.width];

            acceleration.x = (x_length - (pos.x - neighbors[0].x)) * _stiffness +
                (neighbors[1].x - pos.x) * _stiffness + (neighbors[2].x - pos.x) * _stiffness;
            acceleration.y = (y_length - (pos.y - neighbors[1].y)) * _stiffness +
                ((neighbors[2].y - pos.y) - y_length) * _stiffness + (neighbors[0].y - pos.y) * _stiffness;

            acceleration.x /= 3;
            acceleration.y /= 3;

            wwi.acceleration[i] = acceleration;
        }
    }
    for (unsigned int j = 1; j < wwi.height - 1; ++j) {
        for (unsigned int i = 1; i < wwi.width - 1; ++i) {
            unsigned int index = i + j * wwi.width;

            if (wwi.constraint[index]) {
                Pair window_pos = wwi.origin[index];
                Pair current_pos = wwi.position[index];
                Pair move = {window_pos.x - current_pos.x, window_pos.y - current_pos.y};
                Pair accel = {move.x * _stiffness, move.y * _stiffness};
                wwi.acceleration[index] = accel;
            } else {
                Pair &pos = wwi.position[index];
                neighbors[0] = wwi.position[index - 1];
                neighbors[1] = wwi.position[index + 1];
                neighbors[2] = wwi.position[index - wwi.width];
                neighbors[3] = wwi.position[index + wwi.width];

                acceleration.x = ((neighbors[0].x - pos.x) - x_length) * _stiffness +
                    (x_length - (pos.x - neighbors[1].x)) * _stiffness + (neighbors[2].x - pos.x) * _stiffness +
                    (neighbors[3].x - pos.x) * _stiffness;
                acceleration.y = (y_length - (pos.y - neighbors[2].y)) * _stiffness +
                    ((neighbors[3].y - pos.y) - y_length) * _stiffness + (neighbors[0].y - pos.y) * _stiffness +
                    (neighbors[1].y - pos.y) * _stiffness;

                acceleration.x /= 4;
                acceleration.y /= 4;

                wwi.acceleration[index] = acceleration;
            }
        }
    }

    HeightRingLinearMean(wwi.acceleration, wwi);

#if defined COMPUTE_STATS
    Pair accBound = {_maxAcceleration, _minAcceleration};
    Pair velBound = {_maxVelocity, _minVelocity};
#endif

    for (unsigned int i = 0; i < wwi.count; ++i) {
        Pair acc = wwi.acceleration[i];
        FixVectorBounds(acc, _minAcceleration, _maxAcceleration);

#if defined COMPUTE_STATS
        ComputeVectorBounds(acc, accBound);
#endif

        Pair &vel = wwi.velocity[i];
        vel.x = acc.x * time + vel.x * _drag;
        vel.y = acc.y * time + vel.y * _drag;

        acc_sum += fabs(acc.x) + fabs(acc.y);
    }

    HeightRingLinearMean(wwi.velocity, wwi);

    for (unsigned int i = 0; i < wwi.count; ++i) {
        Pair &pos = wwi.position[i];
        Pair &vel = wwi.velocity[i];

        FixVectorBounds(vel, _minVelocity, _maxVelocity);
#if defined COMPUTE_STATS
        ComputeVectorBounds(vel, velBound);
#endif

        pos.x += vel.x * time * _moveFactor;
        pos.y += vel.y * time * _moveFactor;

        vel_sum += fabs(vel.x) + fabs(vel.y);

#if defined VERBOSE_MODE
        if (wwi.constraint[i]) {
            qCDebug(KWIN_WOBBLYWINDOWS) << "Constraint point ** vel : " << vel.x << "," << vel.y
                                        << " ** move : " << vel.x * time << "," << vel.y * time;
        }
#endif
    }

    if (!wwi.canWobbleTop) {
        for (unsigned int i = 0; i < wwi.width; ++i) {
            for (unsigned j = 0; j < wwi.height - 1; ++j) {
                wwi.position[i + wwi.width * j].y = wwi.origin[i + wwi.width * j].y;
            }
        }
    }
    if (!wwi.canWobbleBottom) {
        for (unsigned int i = wwi.width * (wwi.height - 1); i < wwi.count; ++i) {
            for (unsigned j = 0; j < wwi.height - 1; ++j) {
                wwi.position[i - wwi.width * j].y = wwi.origin[i - wwi.width * j].y;
            }
        }
    }
    if (!wwi.canWobbleLeft) {
        for (unsigned int i = 0; i < wwi.count; i += wwi.width) {
            for (unsigned j = 0; j < wwi.width - 1; ++j) {
                wwi.position[i + j].x = wwi.origin[i + j].x;
            }
        }
    }
    if (!wwi.canWobbleRight) {
        for (unsigned int i = wwi.width - 1; i < wwi.count; i += wwi.width) {
            for (unsigned j = 0; j < wwi.width - 1; ++j) {
                wwi.position[i - j].x = wwi.origin[i - j].x;
            }
        }
    }

#if defined VERBOSE_MODE
#if defined COMPUTE_STATS
    qCDebug(KWIN_WOBBLYWINDOWS) << "Acceleration bounds (" << accBound.x << ", " << accBound.y << ")";
    qCDebug(KWIN_WOBBLYWINDOWS) << "Velocity bounds (" << velBound.x << ", " << velBound.y << ")";
#endif
    qCDebug(KWIN_WOBBLYWINDOWS) << "sum_acc : " << acc_sum << "  ***  sum_vel :" << vel_sum;
#endif

    const bool visualOppositePointFollowActive =
        std::hypot(wwi.visualOppositePointFollowOffset.x, wwi.visualOppositePointFollowOffset.y) >
            BetterWobblyWindows::OppositePointFollowStopPixels ||
        std::hypot(wwi.visualOppositePointFollowVelocity.x, wwi.visualOppositePointFollowVelocity.y) >
            BetterWobblyWindows::OppositePointFollowStopVelocity ||
        std::hypot(wwi.visualOppositePointPendingImpulse.x, wwi.visualOppositePointPendingImpulse.y) >
            BetterWobblyWindows::OppositePointFollowStopPixels;
    wwi.wobblying = !(acc_sum < _stopAcceleration && vel_sum < _stopVelocity) || visualOppositePointFollowActive;
    if (wwi.status != Moving && !wwi.wobblying) {
        // The blur hand-off below deliberately clears PAINT_WINDOW_TRANSFORMED
        // while the redirect is still alive. Snap the now-settled mesh to its
        // exact undeformed geometry first, otherwise the normal blur and the
        // offscreen window can differ by the small residual displacement allowed
        // by the physics stop thresholds.
        const Pair stopped = {0.0, 0.0};
        for (unsigned int i = 0; i < wwi.count; ++i) {
            wwi.position[i] = wwi.origin[i];
            wwi.velocity[i] = stopped;
            wwi.acceleration[i] = stopped;
        }

        // Do not tear down the redirected window on the exact frame the spring
        // settles. prePaintWindow() now reverses the blur transition and performs
        // normal cleanup when its opacity reaches zero. This leaves all physics
        // thresholds and integration behavior unchanged.
        if (_embeddedBlurEnabled && wwi.embeddedBlurTransitionProgress > 0.0) {
            setVertexSnappingMode(RenderGeometry::VertexSnappingMode::Round);
            // Stop this frame's integration loop; the window intentionally remains
            // tracked so prePaintWindow() can advance the visual fade.
            return false;
        }

        CleanupWindow(w);
        return false;
    } else if (!wwi.wobblying) {
        setVertexSnappingMode(RenderGeometry::VertexSnappingMode::Round);
    }

    return true;
}

void WobblyWindowsEffect::HeightRingLinearMean(QList<Pair> &data, WindowWobblyInfos &wwi)
{
    Pair neighbors[8];
    {
        Pair &res = wwi.buffer[0];
        Pair vit = data[0];
        neighbors[0] = data[1];
        neighbors[1] = data[wwi.width];
        neighbors[2] = data[wwi.width + 1];

        res.x = (neighbors[0].x + neighbors[1].x + neighbors[2].x + 3.0 * vit.x) / 6.0;
        res.y = (neighbors[0].y + neighbors[1].y + neighbors[2].y + 3.0 * vit.y) / 6.0;
    }
    {
        Pair &res = wwi.buffer[wwi.width - 1];
        Pair vit = data[wwi.width - 1];
        neighbors[0] = data[wwi.width - 2];
        neighbors[1] = data[2 * wwi.width - 1];
        neighbors[2] = data[2 * wwi.width - 2];

        res.x = (neighbors[0].x + neighbors[1].x + neighbors[2].x + 3.0 * vit.x) / 6.0;
        res.y = (neighbors[0].y + neighbors[1].y + neighbors[2].y + 3.0 * vit.y) / 6.0;
    }
    {
        Pair &res = wwi.buffer[wwi.width * (wwi.height - 1)];
        Pair vit = data[wwi.width * (wwi.height - 1)];
        neighbors[0] = data[wwi.width * (wwi.height - 1) + 1];
        neighbors[1] = data[wwi.width * (wwi.height - 2)];
        neighbors[2] = data[wwi.width * (wwi.height - 2) + 1];

        res.x = (neighbors[0].x + neighbors[1].x + neighbors[2].x + 3.0 * vit.x) / 6.0;
        res.y = (neighbors[0].y + neighbors[1].y + neighbors[2].y + 3.0 * vit.y) / 6.0;
    }
    {
        Pair &res = wwi.buffer[wwi.count - 1];
        Pair vit = data[wwi.count - 1];
        neighbors[0] = data[wwi.count - 2];
        neighbors[1] = data[wwi.width * (wwi.height - 1) - 1];
        neighbors[2] = data[wwi.width * (wwi.height - 1) - 2];

        res.x = (neighbors[0].x + neighbors[1].x + neighbors[2].x + 3.0 * vit.x) / 6.0;
        res.y = (neighbors[0].y + neighbors[1].y + neighbors[2].y + 3.0 * vit.y) / 6.0;
    }
    for (unsigned int i = 1; i < wwi.width - 1; ++i) {
        Pair &res = wwi.buffer[i];
        Pair vit = data[i];
        neighbors[0] = data[i - 1];
        neighbors[1] = data[i + 1];
        neighbors[2] = data[i + wwi.width];
        neighbors[3] = data[i + wwi.width - 1];
        neighbors[4] = data[i + wwi.width + 1];

        res.x =
            (neighbors[0].x + neighbors[1].x + neighbors[2].x + neighbors[3].x + neighbors[4].x + 5.0 * vit.x) / 10.0;
        res.y =
            (neighbors[0].y + neighbors[1].y + neighbors[2].y + neighbors[3].y + neighbors[4].y + 5.0 * vit.y) / 10.0;
    }
    for (unsigned int i = wwi.width * (wwi.height - 1) + 1; i < wwi.count - 1; ++i) {
        Pair &res = wwi.buffer[i];
        Pair vit = data[i];
        neighbors[0] = data[i - 1];
        neighbors[1] = data[i + 1];
        neighbors[2] = data[i - wwi.width];
        neighbors[3] = data[i - wwi.width - 1];
        neighbors[4] = data[i - wwi.width + 1];

        res.x =
            (neighbors[0].x + neighbors[1].x + neighbors[2].x + neighbors[3].x + neighbors[4].x + 5.0 * vit.x) / 10.0;
        res.y =
            (neighbors[0].y + neighbors[1].y + neighbors[2].y + neighbors[3].y + neighbors[4].y + 5.0 * vit.y) / 10.0;
    }
    for (unsigned int i = wwi.width; i < wwi.width * (wwi.height - 1); i += wwi.width) {
        Pair &res = wwi.buffer[i];
        Pair vit = data[i];
        neighbors[0] = data[i + 1];
        neighbors[1] = data[i - wwi.width];
        neighbors[2] = data[i + wwi.width];
        neighbors[3] = data[i - wwi.width + 1];
        neighbors[4] = data[i + wwi.width + 1];

        res.x =
            (neighbors[0].x + neighbors[1].x + neighbors[2].x + neighbors[3].x + neighbors[4].x + 5.0 * vit.x) / 10.0;
        res.y =
            (neighbors[0].y + neighbors[1].y + neighbors[2].y + neighbors[3].y + neighbors[4].y + 5.0 * vit.y) / 10.0;
    }
    for (unsigned int i = 2 * wwi.width - 1; i < wwi.count - 1; i += wwi.width) {
        Pair &res = wwi.buffer[i];
        Pair vit = data[i];
        neighbors[0] = data[i - 1];
        neighbors[1] = data[i - wwi.width];
        neighbors[2] = data[i + wwi.width];
        neighbors[3] = data[i - wwi.width - 1];
        neighbors[4] = data[i + wwi.width - 1];

        res.x =
            (neighbors[0].x + neighbors[1].x + neighbors[2].x + neighbors[3].x + neighbors[4].x + 5.0 * vit.x) / 10.0;
        res.y =
            (neighbors[0].y + neighbors[1].y + neighbors[2].y + neighbors[3].y + neighbors[4].y + 5.0 * vit.y) / 10.0;
    }
    for (unsigned int j = 1; j < wwi.height - 1; ++j) {
        for (unsigned int i = 1; i < wwi.width - 1; ++i) {
            unsigned int index = i + j * wwi.width;

            Pair &res = wwi.buffer[index];
            Pair &vit = data[index];
            neighbors[0] = data[index - 1];
            neighbors[1] = data[index + 1];
            neighbors[2] = data[index - wwi.width];
            neighbors[3] = data[index + wwi.width];
            neighbors[4] = data[index - wwi.width - 1];
            neighbors[5] = data[index - wwi.width + 1];
            neighbors[6] = data[index + wwi.width - 1];
            neighbors[7] = data[index + wwi.width + 1];

            res.x = (neighbors[0].x + neighbors[1].x + neighbors[2].x + neighbors[3].x + neighbors[4].x +
                        neighbors[5].x + neighbors[6].x + neighbors[7].x + 8.0 * vit.x) /
                16.0;
            res.y = (neighbors[0].y + neighbors[1].y + neighbors[2].y + neighbors[3].y + neighbors[4].y +
                        neighbors[5].y + neighbors[6].y + neighbors[7].y + 8.0 * vit.y) /
                16.0;
        }
    }

    data.swap(wwi.buffer);
}

} // namespace KWin
