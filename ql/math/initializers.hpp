/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2018 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
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

/*! \file initializers.hpp
    \brief array and matrix initializers
*/

#ifndef quantlib_initializers_hpp
#define quantlib_initializers_hpp

#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>

namespace QuantLib {

namespace initializers {

/*! \deprecated Use initializer lists instead.
                Deprecated in version 1.22.
*/
class QL_DEPRECATED ArrayProxy {
public:
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    ArrayProxy& operator,(const Real x) {
        QL_REQUIRE(a_.size() > idx_,
                   "ArrayProxy: too many initializers, array has size "
                       << a_.size());
        a_[idx_++] = x;
        return *this;
    }

private:
    ArrayProxy(Array& a, const Real x) : a_(a) {
        QL_REQUIRE(!a_.empty(), "ArrayProxy: array has size 0");
        a_[0] = x;
        idx_ = 1;
    }
    friend ArrayProxy operator<<(Array&, Real);
    Size idx_;
    Array& a_;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
};

/*! \deprecated Use initializer lists instead.
                Deprecated in version 1.22.
*/
class QL_DEPRECATED MatrixProxy {
public:
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    MatrixProxy& operator,(const Real x) {
        QL_REQUIRE(m_.rows() * m_.columns() > idx_,
                   "MatrixProxy: too many initializers, matrix has size "
                       << m_.rows() << "x" << m_.columns());
        *(m_.begin() + idx_++) = x;
        return *this;
    }

private:
    MatrixProxy(Matrix& m, const Real x) : m_(m) {
        QL_REQUIRE(m_.rows() * m_.columns() > 0,
                   "MatrixProxy: matrix has size 0");
        *m_.begin() = x;
        idx_ = 1;
    }
    friend MatrixProxy operator<<(Matrix&, Real);
    Size idx_;
    Matrix& m_;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
};

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#if defined(QL_PATCH_MSVC)
#pragma warning(push)
#pragma warning(disable:4996)
#endif

/*! \deprecated Use initializer lists instead.
                Deprecated in version 1.22.
*/
QL_DEPRECATED
inline ArrayProxy operator<<(Array& a, const Real x) {
    return {a, x};
}

/*! \deprecated Use initializer lists instead.
                Deprecated in version 1.22.
*/
QL_DEPRECATED
inline MatrixProxy operator<<(Matrix& m, const Real x) {
    return {m, x};
}

#if defined(QL_PATCH_MSVC)
#pragma warning(pop)
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

} // namespace initializers

using initializers::operator<<;   // NOLINT(misc-unused-using-decls)

} // namespace QuantLib

#endif
