#ifndef ecircdsoption_h
#define ecircdsoption_h

#include "ecirhelpers.hpp"

using namespace QuantLib;

///////////////////////////////
// CIR++ CDS Option Pricer
///////////////////////////////

Real huPrime(Time u,
             const ext::shared_ptr<CDSOptionParams> params,
             const ext::shared_ptr<CDSOptionMarket> market);

class YStarIntegrand {
    const ext::shared_ptr<CDSOptionMarket> market;
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel;
    const Time candidateYstarTa;

  public:
    const ext::shared_ptr<CDSOptionParams> params;

    explicit YStarIntegrand(const ext::shared_ptr<CDSOptionParams> params,
                            const ext::shared_ptr<CDSOptionMarket> market,
                            const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel,
                            const Real candidateYstar)
    : params(params), market(market), ecirModel(ecirModel), candidateYstarTa(candidateYstar) {}

    Real operator()(Real u) const;
};

const Real integralYStarNum(const ext::shared_ptr<YStarIntegrand> yStarIntegrand);

class YStarTarget {
    const ext::shared_ptr<CDSOptionParams> params;
    const ext::shared_ptr<CDSOptionMarket> market;
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel;

  public:
    explicit YStarTarget(const ext::shared_ptr<CDSOptionParams> params,
                         const ext::shared_ptr<CDSOptionMarket> market,
                         const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel)
    : params(params), market(market), ecirModel(ecirModel) {}

    Real operator()(Real yStarTa) const;
};

Real findYStar(YStarTarget yStarTarget,
               const ext::shared_ptr<CDSOptionParams> params,
               const ext::shared_ptr<CDSOptionMarket> market);

class CDSOptionIntegrand {
    const ext::shared_ptr<CDSOptionParams> params;
    const ext::shared_ptr<CDSOptionMarket> market;
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel;
    const Real yStar;

  public:
    explicit CDSOptionIntegrand(const ext::shared_ptr<CDSOptionParams> params,
                                const ext::shared_ptr<CDSOptionMarket> market,
                                const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel,
                                const Real yStar)
    : params(params), market(market), ecirModel(ecirModel), yStar(yStar) {}

    Real operator()(Real u) const;
};

const Real integralCDSOptionNum(const ext::shared_ptr<CDSOptionIntegrand> CDSOptionIntegrand,
                                const ext::shared_ptr<CDSOptionParams> params);

class CDSOptionPricer {
    const ext::shared_ptr<CDSOptionParams> params;
    const ext::shared_ptr<CDSOptionMarket> market;
    const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel;

  public:
    explicit CDSOptionPricer(const ext::shared_ptr<CDSOptionParams> params,
                             const ext::shared_ptr<CDSOptionMarket> market,
                             const ext::shared_ptr<ExtendedCoxIngersollRoss> ecirModel)
    : params(params), market(market), ecirModel(ecirModel) {}

    Real getPrice() const;
};

#endif