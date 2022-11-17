/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2022 Xcelerit
 
 This example is based on code by Peter Caspers.

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


#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/treeswaptionengine.hpp>
#include <ql/pricingengines/swaption/jamshidianswaptionengine.hpp>
#include <ql/pricingengines/swaption/g2swaptionengine.hpp>
#include <ql/pricingengines/swaption/fdhullwhiteswaptionengine.hpp>
#include <ql/pricingengines/swaption/fdg2swaptionengine.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/models/shortrate/onefactormodels/blackkarasinski.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/utilities/dataformatters.hpp>


#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/termstructures/iterativebootstrap.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolcube1.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>


using namespace QuantLib;

// create tape
using tape_type = Real::tape_type;
tape_type tape;

Handle<YieldTermStructure> bootstrapYields(const Date& refDate) {
	Handle<Quote> swapQuote(ext::make_shared<SimpleQuote>(0.03));
	auto euribor6mBt = ext::make_shared<Euribor>(6 * Months);
	std::vector<ext::shared_ptr<RateHelper>> helpers;
	for (Size i = 1; i <= 30; ++i) {
		helpers.push_back(
			ext::make_shared<SwapRateHelper>(
				swapQuote, i * Years, TARGET(), Annual, ModifiedFollowing,
				Thirty360(Thirty360::European), euribor6mBt)
		);
	}
	auto yts6m =
		ext::make_shared<PiecewiseYieldCurve<ZeroYield, Linear,
		IterativeBootstrap> >(
			refDate, helpers, Actual365Fixed());

	Handle<YieldTermStructure> yts6m_h(yts6m);

	yts6m_h->enableExtrapolation();

	return yts6m_h;
}

void prepareTenors(std::vector<Period>& optionTenors, std::vector<Period>& swapTenors)
{
	optionTenors = {
			3 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years,
			4 * Years, 5 * Years, 6 * Years, 7 * Years, 8 * Years, 9 * Years,
			10 * Years, 12 * Years, 15 * Years, 20 * Years, 30 * Years
	};
	swapTenors = {
		1 * Years, 2 * Years, 3 * Years, 4 * Years, 5 * Years,
		6 * Years, 7 * Years, 8 * Years, 9 * Years, 10 * Years, 15 * Years,
		20 * Years, 25 * Years, 30 * Years
	};
}

Handle<SwaptionVolatilityStructure> swaptionVolatilities(const std::vector<Period>& optionTenors, const std::vector<Period>& swapTenors) {

	// atm vol structure
	Matrix atmVols(optionTenors.size(), swapTenors.size(), 0.20);
	ext::shared_ptr<SwaptionVolatilityMatrix> swatm =
		ext::make_shared<SwaptionVolatilityMatrix>(
			TARGET(), ModifiedFollowing, optionTenors, swapTenors, atmVols,
			Actual365Fixed());
	Handle<SwaptionVolatilityStructure> swatm_h(swatm);

	return swatm_h;
}

void getSabrParams(std::vector<Real>& strikeSpreads,
	std::vector<std::vector<Handle<Quote> > >& volSpreads,
	std::vector<std::vector<Handle<Quote> > >& sabrParams,
	std::vector<bool>& paramFixed,
	Size NoptTenors, Size NswapTenors) {

	// we assume we know the SABR parameters
	strikeSpreads.push_back(.0);

	for (Size i = 0; i < NoptTenors * NswapTenors; ++i) {
		// spreaded vols
		std::vector<Handle<Quote>> volTmp(1, Handle<Quote>(ext::make_shared<SimpleQuote>(0.0)));
		volSpreads.push_back(std::move(volTmp));
		// sabr parameters
		std::vector<Handle<Quote>> sabrTmp = {
			Handle<Quote>(ext::make_shared<SimpleQuote>(0.03)), // alpha
			Handle<Quote>(ext::make_shared<SimpleQuote>(0.60)), // beta
			Handle<Quote>(ext::make_shared<SimpleQuote>(0.12)), // nu
			Handle<Quote>(ext::make_shared<SimpleQuote>(0.30)), // rho
		};
		sabrParams.push_back(std::move(sabrTmp));
	}
	paramFixed = { true, true, true, true };
}

Real priceSwaption(std::vector<Real>& inputVol, const Schedule& fixedSchedule,
	const Schedule& floatingSchedule, Real strike, const std::vector<Date>& exerciseDates,
	ext::shared_ptr<Euribor> euribor6m, Handle<YieldTermStructure> yts6m_h, Date refDate) {

	auto underlying =
		ext::make_shared<VanillaSwap>(
			VanillaSwap::Payer, 1.0, fixedSchedule, strike,
			Thirty360(Thirty360::European), floatingSchedule, euribor6m, 0.0, Actual360());
	auto exercise = ext::make_shared<BermudanExercise>(exerciseDates, false);

	// the GSR model
	std::vector<Date> stepDates(exerciseDates.begin(), exerciseDates.end() - 1);
	std::vector<Real> sigmas(stepDates.size() + 1, 0.01);
	Real reversion = 0.01;
	auto gsr = ext::make_shared<Gsr>(yts6m_h, stepDates, sigmas, reversion);


	std::vector<ext::shared_ptr<BlackCalibrationHelper>> basket;
	auto swaptionEngine = ext::make_shared<Gaussian1dSwaptionEngine>(gsr, 32, 5.0);

	for (Size i = 1; i < 10; ++i) {
		Period swapLength = (10 - i) * Years;
		Handle<Quote> vol_q(ext::make_shared<SimpleQuote>(inputVol[i - 1]));
		auto tmp = ext::make_shared<SwaptionHelper>(
			exerciseDates[i - 1], swapLength, vol_q, euribor6m,
			1 * Years, Thirty360(Thirty360::European), Actual360(), yts6m_h,
			SwaptionHelper::RelativePriceError, strike, 1.0);
		tmp->setPricingEngine(swaptionEngine);
		basket.push_back(tmp);
	}

	// calibrate the model
	LevenbergMarquardt method;
	EndCriteria ec(1000, 10, 1E-8, 1E-8, 1E-8);

	gsr->calibrateVolatilitiesIterative(basket, method, ec);
	auto volatility = gsr->volatility();

	auto swaption = ext::make_shared<Swaption>(underlying, exercise);
	swaption->setPricingEngine(swaptionEngine);
	return swaption->NPV();
}

Real priceWithSensi(std::vector<Real>& inputVol, const Schedule& fixedSchedule,
	const Schedule& floatingSchedule, Real strike, const std::vector<Date>& exerciseDates,
	ext::shared_ptr<Euribor> euribor6m, Handle<YieldTermStructure> yts6m_h, Date refDate,
	std::vector<Real>& gradient) {

	tape.clearAll();
	tape.registerInputs(inputVol);
	tape.newRecording();

	Real value = priceSwaption(inputVol, fixedSchedule, floatingSchedule, strike, exerciseDates, euribor6m, yts6m_h, refDate);

	// get the vegas
	tape.registerOutput(value);
	derivative(value) = 1.0;
	tape.computeAdjoints();

	gradient.resize(inputVol.size());
	std::transform(inputVol.begin(), inputVol.end(), gradient.begin(), [](const Real& vol) { return derivative(vol);  });

	return value;
}

void printResults(Real v, const std::vector<Real>& gradient) {
	
	std::cout.precision(6);
	std::cout << std::fixed;
	std::cout.imbue(std::locale(""));

	std::cout << "\nSensitivities w.r.t. volatilities:\n";
	for (std::size_t i = 0; i < gradient.size(); ++i)
		std::cout << "Vega #" << i << " = " << gradient[i] << "\n";

	std::cout << std::endl;
}

int main() {

	try {
		Date refDate(13, April, 2015);
		Settings::instance().evaluationDate() = refDate;

		Handle<YieldTermStructure> yts6m_h = bootstrapYields(refDate);
		auto euribor6m = ext::make_shared<Euribor>(6 * Months, yts6m_h);
		std::vector<Period> optionTenors, swapTenors;
		prepareTenors(optionTenors, swapTenors);
		Handle<SwaptionVolatilityStructure> swatm_h = swaptionVolatilities(optionTenors, swapTenors);
		std::vector<Real> strikeSpreads;
		std::vector<std::vector<Handle<Quote> > > volSpreads,
			sabrParams;
		std::vector<bool> paramFixed;
		getSabrParams(strikeSpreads, volSpreads, sabrParams, paramFixed, optionTenors.size(), swapTenors.size());

		auto indexBase = ext::make_shared<EuriborSwapIsdaFixA>(30 * Years, yts6m_h);
		auto indexBaseShort = ext::make_shared<EuriborSwapIsdaFixA>(2 * Years, yts6m_h);
		auto swvol = ext::make_shared<SwaptionVolCube1>(
			swatm_h, optionTenors, swapTenors, strikeSpreads, volSpreads,
			indexBase, indexBaseShort, true, sabrParams, paramFixed, true,
			boost::shared_ptr<EndCriteria>(), 100.0);
		Handle<SwaptionVolatilityStructure> swvol_h(swvol);

		// Setup the Bermudan Swaption
		Date effectiveDate = TARGET().advance(refDate, 2 * Days);
		Date maturityDate = TARGET().advance(effectiveDate, 10 * Years);
		Schedule fixedSchedule(effectiveDate, maturityDate, 1 * Years, TARGET(),
			ModifiedFollowing, ModifiedFollowing,
			DateGeneration::Forward, false);
		Schedule floatingSchedule(effectiveDate, maturityDate, 6 * Months,
			TARGET(), ModifiedFollowing,
			ModifiedFollowing, DateGeneration::Forward,
			false);
		Real strike = 0.03;

		std::vector<Date> exerciseDates;
		for (Size i = 1; i < 10; ++i) {
			exerciseDates.push_back(TARGET().advance(fixedSchedule[i], -2 * Days));
		}

		// calibration basket
		std::vector<Real> inputVol(9, 0.0);
		for (Size i = 1; i < 10; ++i) {
			Period swapLength = (10 - i) * Years;
			inputVol[i - 1] = swvol->volatility(exerciseDates[i - 1], swapLength, strike);
		}

		// pricing without AAD
		std::cout << "Pricing Bermudan swaption...\n";
		auto start = std::chrono::high_resolution_clock::now();
		Real v = priceSwaption(inputVol, fixedSchedule, floatingSchedule, strike, exerciseDates, euribor6m, yts6m_h, refDate);
		auto end = std::chrono::high_resolution_clock::now();
		auto time_plain = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * 1e-3;
		std::cout << "Swaption Value: " << v << "\n";

		// pricing with AAD
		std::vector<Real> gradient;
		std::cout << "Pricing Bermudan swaption with sensitivities...\n";
		start = std::chrono::high_resolution_clock::now();
		Real v2 = priceWithSensi(inputVol, fixedSchedule, floatingSchedule, strike, exerciseDates, euribor6m, yts6m_h, refDate, gradient);
		end = std::chrono::high_resolution_clock::now();
		auto time_sensi = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * 1e-3;
		std::cout << "Swaption Value: " << v2 << "\n";

		printResults(v2, gradient);

		std::cout << "Plain time : " << time_plain << "ms\n"
			<< "Sensi time : " << time_sensi << "ms\n"
			<< "Factor     : " << time_sensi / time_plain << "x\n";

		return 0;
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	catch (...) {
		std::cerr << "unknown error" << std::endl;
		return 1;
	}
}