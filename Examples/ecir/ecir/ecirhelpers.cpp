#include <ql/qldefines.hpp>
#ifdef BOOST_MSVC
#include <ql/auto_link.hpp>
#endif

#include "ecirhelpers.hpp"
#include <ql/experimental/credit/blackcdsoptionengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/pricingengines/credit/isdacdsengine.hpp>
#include <ql/termstructures/credit/all.hpp>

#include <iostream>
#include <ql/time/daycounters/all.hpp>
#include <ql/currencies/europe.hpp>


ext::shared_ptr<ExtendedCoxIngersollRoss>
ECIRFactory::basic(ext::shared_ptr<CDSOptionMarket> market) {
    const Real rate = market->hazardRateT0();
    return ext::make_shared<ExtendedCoxIngersollRoss>(market->drTS, rate, 0.1, 0.1, rate);
    // ExtendedCoxIngersollRoss(termStructure, theta = 0.1, k = 0.1, sigma = 0.1, x0 = 0.05);
    // ExtendedCoxIngersollRoss(termStructure, mu = 0.026, kappa = 0.4, v = 0.14, y0 = 0.0165);
    // hazardRate = 0.03 / 0.05 / 0.07 return
    // ext::make_shared<ExtendedCoxIngersollRoss>(market->drTS, 0.026, 0.4, 0.14, 0.0165); return
    // ext::make_shared<ExtendedCoxIngersollRoss>(market->drTS, 0.02, 0.8, 0.2, 0.01); //Mid, hazard
    // rate -> CDS spreads 100 <- incorrect return
    // ext::make_shared<ExtendedCoxIngersollRoss>(market->drTS, 0.05, 0.5, 0.2, 0.03); // Mid, hazard
    // rate -> CDS spreads 250 <- incorrect?
}

CDSOptionParams::CDSOptionParams(const ext::shared_ptr<CDSOptionMarket> market,
                                 const Integer cdsStartYs,
                                 const Integer cdsEndYs)
: _cdsStart(cdsStartYs), _cdsEnd(cdsEndYs) {
    Calendar cal = CDSOptionMarket::calendar();
    _expiryDate = cal.advance(CDSOptionMarket::today(), cdsStartYs, Years);
    _startDate = cal.advance(_expiryDate, 0, Years); // startDate = expiryDate
    _maturityDate = cal.advance(_startDate, cdsEndYs - cdsStartYs, Years);
    BusinessDayConvention bdc = CDSOptionMarket::convention();
    _schedule = Schedule(_startDate, _maturityDate, Period(Semiannual), cal, bdc, bdc,
                        DateGeneration::Forward, false);
    underlying = ext::make_shared<CreditDefaultSwap>(Protection::Seller, notional(), cdsStrike(),
                                                     _schedule, bdc, CDSOptionMarket::dayCounter());
    underlying->setPricingEngine(market->swapEngine);
    exercise = ext::make_shared<EuropeanExercise>(_expiryDate);
    //_cdsStrike = underlying->fairSpread();
}

const Time CDSOptionParams::beta(Time u) const {
    Time deltau = std::max(u - cdsStart(), 0.0);
    Date uDate = _startDate + Period(Integer(deltau * 365.0), Days);
    Time out = cdsStart() +
               std::max(Time(_schedule.previousDate(uDate) - _startDate) / 365.0, 0.0);
    return out;
} 

CDSOptionMarket::CDSOptionMarket(const Handle<YieldTermStructure> drTS,
                                 const Handle<YieldTermStructure> hrTS,
                                 const Handle<DefaultProbabilityTermStructure> dpTS)
: drTS(drTS), hrTS(hrTS), dpTS(dpTS) {
    Settings::instance().evaluationDate() = CDSOptionMarket::today();
    swapEngine = ext::make_shared<MidPointCdsEngine>(dpTS, recoveryRate, drTS);
}

Real CDSOptionMarket::blackPrice(ext::shared_ptr<CDSOptionParams> params, Real cdsVol) {
    CdsOption option(params->underlying, params->exercise);
    Handle<Quote> cdsVolQuote(ext::shared_ptr<Quote>(new SimpleQuote(cdsVol)));
    option.setPricingEngine(ext::shared_ptr<PricingEngine>(
        new BlackCdsOptionEngine(dpTS, recoveryRate, drTS, cdsVolQuote)));
    std::cout << "ATM rate=" << option.atmRate() << std::endl;
    std::cout << "NPV=" << option.NPV() << std::endl;
    return option.NPV();
}

Real CDSOptionMarket::implVol(ext::shared_ptr<CDSOptionParams> params, Real cdsOptionPrice) {
    CdsOption option(params->underlying, params->exercise);
    Handle<Quote> cdsVolQuote(ext::shared_ptr<Quote>(new SimpleQuote(0.3)));
    option.setPricingEngine(ext::shared_ptr<PricingEngine>(
        new BlackCdsOptionEngine(dpTS, recoveryRate, drTS, cdsVolQuote)));
    Real implvol = option.impliedVolatility(cdsOptionPrice, drTS, dpTS, recoveryRate);
    return implvol;
}

Real CDSOptionMarket::cdsFairSpread(ext::shared_ptr<CDSOptionParams> params) {
    const Real fairSpread = params->underlying->fairSpread();
    std::cout << "Pricing of underlying CDS: fair spread=" << io::rate(fairSpread)
              << ", NPV=" << params->underlying->NPV() << std::endl;
    return fairSpread;
}

//////////////////////////////////
// CDS Option Market Factory
/////////////////////////////////

ext::shared_ptr<YieldTermStructure>
flatRate(const Date& today, const ext::shared_ptr<Quote>& forward, const DayCounter& dc) {
    return ext::shared_ptr<YieldTermStructure>(new FlatForward(today, Handle<Quote>(forward), dc));
}

ext::shared_ptr<YieldTermStructure>
flatRate(const Date& today, Rate forward, const DayCounter& dc) {
    return flatRate(today, ext::shared_ptr<Quote>(new SimpleQuote(forward)), dc);
}

ext::shared_ptr<CDSOptionMarket> CDSOptionMarketFactory::doubleFlat(const Real discRate,
                                                                    const Real hazardRate) {

    const Handle<YieldTermStructure> drTS(
        flatRate(CDSOptionMarket::today(), discRate, CDSOptionMarket::dayCounter()));
    
    const ext::shared_ptr<Quote> hazardRateQuote(new SimpleQuote(hazardRate));
    const Handle<YieldTermStructure> hrTS(
        flatRate(CDSOptionMarket::today(), hazardRateQuote, CDSOptionMarket::dayCounter()));
    const Handle<DefaultProbabilityTermStructure> dpTS(
        ext::shared_ptr<DefaultProbabilityTermStructure>(new FlatHazardRate(
            0, CDSOptionMarket::calendar(), Handle<Quote>(hazardRateQuote), CDSOptionMarket::dayCounter())));

    return ext::make_shared<CDSOptionMarket>(drTS, hrTS, dpTS);
}

ext::shared_ptr<CDSOptionMarket> CDSOptionMarketFactory::cdsAndSwap() {
    // this is the example from Apdx E in pricing and risk management of CDS, OpenGamma

    /*Date tradeDate(13, June, 2011);
    Settings::instance().evaluationDate() = tradeDate;*/
    Settings::instance().evaluationDate() = CDSOptionMarket::today();

    ext::shared_ptr<DepositRateHelper> dp1m = ext::make_shared<DepositRateHelper>(
        0.00445, 1 * Months, 2, WeekendsOnly(), ModifiedFollowing, false, Actual360());
    ext::shared_ptr<DepositRateHelper> dp2m = ext::make_shared<DepositRateHelper>(
        0.00949, 2 * Months, 2, WeekendsOnly(), ModifiedFollowing, false, Actual360());
    ext::shared_ptr<DepositRateHelper> dp3m = ext::make_shared<DepositRateHelper>(
        0.01234, 3 * Months, 2, WeekendsOnly(), ModifiedFollowing, false, Actual360());
    ext::shared_ptr<DepositRateHelper> dp6m = ext::make_shared<DepositRateHelper>(
        0.01776, 6 * Months, 2, WeekendsOnly(), ModifiedFollowing, false, Actual360());
    ext::shared_ptr<DepositRateHelper> dp9m = ext::make_shared<DepositRateHelper>(
        0.01935, 9 * Months, 2, WeekendsOnly(), ModifiedFollowing, false, Actual360());
    ext::shared_ptr<DepositRateHelper> dp1y = ext::make_shared<DepositRateHelper>(
        0.02084, 12 * Months, 2, WeekendsOnly(), ModifiedFollowing, false, Actual360());

    // this index is probably not important since we are not using
    // IborCoupon::usingAtParCoupons() == false
    // - define it "isda compliant" anyway
    ext::shared_ptr<IborIndex> euribor6m =
        ext::make_shared<IborIndex>("IsdaIbor", 6 * Months, 2, EURCurrency(), WeekendsOnly(),
                                    ModifiedFollowing, false, Actual360());

    ext::shared_ptr<SwapRateHelper> sw2y = ext::make_shared<SwapRateHelper>(
        0.01652, 2 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw3y = ext::make_shared<SwapRateHelper>(
        0.02018, 3 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw4y = ext::make_shared<SwapRateHelper>(
        0.02303, 4 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw5y = ext::make_shared<SwapRateHelper>(
        0.02525, 5 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw6y = ext::make_shared<SwapRateHelper>(
        0.02696, 6 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw7y = ext::make_shared<SwapRateHelper>(
        0.02825, 7 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw8y = ext::make_shared<SwapRateHelper>(
        0.02931, 8 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw9y = ext::make_shared<SwapRateHelper>(
        0.03017, 9 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw10y = ext::make_shared<SwapRateHelper>(
        0.03092, 10 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw11y = ext::make_shared<SwapRateHelper>(
        0.03160, 11 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw12y = ext::make_shared<SwapRateHelper>(
        0.03231, 12 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw15y = ext::make_shared<SwapRateHelper>(
        0.03367, 15 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw20y = ext::make_shared<SwapRateHelper>(
        0.03419, 20 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw25y = ext::make_shared<SwapRateHelper>(
        0.03411, 25 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);
    ext::shared_ptr<SwapRateHelper> sw30y = ext::make_shared<SwapRateHelper>(
        0.03412, 30 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(), euribor6m);

    std::vector<ext::shared_ptr<RateHelper> > isdaYieldHelpers;

    isdaYieldHelpers.push_back(dp1m);
    isdaYieldHelpers.push_back(dp2m);
    isdaYieldHelpers.push_back(dp3m);
    isdaYieldHelpers.push_back(dp6m);
    isdaYieldHelpers.push_back(dp9m);
    isdaYieldHelpers.push_back(dp1y);
    isdaYieldHelpers.push_back(sw2y);
    isdaYieldHelpers.push_back(sw3y);
    isdaYieldHelpers.push_back(sw4y);
    isdaYieldHelpers.push_back(sw5y);
    isdaYieldHelpers.push_back(sw6y);
    isdaYieldHelpers.push_back(sw7y);
    isdaYieldHelpers.push_back(sw8y);
    isdaYieldHelpers.push_back(sw9y);
    isdaYieldHelpers.push_back(sw10y);
    isdaYieldHelpers.push_back(sw11y);
    isdaYieldHelpers.push_back(sw12y);
    isdaYieldHelpers.push_back(sw15y);
    isdaYieldHelpers.push_back(sw20y);
    isdaYieldHelpers.push_back(sw25y);
    isdaYieldHelpers.push_back(sw30y);

    // build yield curve
    Handle<YieldTermStructure> isdaYts =
        Handle<YieldTermStructure>(ext::make_shared<PiecewiseYieldCurve<Discount, LogLinear> >(
            0, WeekendsOnly(), isdaYieldHelpers, Actual365Fixed()));
    isdaYts->enableExtrapolation();


    CreditDefaultSwap::PricingModel model = CreditDefaultSwap::ISDA;
    ext::shared_ptr<CdsHelper> cds6m(new SpreadCdsHelper(
        0.007927, 6 * Months, 1, WeekendsOnly(), Quarterly, Following, DateGeneration::CDS,
        Actual360(), 0.4, isdaYts, true, true, Date(), Actual360(true), true, model));
    ext::shared_ptr<CdsHelper> cds1y(new SpreadCdsHelper(
        0.007927, 1 * Years, 1, WeekendsOnly(), Quarterly, Following, DateGeneration::CDS,
        Actual360(), 0.4, isdaYts, true, true, Date(), Actual360(true), true, model));
    ext::shared_ptr<CdsHelper> cds3y(new SpreadCdsHelper(
        0.012239, 3 * Years, 1, WeekendsOnly(), Quarterly, Following, DateGeneration::CDS,
        Actual360(), 0.4, isdaYts, true, true, Date(), Actual360(true), true, model));
    ext::shared_ptr<CdsHelper> cds5y(new SpreadCdsHelper(
        0.016979, 5 * Years, 1, WeekendsOnly(), Quarterly, Following, DateGeneration::CDS,
        Actual360(), 0.4, isdaYts, true, true, Date(), Actual360(true), true, model));
    ext::shared_ptr<CdsHelper> cds7y(new SpreadCdsHelper(
        0.019271, 7 * Years, 1, WeekendsOnly(), Quarterly, Following, DateGeneration::CDS,
        Actual360(), 0.4, isdaYts, true, true, Date(), Actual360(true), true, model));
    ext::shared_ptr<CdsHelper> cds10y(new SpreadCdsHelper(
        0.020860, 10 * Years, 1, WeekendsOnly(), Quarterly, Following, DateGeneration::CDS,
        Actual360(), 0.4, isdaYts, true, true, Date(), Actual360(true), true, model));

    std::vector<ext::shared_ptr<DefaultProbabilityHelper> > isdaCdsHelpers;

    isdaCdsHelpers.push_back(cds6m);
    isdaCdsHelpers.push_back(cds1y);
    isdaCdsHelpers.push_back(cds3y);
    isdaCdsHelpers.push_back(cds5y);
    isdaCdsHelpers.push_back(cds7y);
    isdaCdsHelpers.push_back(cds10y);

    // build credit curve
    Handle<DefaultProbabilityTermStructure> isdaCts = Handle<DefaultProbabilityTermStructure>(
        ext::make_shared<PiecewiseDefaultCurve<SurvivalProbability, LogLinear> >(
            0, WeekendsOnly(), isdaCdsHelpers, Actual365Fixed()));

    // set up isda engine
    ext::shared_ptr<IsdaCdsEngine> isdaPricer =
        ext::make_shared<IsdaCdsEngine>(isdaCts, 0.4, isdaYts);

    // check the curves
    std::cout << "ISDA yield curve:" << std::endl;
    std::cout << "date;time;zeroyield" << std::endl;
    for (Size i = 0; i < isdaYieldHelpers.size(); i++) {
        Date d = isdaYieldHelpers[i]->latestDate();
        Real t = isdaYts->timeFromReference(d);
        std::cout << d << ";" << t << ";"
                  << isdaYts->zeroRate(d, Actual365Fixed(), Continuous).rate() << std::endl;
    }

    /*std::cout << "ISDA credit curve:" << std::endl;
    std::cout << "date;time;survivalprob" << std::endl;
    for (Size i = 0; i < isdaCdsHelpers.size(); i++) {
        Date d = isdaCdsHelpers[i]->latestDate();
        Real t = isdaCts->timeFromReference(d);
        std::cout << d << ";" << t << ";" << isdaCts->survivalProbability(d) << std::endl;
    }*/

    std::vector<Date> dates({CDSOptionMarket::today()});
    std::vector<DiscountFactor> dfs({1.0});
    const DayCounter& dayCounter = Actual365Fixed();
    const Calendar& calendar = TARGET();

    std::cout << "ISDA credit curve:" << std::endl;
    std::cout << "date;time;survivalprob" << std::endl;
    for (Size i = 0; i < isdaCdsHelpers.size(); i++) {
        Date d = isdaCdsHelpers[i]->latestDate();
        Real t = isdaCts->timeFromReference(d);
        std::cout << d << ";" << t << ";" << isdaCts->survivalProbability(d) << std::endl;
        
        dates.push_back(d);
        dfs.push_back(isdaCts->survivalProbability(d));
    }

    //DiscountCurve(dates, dfs, dayCounter, calendar); //Log linear //, const Interpolator& interpolator

    const Handle<YieldTermStructure> drTS(isdaYts);
    const Handle<DefaultProbabilityTermStructure> dpTS(isdaCts);
    const Handle<YieldTermStructure> hrTS(ext::shared_ptr<YieldTermStructure>(new DiscountCurve(dates, dfs, dayCounter, calendar)));
    return ext::make_shared<CDSOptionMarket>(drTS, hrTS, dpTS);
}

