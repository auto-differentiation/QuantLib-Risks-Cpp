/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004, 2005, 2006, 2007 Ferdinando Ametrano
 Copyright (C) 2004, 2005, 2006, 2007, 2008 StatPro Italia srl
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

#include <ql/settings.hpp>
#include <ql/types.hpp>
#include <ql/version.hpp>
#include <boost/test/included/unit_test.hpp>

/* Use BOOST_MSVC instead of _MSC_VER since some other vendors (Metrowerks,
   for example) also #define _MSC_VER
*/
#if !defined(BOOST_ALL_NO_LIB) && defined(BOOST_MSVC)
#    include <ql/auto_link.hpp>
#endif

#include "americanoption_xad.hpp"
#include "barrieroption_xad.hpp"
#include "batesmodel_xad.hpp"
#include "bermudanswaption_xad.hpp"
#include "bonds_xad.hpp"
#include "creditdefaultswap_xad.hpp"
#include "europeanoption_xad.hpp"
#include "forwardrateagreement_xad.hpp"
#include "hestonmodel_xad.hpp"
#include "swap_xad.hpp"
#include "utilities_xad.hpp"

using namespace boost::unit_test_framework;

namespace {

    QuantLib::Date evaluation_date(int, char**) {
        return QuantLib::Date(16, QuantLib::September, 2015);
    }

    void configure(QuantLib::Date evaluationDate) {
        QuantLib::Settings::instance().evaluationDate() = evaluationDate;
    }
}

test_suite* init_unit_test_suite(int, char*[]) {

    int argc = boost::unit_test::framework::master_test_suite().argc;
    char** argv = boost::unit_test::framework::master_test_suite().argv;
    configure(evaluation_date(argc, argv));

    auto* test = BOOST_TEST_SUITE("QuantLib XAD test suite");

    test->add(AmericanOptionXadTest::suite());
    test->add(EuropeanOptionXadTest::suite());
    test->add(BarrierOptionXadTest::suite());
    test->add(BatesModelXadTest::suite());
    test->add(SwapXadTest::suite());
    test->add(CreditDefaultSwapXadTest::suite());
    test->add(ForwardRateAgreementXadTest::suite());
    test->add(BondsXadTest::suite());
    test->add(BermudanSwaptionXadTest::suite());
    test->add(HestonModelXadTest::suite());


    return test;
}