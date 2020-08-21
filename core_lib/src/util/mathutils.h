#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <QPoint>
#include <QtMath>

namespace MathUtils
{
/** Get the angle from the difference vector a->b to the x-axis.
 *
 * \param a Start point of vector
 * \param b End point of vector
 * \return Angle in radians from [-pi,+pi]
 */
inline qreal getDifferenceAngle(const QPointF a, const QPointF b)
{
    return qAtan2(b.y() - a.y(), b.x() - a.x());
}
} // namespace MathUtils

#endif // MATHUTILS_H
