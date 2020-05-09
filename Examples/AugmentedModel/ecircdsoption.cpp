#include <ql/qldefines.hpp>
#ifdef BOOST_MSVC
#include <ql/auto_link.hpp>
#endif

#include "ecircdsoption.hpp"
#include <ql/math/integrals/trapezoidintegral.hpp>
#include <ql/math/solvers1d/brent.hpp>

using namespace QuantLib;

Real huPrime(Time u,
             const ext::shared_ptr<CDSOptionParams> params,
             const ext::shared_ptr<CDSOptionMarket> market) {
    const Time tBu = params->beta(u);
    const Real discCdsStartU = market->drTS->discount(u) / market->drTS->discount(params->cdsStart());
    const Real dPdMaturity = market->drTS->forwardRate(u, u, Continuous, NoFrequency) *
                             discCdsStartU; // TODO: Assumption about q(u)
    return (1 - market->recoveryRate) * dPdMaturity - params->cdsStrike() * (u - tBu) * dPdMaturity +
           params->cdsStrike() * discCdsStartU;
}


Real YStarIntegrand::operator()(Real u) const {
    const Real hDiscBondU = ecirModel->discountBondYt(params->cdsStart(), u, candidateYstarTa);
    const Real integrandHuPrime = huPrime(u, params, market) * hDiscBondU;
    // std::cout << "forward rate at u = " << drTS->forwardRate(u, u, Continuous, NoFrequency)
    // << " huPrime  = " << huPrime << " hDiscBondU  = " << hDiscBondU << std::endl;
    return integrandHuPrime;
}

const Real integralYStarNum(const ext::shared_ptr<YStarIntegrand> yStarIntegrand) {
    const Real tolerance = 1.0e-6;
    const Size maxIter = 100;
    const TrapezoidIntegral<Default> integral(tolerance, maxIter);
    const Time xMin = yStarIntegrand->params->cdsStart();
    const Time xMax = yStarIntegrand->params->cdsEnd();
    const Real candidateIntegralHuPrime =
        integral(ext::function<Real(Real)>(*yStarIntegrand), xMin, xMax);
    return candidateIntegralHuPrime;
}

Real YStarTarget::operator()(Real yStarTa) const {

    const ext::shared_ptr<YStarIntegrand> yStarIntegrand =
        ext::make_shared<YStarIntegrand>(params, market, ecirModel, yStarTa);
    const Real intHuPrime = integralYStarNum(yStarIntegrand);
    const Real hDiscBondCds = ecirModel->discountBondYt(params->cdsStart(), params->cdsEnd(), yStarTa);
    const Real lhs = intHuPrime;
    const Real rhs = (1 - market->recoveryRate) *
                     (1 - market->drTS->discount(params->cdsEnd()) /
                              market->drTS->discount(params->cdsStart()) * hDiscBondCds);
    Real target = lhs - rhs;
    // std::cout << "y_star = " << yStarTa << ", integral = " << intHuPrime << ", target = " <<
    // target << std::endl;
    return target;
}

Real findYStar(YStarTarget yStarTarget,
               const ext::shared_ptr<CDSOptionParams> params,
               const ext::shared_ptr<CDSOptionMarket> market) {
    const Real yStarGuess =
        market->hrTS->forwardRate(params->cdsStart(), params->cdsEnd(), Continuous, NoFrequency);
    const Brent solver = Brent();
    const Real tolerance = 1.0e-6;
    const Real yStar = solver.solve(yStarTarget, tolerance, yStarGuess, 0.1);
    return yStar;
}

Real CDSOptionIntegrand::operator()(Real u) const {
    if (u == params->cdsStart()) {
        return 0.0;
    }
    const Real strikeDiscBondYStarU = ecirModel->discountBondYt(params->cdsStart(), u, yStar);
    const Real calculatedOption = ecirModel->discountBondOption(
        params->calcOptionType(), strikeDiscBondYStarU, params->cdsStart(), u);
    const Real integrand = huPrime(u, params, market) * calculatedOption;
    // std::cout << "u = " << u << ", integrand = " << integrand << std::endl;
    return integrand;
}

const Real integralCDSOptionNum(const ext::shared_ptr<CDSOptionIntegrand> CDSOptionIntegrand,
                                const ext::shared_ptr<CDSOptionParams> params) {
    const Real tolerance = 1.0e-6;
    const Size maxIter = 100;
    const TrapezoidIntegral<Default> integral(tolerance, maxIter);
    const Time xMin = params->cdsStart();
    const Time xMax = params->cdsEnd();
    return integral(ext::function<Real(Real)>(*CDSOptionIntegrand), xMin, xMax);
}

Real CDSOptionPricer::getPrice() const {
    const Real yStar = findYStar(YStarTarget(params, market, ecirModel), params, market);
    const ext::shared_ptr<CDSOptionIntegrand> cdsOptionIntegrand =
        ext::make_shared<CDSOptionIntegrand>(params, market, ecirModel, yStar);
    const Real integralCDSOptionPrime = integralCDSOptionNum(cdsOptionIntegrand, params);
    const Real integralCDSOption =
        integralCDSOptionPrime +
        (1 - market->recoveryRate) * market->drTS->discount(params->cdsEnd()) /
            market->drTS->discount(params->cdsStart()) *
            ecirModel->discountBondOption(
                params->calcOptionType(),
                ecirModel->discountBondYt(params->cdsStart(), params->cdsEnd(), yStar),
                params->cdsStart(), params->cdsEnd());
    const Real cdsOptionPrice = params->notional() * market->drTS->discount(params->cdsStart()) * integralCDSOption;
    return cdsOptionPrice;
}