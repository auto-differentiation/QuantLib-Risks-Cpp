/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003, 2004, 2008 StatPro Italia srl
 Copyright (C) 2022 Xcelerit

 This file is part of QuantLib / XAD integration module.
 It is modified from QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#pragma once

#include <ql/exercise.hpp>
#include <ql/functional.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/patterns/observable.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/test/unit_test.hpp>
#if BOOST_VERSION < 105900
#    include <boost/test/floating_point_comparison.hpp>
#else
#    include <boost/test/tools/floating_point_comparison.hpp>
#endif
#include <cmath>
#include <iomanip>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

// This adapts the BOOST_CHECK_SMALL and BOOST_CHECK_CLOSE macros to
// support a struct as Real for arguments, while fully transparant to regular doubles.
// Unfortunately boost does not provide a portable way to customize these macros' behaviour,
// so we need to define wrapper macros QL_CHECK_SMALL etc.
//
// It is required to have a function `value` defined that returns the double-value
// of the Real type (or a value function in the Real type's namespace for ADT).

namespace QuantLib {
    // overload this function in case Real is something different - it should alway return double
    inline double value(double x) {
        return x;
    }
}

using QuantLib::value;

#define QL_CHECK_SMALL(FPV, T) BOOST_CHECK_SMALL(value(FPV), value(T))
#define QL_CHECK_CLOSE(L, R, T) BOOST_CHECK_CLOSE(value(L), value(R), value(T))


// This makes it easier to use array literals (for new code, use std::vector though)
#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

// make test names pretty before registering them
#define QLRISKS_TEST_CASE(f)                                        \
    BOOST_TEST_CASE_NAME(QuantLib::detail::quantlib_test_case(f), \
                         QuantLib::detail::extract_test_name(BOOST_STRINGIZE(f)))

namespace QuantLib {

    namespace detail {

        inline std::string extract_test_name(const std::string& name) {
            auto pos = name.rfind("::");
            std::string n = (pos == std::string::npos) ? name : name.substr(pos + 2);
            // if we had a lambda, there are likely semicolons - let's read until that point
            pos = n.find(';');
            std::string n2 = (pos == std::string::npos) ? n : n.substr(0, pos);
            // replace "," that happen due to bind expressions in test registration - these are not
            // command-line friendly
            boost::algorithm::replace_all(n2, ",", "_");
            return n2;
        }


        // used to avoid no-assertion messages in Boost 1.35
        class quantlib_test_case {
            std::function<void()> test_;

          public:
            template <class F>
            explicit quantlib_test_case(F test) : test_(test) {}
            void operator()() const {
                Date before = Settings::instance().evaluationDate();
                BOOST_CHECK(true);
                test_();
                Date after = Settings::instance().evaluationDate();
                if (before != after)
                    BOOST_ERROR("Evaluation date not reset"
                                << "\n  before: " << before << "\n  after:  " << after);
            }
#if BOOST_VERSION <= 105300
            // defined to avoid unused-variable warnings. It doesn't
            // work after Boost 1.53 because the functions were
            // overloaded and the address can't be resolved.
            void _use_check(const void* = &boost::test_tools::check_is_close,
                            const void* = &boost::test_tools::check_is_small) const {}
#endif
        };

    }

    std::string payoffTypeToString(const ext::shared_ptr<Payoff>&);
    std::string exerciseTypeToString(const ext::shared_ptr<Exercise>&);


    ext::shared_ptr<YieldTermStructure>
    flatRate(const Date& today, const ext::shared_ptr<Quote>& forward, const DayCounter& dc);

    ext::shared_ptr<YieldTermStructure>
    flatRate(const Date& today, Rate forward, const DayCounter& dc);

    ext::shared_ptr<YieldTermStructure> flatRate(const ext::shared_ptr<Quote>& forward,
                                                 const DayCounter& dc);

    ext::shared_ptr<YieldTermStructure> flatRate(Rate forward, const DayCounter& dc);


    ext::shared_ptr<BlackVolTermStructure>
    flatVol(const Date& today, const ext::shared_ptr<Quote>& volatility, const DayCounter& dc);

    ext::shared_ptr<BlackVolTermStructure>
    flatVol(const Date& today, Volatility volatility, const DayCounter& dc);

    ext::shared_ptr<BlackVolTermStructure> flatVol(const ext::shared_ptr<Quote>& volatility,
                                                   const DayCounter& dc);

    ext::shared_ptr<BlackVolTermStructure> flatVol(Volatility volatility, const DayCounter& dc);


    Real relativeError(Real x1, Real x2, Real reference);

    // bool checkAbsError(Real x1, Real x2, Real tolerance){
    //     return std::fabs(x1 - x2) < tolerance;
    // };

    class Flag : public QuantLib::Observer {
      private:
        bool up_ = false;

      public:
        Flag() = default;
        void raise() { up_ = true; }
        void lower() { up_ = false; }
        bool isUp() const { return up_; }
        void update() override { raise(); }
    };

    template <class Iterator>
    Real norm(const Iterator& begin, const Iterator& end, Real h) {
        // squared values
        std::vector<Real> f2(end - begin);
        std::transform(begin, end, begin, f2.begin(), std::multiplies<>());
        // numeric integral of f^2
        Real I = h * (std::accumulate(f2.begin(), f2.end(), Real(0.0)) - 0.5 * f2.front() -
                      0.5 * f2.back());
        return std::sqrt(I);
    }


    inline Integer timeToDays(Time t, Integer daysPerYear = 360) {
        return Integer(std::lround(t * daysPerYear));
    }


    // this cleans up index-fixing histories when destroyed
    class IndexHistoryCleaner {
      public:
        IndexHistoryCleaner() = default;
        ~IndexHistoryCleaner();
    };


    // Allow streaming vectors to error messages.

    // The standard forbids defining new overloads in the std
    // namespace, so we have to use a wrapper instead of overloading
    // operator<< to send a vector to the stream directly.
    // Defining the overload outside the std namespace wouldn't work
    // with Boost streams because of ADT name lookup rules.

    template <class T>
    struct vector_streamer {
        explicit vector_streamer(std::vector<T> v) : v(std::move(v)) {}
        std::vector<T> v;
    };

    template <class T>
    vector_streamer<T> to_stream(const std::vector<T>& v) {
        return vector_streamer<T>(v);
    }

    template <class T>
    std::ostream& operator<<(std::ostream& out, const vector_streamer<T>& s) {
        out << "{ ";
        if (!s.v.empty()) {
            for (size_t n = 0; n < s.v.size() - 1; ++n)
                out << s.v[n] << ", ";
            out << s.v.back();
        }
        out << " }";
        return out;
    }


}