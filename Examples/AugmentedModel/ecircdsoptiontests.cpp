#include <ql/qldefines.hpp>
#ifdef BOOST_MSVC
#include <ql/auto_link.hpp>
#endif

#include "ecircdsoption.hpp"
#include "ecircdsoptiontests.hpp"
#include <iostream>

using namespace QuantLib;

// CIR++ Discount Bond Option price depending on strike
void case1() {
    std::cout << "ECIR model: zero-bond option vs strike pricing..." << std::endl;
    const ext::shared_ptr<CDSOptionMarket> market = CDSOptionMarketFactory().doubleFlat();
    const ext::shared_ptr<CDSOptionParams> params = std::make_shared<CDSOptionParams>(market);
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel = ECIRFactory::basic(market);
    const Time bondStart = params->cdsStart();
    const Time bondEnd = params->cdsEnd();

    const Real expectBond = market->drTS->discount(bondEnd) / market->drTS->discount(bondStart);
    const Real calcBond = ecirModel->discountBond(bondStart, bondEnd, market->discRateT0());
    std::cout << "ATM bond=" << calcBond << ", cir expect diff=" << std::abs(expectBond - calcBond)
              << std::endl;

    const int gridSize = 10;
    const double gridPercDrop = 0.1;
    for (int i = 0; i <= gridSize; ++i) {
        const double bondStrike = calcBond * (1.0 + gridPercDrop * (i * 2.0 / gridSize - 1.0));
        const Real calcCall =
            ecirModel->discountBondOption(Option::Type::Call, bondStrike, bondStart, bondEnd);
        const Real calcPut =
            ecirModel->discountBondOption(Option::Type::Put, bondStrike, bondStart, bondEnd);
        std::cout << "Strike= " << bondStrike << ", Call=" << calcCall << ", Put=" << calcPut
                  << std::endl;
    }
}

// CIR++ Discount Bond Option price depending on time to maturity (u)
void case2() {
    std::cout << "ECIR model: zero-bond option vs time to maturity pricing..." << std::endl;
    const ext::shared_ptr<CDSOptionMarket> market = CDSOptionMarketFactory().doubleFlat();
    const ext::shared_ptr<CDSOptionParams> params = std::make_shared<CDSOptionParams>(market);
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel = ECIRFactory::basic(market);
    const Time bondStart = params->cdsStart();
    const Time bondEnd = params->cdsEnd();

    const Real expectBond = market->drTS->discount(bondEnd) / market->drTS->discount(bondStart);

    int gridSize = 10;
    const double gridPercDrop = 0.1;
    for (int i = 0; i <= gridSize; ++i) {
        double maturity = bondStart + (bondEnd - bondStart) * (i * 1.0 / gridSize);
        Real strikeBond = ecirModel->discountBond(bondStart, maturity, market->discRateT0());
        const Real calcCall =
            ecirModel->discountBondOption(Option::Type::Call, strikeBond, bondStart, maturity);
        const Real calcPut =
            ecirModel->discountBondOption(Option::Type::Put, strikeBond, bondStart, maturity);
        std::cout << "Bond maturity=" << maturity << ", bond=" << strikeBond
                  << ", atm call=" << calcCall << ", atm put=" << calcPut << std::endl;
    }
}

// CIR++ Discount Bond Option price depending on the y of the lambda process at now (undrly disc
// bond start)
void case3() {
    std::cout << "Testing zero-bond pricing for extended CIR model..." << std::endl;
    const ext::shared_ptr<CDSOptionMarket> market = CDSOptionMarketFactory().doubleFlat();
    const ext::shared_ptr<CDSOptionParams> params = std::make_shared<CDSOptionParams>(market);
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel = ECIRFactory::basic(market);
    const Time bondStart = params->cdsStart();
    const Time bondEnd = params->cdsEnd();
    
    //const Real expected = market->drTS->discount(bondEnd) / market->drTS->discount(bondStart);
    const CoxIngersollRoss cirModel(market->discRateT0(), market->discRateT0(), 0.1, 0.1);
    const Real calculatedcir = cirModel.discountBond(0, bondEnd - bondStart, market->discRateT0());
    std::cout << "cir discountBond = " << calculatedcir << std::endl;

    const int gridSize = 10;
    const double gridPercDrop = 0.5;
    for (int i = 0; i <= gridSize; ++i) {
        double x0 = market->discRateT0() * (1.0 + gridPercDrop * (i * 2.0 / gridSize - 1.0));
        const Real calcBond =
            ecirModel->discountBond(0, bondEnd - bondStart, x0 + ecirModel->phi(0));
        const Real calcBondYt = ecirModel->discountBondYt(0, bondEnd - bondStart, x0);
        std::cout << "x0=" << x0 << ", phi0=" << ecirModel->phi(0) << " ecir bond=" << calcBond
                  << " ecir bondYt=" << calcBondYt << std::endl;
    }
}

// Ystar findings
void case4() {
    std::cout << "Testing ystar finding for extended CIR model..." << std::endl;
    const ext::shared_ptr<CDSOptionMarket> market = CDSOptionMarketFactory().doubleFlat();
    const ext::shared_ptr<CDSOptionParams> params = std::make_shared<CDSOptionParams>(market);
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel = ECIRFactory::basic(market);
    
    const Real yStar = findYStar(YStarTarget(params, market, ecirModel), params, market);
    const Real discBondYStarU = ecirModel->discountBondYt(params->cdsStart(), params->cdsEnd(), yStar);
    std::cout << "Solver y*=" << yStar << ", disc bond at y*=" << discBondYStarU << std::endl;

    const Real calcCall = ecirModel->discountBondOption(Option::Type::Call, discBondYStarU,
                                                        params->cdsStart(), params->cdsEnd());
    const Real calcPut = ecirModel->discountBondOption(Option::Type::Put, discBondYStarU,
                                                       params->cdsStart(), params->cdsEnd());
    std::cout << "Call on bond at y*=" << calcCall << ", put=" << calcPut << std::endl;
}

// CIR++ Discount Bond Option price after finding y star
void case5() {
    std::cout << "Testing CDS Option price after finding y*for eCIR model..." << std::endl;
    const ext::shared_ptr<CDSOptionMarket> market = CDSOptionMarketFactory().doubleFlat();
    const ext::shared_ptr<CDSOptionParams> params = std::make_shared<CDSOptionParams>(market);
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel = ECIRFactory::basic(market);

    CDSOptionPricer pricer(params, market, ecirModel);
    const Real cdsOptionPrice = pricer.getPrice();
    Real vol = market->implVol(params, cdsOptionPrice);
    std::cout << "CDS Option price=" << cdsOptionPrice << " implVol=" << vol << std::endl;
}

void case6() {
    std::cout << "Testing CDS data..." << std::endl;
    const ext::shared_ptr<CDSOptionMarket> market = CDSOptionMarketFactory().doubleFlat();
    const ext::shared_ptr<CDSOptionParams> params = std::make_shared<CDSOptionParams>(market);
    Real spreadCalc = market->cdsFairSpread(params);
    std::cout << "Spread calc=" << spreadCalc << std::endl;
    Real blackCalc = market->blackPrice(params, 0.25);
    Real vol = market->implVol(params, blackCalc);
    std::cout << "Blackcalc=" << blackCalc << " implVol=" << vol << std::endl;
}

void case7() {
    std::cout << "Testing CDS Option under new conditions eCIR model..." << std::endl;
    const ext::shared_ptr<CDSOptionMarket> market = CDSOptionMarketFactory().cdsAndSwap();
    const ext::shared_ptr<CDSOptionParams> params = std::make_shared<CDSOptionParams>(market);
    
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel =
        ext::make_shared<ExtendedCoxIngersollRoss>(market->drTS, market->hazardRateT0(), 0.2, 0.05,
                                                   market->hazardRateT0());

    CDSOptionPricer pricer(params, market, ecirModel);
    const Real cdsOptionPrice = pricer.getPrice();
    Real vol = market->implVol(params, cdsOptionPrice);
    std::cout << "CDS Option t=" << params->cdsStart() << " T=" << params->cdsEnd()
              << " price=" << cdsOptionPrice
              << " implVol=" << vol << std::endl;
    
    Real blackCalc = market->blackPrice(params, vol);
    std::cout << "Blackcalc=" << blackCalc << std::endl;
    market->implVol(params, blackCalc);
}

void case8() {
    std::cout << "Testing CDS Option vol surface..." << std::endl;
    const ext::shared_ptr<CDSOptionMarket> market = CDSOptionMarketFactory().doubleFlat();
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel =
        ext::make_shared<ExtendedCoxIngersollRoss>(market->drTS, market->hazardRateT0(), 0.2, 0.05,
                                                   market->hazardRateT0());
    for (int cdsStart = 1; cdsStart < 10; cdsStart++) {
        for (int cdsEnd = cdsStart + 1; cdsEnd < 10; cdsEnd++) {
            const ext::shared_ptr<CDSOptionParams> params =
                std::make_shared<CDSOptionParams>(market, cdsStart, cdsEnd);
            CDSOptionPricer pricer(params, market, ecirModel);
            const Real cdsOptionPrice = pricer.getPrice();
            Real vol = market->implVol(params, cdsOptionPrice);
            std::cout << "CDS Option t=" << params->cdsStart() << " T=" << params->cdsEnd()
                      << " price=" << cdsOptionPrice << " implVol=" << vol << std::endl;
        }
    }
    

}

