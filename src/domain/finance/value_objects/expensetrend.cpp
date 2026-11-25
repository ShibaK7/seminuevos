#include "domain/finance/value_objects/expensetrend.h"

namespace domain {

std::optional<double> ExpenseTrend::percentChange(double current, double previous)
{
    // Los importes llegan redondeados al centavo: menos de medio centavo es
    // cero.
    constexpr double kHalfCent = 0.005;
    if (previous < kHalfCent)
        return current < kHalfCent ? std::optional<double>(0.0) : std::nullopt;
    return (current - previous) / previous * 100.0;
}

} // namespace domain
