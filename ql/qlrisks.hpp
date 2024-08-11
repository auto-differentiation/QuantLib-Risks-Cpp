/*******************************************************************************

   This file is part of QuantLib-Risks, an adaptor module to enable using XAD with
   QuantLib. XAD is a fast and comprehensive C++ library for
   automatic differentiation.

   Copyright (C) 2010-2024 Xcelerit Computing Ltd.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as published
   by the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

******************************************************************************/

#pragma once

#include <boost/accumulators/numeric/functional.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>
#include <boost/math/special_functions/math_fwd.hpp>
#include <boost/math/tools/promotion.hpp>
#include <boost/math/tools/rational.hpp>
#include <boost/numeric/ublas/operations.hpp>
#include <boost/type_traits/is_arithmetic.hpp>
#include <boost/type_traits/is_floating_point.hpp>
#include <boost/type_traits/is_pod.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <XAD/XAD.hpp>
#include <XAD/Complex.hpp>
#include <XAD/StdCompatibility.hpp>
#include <limits>
#include <type_traits>

#define QL_REAL xad::AReal<double>
#define QL_RISKS 1

// QuantLib specialisations to work with expressions
namespace QuantLib {

    // from ql/functional.hpp
    template <class T>
    T squared(T);

    // for binary expressions
    template <class Op, class Expr1, class Expr2>
    typename xad::AReal<double> squared(const xad::BinaryExpr<double, Op, Expr1, Expr2>& x) {
        return squared(xad::AReal<double>(x));
    }

    // for unary expressions
    template <class Op, class Expr>
    typename xad::AReal<double> squared(const xad::UnaryExpr<double, Op, Expr>& x) {
        return squared(xad::AReal<double>(x));
    }
}

// boost specialisations
namespace boost {

    template <class Target, class Op, class Expr> 
    inline Target numeric_cast(const xad::UnaryExpr<double, Op, Expr>& arg) {
        return numeric_cast<Target>(value(arg));
    }

    template <class Target, class Op, class Expr1, class Expr2> 
    inline Target numeric_cast(const xad::BinaryExpr<double, Op, Expr1, Expr2>& arg) {
        return numeric_cast<Target>(value(arg));
    }

    namespace math {

        // full specialisations for promoting 2 types where one of them is AReal<double>,
        // used by boost math functions a lot
        namespace tools {
            template <>
            struct promote_args_2<xad::AReal<double>, xad::AReal<double>> {
                typedef xad::AReal<double> type;
            };

            template <class T>
            struct promote_args_2<xad::AReal<double>, T> {
                typedef xad::AReal<double> type;
            };

            template <class T>
            struct promote_args_2<T, xad::AReal<double>> {
                typedef xad::AReal<double> type;
            };
        }

        // Propagating policies for boost math involving AReal
        namespace policies {
            template <class Policy>
            struct evaluation<xad::AReal<double>, Policy> {
                using type = xad::AReal<double>;
            };

            template <class Policy, class Op, class Expr1, class Expr2>
            struct evaluation<xad::BinaryExpr<double, Op, Expr1, Expr2>, Policy> {
                using type = typename evaluation<xad::AReal<double>, Policy>::type;
            };

            template <class Policy, class Op, class Expr>
            struct evaluation<xad::UnaryExpr<double, Op, Expr>, Policy> {
                using type = typename evaluation<xad::AReal<double>, Policy>::type;
            };
        }

        /* specialised version of boost/math/special_functions/erfc for XAD expressions,
         *  casting the argument type to the underlying value-type and calling the boost original.
         */
        template <class Op, class Expr, class Policy>
        inline xad::AReal<double> erfc(xad::UnaryExpr<double, Op, Expr> z, const Policy& pol) {
            return boost::math::erfc(xad::AReal<double>(z), pol);
        }

        template <class Op, class Expr, class Policy>
        xad::AReal<double> erfc_inv(xad::UnaryExpr<double, Op, Expr> z, const Policy& pol) {
            return boost::math::erfc_inv(xad::AReal<double>(z), pol);
        }

        namespace tools {
            // boost/tools/rational.hpp, as it's called from erfc with expressions since boost 1.83

            template <std::size_t N, class T, class Op, class Expr1, class Expr2>
            xad::AReal<double> evaluate_polynomial(const T (&a)[N],
                                                   xad::BinaryExpr<double, Op, Expr1, Expr2> val) {
                return evaluate_polynomial(a, xad::AReal<double>(val));
            }
        }


        template <class RT1, class Op, class Expr, class RT3, class Policy>
        inline xad::AReal<double>
        ibetac(RT1 a, xad::UnaryExpr<double, Op, Expr> b, RT3 x, const Policy& pol) {
            return boost::math::ibetac(xad::AReal<double>(a), xad::AReal<double>(b),
                                       xad::AReal<double>(x), pol);
        }

        template <class RT1, class RT2, class Op, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> ibeta_derivative(RT1 a,
                                                   RT2 b,
                                                   xad::BinaryExpr<double, Op, Expr1, Expr2> x,
                                                   const Policy& pol) {
            return boost::math::ibeta_derivative(xad::AReal<double>(a), xad::AReal<double>(b),
                                                 xad::AReal<double>(x), pol);
        }

        template <class Op, class Expr, class RT2, class RT3, class Policy>
        inline xad::AReal<double>
        ibeta(xad::UnaryExpr<double, Op, Expr> a, RT2 b, RT3 x, const Policy& pol) {
            return boost::math::ibeta(xad::AReal<double>(a), xad::AReal<double>(b),
                                      xad::AReal<double>(x), pol);
        }

        template <class Op, class Expr, class T2, class Op2, class Expr2, class T4, class Policy>
        inline xad::AReal<double> ibeta_inv(xad::UnaryExpr<double, Op, Expr> a,
                                            T2 b,
                                            xad::UnaryExpr<double, Op2, Expr2> p,
                                            T4* py,
                                            const Policy& pol) {
            return boost::math::ibeta_inv(xad::AReal<double>(a), xad::AReal<double>(b),
                                          xad::AReal<double>(p), py, pol);
        }

        template <class Op, class Expr, class RT2, class A>
        inline xad::AReal<double> beta(xad::UnaryExpr<double, Op, Expr> a, RT2 b, A arg) {
            return boost::math::beta(xad::AReal<double>(a), xad::AReal<double>(b), arg);
        }

        template <class Op, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> log1p(xad::BinaryExpr<double, Op, Expr1, Expr2> x,
                                        const Policy& pol) {
            return boost::math::log1p(xad::AReal<double>(x), pol);
        }

        template <class Op, class Expr, class Policy>
        inline xad::AReal<double> log1p(xad::UnaryExpr<double, Op, Expr> x, const Policy& pol) {
            return boost::math::log1p(xad::AReal<double>(x), pol);
        }

        template <class Op, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> tgamma(xad::BinaryExpr<double, Op, Expr1, Expr2> z,
                                         const Policy& pol) {
            return boost::math::tgamma(xad::AReal<double>(z), pol);
        }

        template <class Op, class Expr, class Policy>
        inline xad::AReal<double> tgamma(xad::UnaryExpr<double, Op, Expr> z, const Policy& pol) {
            return boost::math::tgamma(xad::AReal<double>(z), pol);
        }

        template <class Op, class Expr, class T2, class Policy>
        inline xad::AReal<double>
        tgamma_delta_ratio(xad::UnaryExpr<double, Op, Expr> z, T2 delta, const Policy& pol) {
            return boost::math::tgamma_delta_ratio(xad::AReal<double>(z), xad::AReal<double>(delta),
                                                   pol);
        }

        template <class Op, class Expr, class T2, class Policy>
        inline xad::AReal<double>
        gamma_q_inv(xad::UnaryExpr<double, Op, Expr> a, T2 p, const Policy& pol) {
            return boost::math::gamma_q_inv(xad::AReal<double>(a), xad::AReal<double>(p), pol);
        }

        template <class Op, class Expr, class T2, class Policy>
        inline xad::AReal<double>
        gamma_p_inv(xad::UnaryExpr<double, Op, Expr> a, T2 p, const Policy& pol) {
            return boost::math::gamma_p_inv(xad::AReal<double>(a), xad::AReal<double>(p), pol);
        }

        template <class Op, class Expr>
        inline xad::AReal<double> trunc(const xad::UnaryExpr<double, Op, Expr>& v) {
            return boost::math::trunc(xad::AReal<double>(v));
        }

        template <class Op, class Expr1, class Expr2>
        inline xad::AReal<double> trunc(const xad::BinaryExpr<double, Op, Expr1, Expr2>& v) {
            return boost::math::trunc(xad::AReal<double>(v));
        }

        template <class Op, class Expr>
        inline long_long_type lltrunc(const xad::UnaryExpr<double, Op, Expr>& v) {
            return boost::math::lltrunc(xad::value(v));
        }

        template <class Op, class Expr1, class Expr2>
        inline long_long_type lltrunc(const xad::BinaryExpr<double, Op, Expr1, Expr2>& v) {
            return boost::math::lltrunc(xad::value(v));
        }

        inline long_long_type lltrunc(const xad::AReal<double>& v) {
            return boost::math::lltrunc(xad::value(v));
        }

        template <class Policy>
        inline long_long_type llround(const xad::AReal<double>& v, const Policy& p) {
            return boost::math::llround(xad::value(v), p);
        }

        template <class Op, class Expr1, class Expr2>
        inline int itrunc(const xad::BinaryExpr<double, Op, Expr1, Expr2>& v) {
            return itrunc(xad::value(v), policies::policy<>());
        }

        template <class Op, class Expr>
        inline int itrunc(const xad::UnaryExpr<double, Op, Expr>& v) {
            return itrunc(xad::value(v), policies::policy<>());
        }

        template <class Op, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> expm1(xad::BinaryExpr<double, Op, Expr1, Expr2> x,
                                        const Policy& pol) {
            return boost::math::expm1(xad::AReal<double>(x), pol);
        }

        template <class Op1, class Op2, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> gamma_p(xad::UnaryExpr<double, Op1, Expr1> a,
                                          xad::UnaryExpr<double, Op2, Expr2> z,
                                          const Policy& pol) {
            return boost::math::gamma_p(xad::AReal<double>(a), xad::AReal<double>(z), pol);
        }

        template <class Op1, class Expr1, class Op2, class Expr2, class Expr3>
        inline xad::AReal<double> gamma_p(xad::UnaryExpr<double, Op1, Expr1> a,
                                          xad::BinaryExpr<double, Op2, Expr2, Expr3> z) {
            return boost::math::gamma_p(xad::AReal<double>(a), xad::AReal<double>(z),
                                        policies::policy<>());
        }

        template <class Op1, class Op2, class Expr, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> gamma_p(xad::UnaryExpr<double, Op1, Expr> a,
                                          xad::BinaryExpr<double, Op2, Expr1, Expr2> z,
                                          const Policy& pol) {
            return boost::math::gamma_p(xad::AReal<double>(a), xad::AReal<double>(z), pol);
        }

        inline int fpclassify BOOST_NO_MACRO_EXPAND(const xad::AReal<double>& t) {
            return (boost::math::fpclassify)(xad::value(t));
        }

        template <class Op1, class Op2, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> gamma_p_derivative(xad::UnaryExpr<double, Op1, Expr1> a,
                                                     xad::UnaryExpr<double, Op2, Expr2> x,
                                                     const Policy&) {
            return boost::math::gamma_p_derivative(xad::AReal<double>(a), xad::AReal<double>(x),
                                                   policies::policy<>());
        }

        template <class Op1, class Op2, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> gamma_q(xad::UnaryExpr<double, Op1, Expr1> a,
                                          xad::UnaryExpr<double, Op2, Expr2> x,
                                          const Policy& pol) {
            return boost::math::gamma_q(xad::AReal<double>(a), xad::AReal<double>(x), pol);
        }

        template <class Op, class Expr, class T2, class Policy>
        inline xad::AReal<double>
        gamma_q(xad::UnaryExpr<double, Op, Expr> a, T2 z, const Policy& pol) {
            return boost::math::gamma_q(xad::AReal<double>(a), z, pol);
        }

        template <class Op, class Expr, class T2, class Policy>
        inline xad::AReal<double>
        gamma_p_derivative(xad::UnaryExpr<double, Op, Expr> a, T2 x, const Policy& pol) {
            return boost::math::gamma_p_derivative(xad::AReal<double>(a), xad::AReal<double>(x),
                                                   pol);
        }

        template <class Policy>
        inline int itrunc(const xad::AReal<double>& v, const Policy& pol) {
            return boost::math::itrunc(xad::value(v), pol);
        }

        template <class Policy>
        inline int iround(const xad::AReal<double>& v, const Policy& pol) {
            return boost::math::iround(xad::value(v), pol);
        }

        template <class Op, class Expr1, class Expr2>
        inline xad::AReal<double> expm1(xad::BinaryExpr<double, Op, Expr1, Expr2> x) {
            return expm1(xad::AReal<double>(x), policies::policy<>());
        }

        template <class Op1, class Op2, class Expr1, class Expr2, class Policy>
        inline typename detail::bessel_traits<xad::AReal<double>, xad::AReal<double>, Policy>::
            result_type
            cyl_bessel_i(xad::UnaryExpr<double, Op1, Expr1> v,
                         xad::UnaryExpr<double, Op2, Expr2> x,
                         const Policy&) {
            return boost::math::cyl_bessel_i(xad::AReal<double>(v), xad::AReal<double>(x),
                                             policies::policy<>());
        }

        template <class Op, class Expr>
        inline xad::AReal<double> lgamma(xad::UnaryExpr<double, Op, Expr> z, int* sign) {
            return boost::math::lgamma(xad::AReal<double>(z), sign);
        }

        template <class Op, class Expr1, class Expr2>
        inline xad::AReal<double> lgamma(xad::BinaryExpr<double, Op, Expr1, Expr2> z, int* sign) {
            return boost::math::lgamma(xad::AReal<double>(z), sign);
        }

        template <class Op, class Expr, class Policy>
        inline xad::AReal<double> lgamma(xad::UnaryExpr<double, Op, Expr> x, const Policy& pol) {
            return boost::math::lgamma(xad::AReal<double>(x), pol);
        }
        template <class Op, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> lgamma(xad::BinaryExpr<double, Op, Expr1, Expr2> x,
                                         const Policy& pol) {
            return boost::math::lgamma(xad::AReal<double>(x), pol);
        }

        template <class Op, class Expr, class Policy>
        inline xad::AReal<double> tgamma1pm1(xad::UnaryExpr<double, Op, Expr> z,
                                             const Policy& pol) {
            return boost::math::tgamma1pm1(xad::AReal<double>(z), pol);
        }

        inline bool(isfinite)(const xad::AReal<double>& x) {
            return (boost::math::isfinite)(xad::value(x));
        }
        inline bool(isinf)(const xad::AReal<double>& x) {
            return (boost::math::isinf)(xad::value(x));
        }

        template <class Op, class Expr1, class Expr2, class Policy>
        inline xad::AReal<double> powm1(xad::BinaryExpr<double, Op, Expr1, Expr2> a,
                                        const xad::AReal<double>& z,
                                        const Policy& pol) {
            return boost::math::powm1(xad::AReal<double>(a), z, pol);
        }
    }

    namespace numeric {

        // static integer power implementations with XAD expressions - evaluate first and call
        // underlying
        template <class Op, class Expr, int N>
        xad::AReal<double> pow(xad::UnaryExpr<double, Op, Expr> const& x, mpl::int_<N>) {
            return pow(xad::AReal<double>(x), N);
        }

        template <class Op, class Expr1, class Expr2, int N>
        xad::AReal<double> pow(xad::BinaryExpr<double, Op, Expr1, Expr2> const& x, mpl::int_<N>) {
            return pow(xad::AReal<double>(x), N);
        }

        // override boost accumulators traits to determine result types for AReal
        // (only divides and multiplies are used from QuantLib)
        namespace functional {

            template <>
            struct result_of_divides<xad::AReal<double>, xad::AReal<double> > {
                typedef xad::AReal<double> type;
            };

            template <class T>
            struct result_of_divides<xad::AReal<double>, T> {
                typedef xad::AReal<double> type;
            };

            template <class T>
            struct result_of_divides<T, xad::AReal<double> > {
                typedef xad::AReal<double> type;
            };

            template <>
            struct result_of_multiplies<xad::AReal<double>, xad::AReal<double> > {
                typedef xad::AReal<double> type;
            };

            template <class T>
            struct result_of_multiplies<xad::AReal<double>, T> {
                typedef xad::AReal<double> type;
            };

            template <class T>
            struct result_of_multiplies<T, xad::AReal<double> > {
                typedef xad::AReal<double> type;
            };

        }

        // traits for ublas type promotion for operands of XAD type
        namespace ublas {
            // AReal x AReal
            template <>
            struct promote_traits<xad::AReal<double>, xad::AReal<double> > {
                typedef xad::AReal<double> promote_type;
            };

            // AReal x T
            template <class T>
            struct promote_traits<xad::AReal<double>, T> {
                typedef xad::AReal<double> promote_type;
            };

            // T x AReal
            template <class T>
            struct promote_traits<T, xad::AReal<double> > {
                typedef xad::AReal<double> promote_type;
            };

        }
    }

    // AReal behaves like a floating point number
    template <>
    struct is_floating_point<xad::AReal<double> > : public true_type {};

    // AReal is arithmetic
    template <>
    struct is_arithmetic<xad::AReal<double> > : public true_type {};

    // AReal is not a POD type though
    template <>
    struct is_pod<xad::AReal<double> > : public false_type {};

    // AReal is only convertible to itself, not to another type
    template <class To>
    struct is_convertible<xad::AReal<double>, To> : public false_type {};

    template <>
    struct is_convertible<xad::AReal<double>, xad::AReal<double> > : public true_type {};
}


// MSVC specialisations / fixes
#ifdef _MSC_VER

#include <random>

// required for random - as it calls ::sqrt on arguments
using xad::sqrt;
using xad::pow;
using xad::exp;
using xad::log;
using xad::tan;

namespace std {

    // this is needed to avoid std::random to revert to a binary / constexpr
    // implementation for Real in the random generator
    template <>
    struct _Has_static_min_max<std::mt19937, void> : false_type {};
}

#endif

// Mac specialisations / fixes
#ifdef __APPLE__

// Mac uses an internal namespace _VSTD for its math functions, which are called from
// random header with full namespace qualification.
// We therefore need to import the xad math functions into that to make it work

namespace std {
    inline namespace _LIBCPP_ABI_NAMESPACE {
        using xad::sqrt;
        using xad::pow;
        using xad::log;
        using xad::tan;
    }
}

// have to include this last, to make sure functions are in the right namespace before
#include <random>

#endif
