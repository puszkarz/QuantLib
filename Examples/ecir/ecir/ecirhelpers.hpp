#ifndef ecirhelpers_h
#define ecirhelpers_h

#include <ql/models/shortrate/onefactormodels/extendedcoxingersollross.hpp>
#include <ql/experimental/credit/cdsoption.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/termstructures/yield/all.hpp>

using namespace QuantLib;

class CDSOptionParams;
class CDSOptionMarket;

class CDSOptionParams {
    const Option::Type _cdsOptionType = Option::Type::Put;
    const Real _notional = 1000000.0;

    const Integer _cdsStart; // Integer years no = 1.0
    const Integer _cdsEnd;   // Integer years no  = 2.0
    Date _expiryDate;
    Date _startDate;
    Date _maturityDate;
    Schedule _schedule;

    Real _cdsStrike = 0.06; // 0.0614015 // 2.0

  public:
    const Time cdsStart() const { return Time(_cdsStart); }
    const Time cdsEnd() const { return Time(_cdsEnd); }
    const Real cdsStrike() const { return _cdsStrike; }
    const Option::Type cdsOptionType() const { return _cdsOptionType; }
    const Real notional() const { return _notional; }

    ext::shared_ptr<CreditDefaultSwap> underlying;
    ext::shared_ptr<Exercise> exercise;

    CDSOptionParams(const ext::shared_ptr<CDSOptionMarket> market,
                    const Integer cdsStartYs = 1,
                    const Integer cdsEndYs = 2);

    const Option::Type calcOptionType() const { return Option::Type(-cdsOptionType()); }
    const Time beta(Time u) const;
    
};

class CDSOptionMarket {
  public:
    static Date today() { return Date(10, December, 2007); }
    static BusinessDayConvention convention() { return ModifiedFollowing; }
    static DayCounter dayCounter() { return Actual365Fixed(); }
    static Calendar calendar() { return TARGET(); }

    const Handle<YieldTermStructure> drTS;
    const Real recoveryRate = 0.7;
    const Handle<YieldTermStructure> hrTS;
    const Handle<DefaultProbabilityTermStructure> dpTS;

    ext::shared_ptr<PricingEngine> swapEngine;

    CDSOptionMarket(const Handle<YieldTermStructure> drTS,
                    const Handle<YieldTermStructure> hrTS,
                    const Handle<DefaultProbabilityTermStructure> dpTS);

    const Real hazardRateT0() { return hrTS->forwardRate(0.0, 0.0, Continuous, NoFrequency); }
    const Real discRateT0() { return drTS->forwardRate(0.0, 0.0, Continuous, NoFrequency); }

    Real cdsFairSpread(ext::shared_ptr<CDSOptionParams> params);
    Real blackPrice(ext::shared_ptr<CDSOptionParams> params, Real cdsVol);
    Real implVol(ext::shared_ptr<CDSOptionParams> params, Real cdsOptionPrice);
};

ext::shared_ptr<YieldTermStructure>
flatRate(const Date& today, const ext::shared_ptr<Quote>& forward, const DayCounter& dc);

ext::shared_ptr<YieldTermStructure> flatRate(const Date& today, Rate forward, const DayCounter& dc);

class CDSOptionMarketFactory {
  public:
    static ext::shared_ptr<CDSOptionMarket> doubleFlat(const Real discRate = 0.1,
                                                       const Real hazardRate = 0.2);
    static ext::shared_ptr<CDSOptionMarket> cdsAndSwap();
};

class ECIRFactory {
  public:
    static ext::shared_ptr<ExtendedCoxIngersollRoss> basic(ext::shared_ptr<CDSOptionMarket> market);
};

#endif