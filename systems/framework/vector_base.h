#pragma once

#include <algorithm>
#include <memory>
#include <utility>

#include <Eigen/Dense>

#include "drake/common/default_scalars.h"
#include "drake/common/drake_copyable.h"
#include "drake/common/drake_deprecated.h"
#include "drake/common/drake_throw.h"
#include "drake/common/eigen_types.h"

namespace drake {
namespace systems {

/// VectorBase is an abstract base class that real-valued signals
/// between Systems and real-valued System state vectors must implement.
/// Classes that inherit from VectorBase will typically provide names
/// for the elements of the vector, and may also provide other
/// computations for the convenience of Systems handling the
/// signal. The vector is always a column vector. It may or may not
/// be contiguous in memory. Contiguous subclasses should typically
/// inherit from BasicVector, not from VectorBase directly.
///
/// @tparam T Must be a Scalar compatible with Eigen.
template <typename T>
class VectorBase {
 public:
  // VectorBase objects are neither copyable nor moveable.
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(VectorBase)

  virtual ~VectorBase() {}

  /// Returns the number of elements in the vector.
  ///
  /// Implementations should ensure this operation is O(1) and allocates no
  /// memory.
  virtual int size() const = 0;

  /// Returns the element at the given index in the vector.
  /// @throws std::runtime_error if the index is >= size().
  ///
  /// Implementations should ensure this operation is O(1) and allocates no
  /// memory.
  virtual const T& GetAtIndex(int index) const = 0;

  /// Returns the element at the given index in the vector.
  /// @throws std::runtime_error if the index is >= size().
  ///
  /// Implementations should ensure this operation is O(1) and allocates no
  /// memory.
  virtual T& GetAtIndex(int index) = 0;

  T& operator[](std::size_t idx) { return GetAtIndex(idx); }
  const T& operator[](std::size_t idx) const { return GetAtIndex(idx); }

  /// Replaces the state at the given index with the value.
  /// @throws std::runtime_error if the index is >= size().
  void SetAtIndex(int index, const T& value) {
    GetAtIndex(index) = value;
  }

  /// Replaces the entire vector with the contents of @p value.
  /// @throws std::runtime_error if @p value is not a column vector with size()
  /// rows.
  ///
  /// Implementations should ensure this operation is O(N) in the size of the
  /// value and allocates no memory.
  virtual void SetFrom(const VectorBase<T>& value) {
    DRAKE_THROW_UNLESS(value.size() == size());
    for (int i = 0; i < value.size(); ++i) {
      SetAtIndex(i, value.GetAtIndex((i)));
    }
  }

  /// Replaces the entire vector with the contents of @p value. Throws
  /// std::runtime_error if @p value is not a column vector with size() rows.
  ///
  /// Implementations should ensure this operation is O(N) in the size of the
  /// value and allocates no memory.
  virtual void SetFromVector(const Eigen::Ref<const VectorX<T>>& value) {
    DRAKE_THROW_UNLESS(value.rows() == size());
    for (int i = 0; i < value.rows(); ++i) {
      SetAtIndex(i, value[i]);
    }
  }

  virtual void SetZero() {
    const int sz = size();
    for (int i = 0; i < sz; ++i) {
      SetAtIndex(i, T(0));
    }
  }

  /// Copies this entire %VectorBase into a contiguous Eigen Vector.
  ///
  /// Implementations should ensure this operation is O(N) in the size of the
  /// value and allocates only the O(N) memory that it returns.
  virtual VectorX<T> CopyToVector() const {
    VectorX<T> vec(size());
    for (int i = 0; i < size(); ++i) {
      vec[i] = GetAtIndex(i);
    }
    return vec;
  }

  /// Copies this entire %VectorBase into a pre-sized Eigen Vector.
  ///
  /// Implementations should ensure this operation is O(N) in the size of the
  /// value.
  /// @throws std::exception if `vec` is the wrong size.
  virtual void CopyToPreSizedVector(EigenPtr<VectorX<T>> vec) const {
    DRAKE_THROW_UNLESS(vec != nullptr);
    DRAKE_THROW_UNLESS(vec->rows() == size());
    for (int i = 0; i < size(); ++i) {
      (*vec)[i] = GetAtIndex(i);
    }
  }

  /// Adds a scaled version of this vector to Eigen vector @p vec, which
  /// must be the same size.
  ///
  /// Implementations may override this default implementation with a more
  /// efficient approach, for instance if this vector is contiguous.
  /// Implementations should ensure this operation remains O(N) in the size of
  /// the value and allocates no memory.
  virtual void ScaleAndAddToVector(const T& scale,
                                   EigenPtr<VectorX<T>> vec) const {
    DRAKE_THROW_UNLESS(vec != nullptr);
    if (vec->rows() != size()) {
      throw std::out_of_range("Addends must be the same size.");
    }
    for (int i = 0; i < size(); ++i) {
      (*vec)[i] += scale * GetAtIndex(i);
    }
  }

  DRAKE_DEPRECATED("2019-06-01", "Use the EigenPtr overload instead.")
  void ScaleAndAddToVector(const T& scale, Eigen::Ref<VectorX<T>> vec) const {
    ScaleAndAddToVector(scale, &vec);
  }

  /// Add in scaled vector @p rhs to this vector. Both vectors must
  /// be the same size.
  VectorBase& PlusEqScaled(const T& scale, const VectorBase<T>& rhs) {
    return PlusEqScaled({{scale, rhs}});
  }

  /// Add in multiple scaled vectors to this vector. All vectors
  /// must be the same size.
  VectorBase& PlusEqScaled(const std::initializer_list<
                           std::pair<T, const VectorBase<T>&>>& rhs_scale) {
    for (const auto& operand : rhs_scale) {
      if (operand.second.size() != size())
        throw std::out_of_range("Addends must be the same size.");
    }

    DoPlusEqScaled(rhs_scale);

    return *this;
  }

  /// Add in vector @p rhs to this vector.
  VectorBase& operator+=(const VectorBase<T>& rhs) {
    return PlusEqScaled(T(1), rhs);
  }

  /// Subtract in vector @p rhs to this vector.
  VectorBase& operator-=(const VectorBase<T>& rhs) {
    return PlusEqScaled(T(-1), rhs);
  }

  DRAKE_DEPRECATED("2019-06-01", "Use CopyToVector + Eigen lpNorm.")
  virtual T NormInf() const {
    using std::abs;
    using std::max;
    T norm(0);
    const int count = size();
    for (int i = 0; i < count; ++i) {
      T val = abs(GetAtIndex(i));
      norm = max(norm, val);
    }

    return norm;
  }

  /// Get the bounds for the elements.
  /// If lower and upper are both empty size vectors, then there are no bounds.
  /// Otherwise, the bounds are (*lower)(i) <= GetAtIndex(i) <= (*upper)(i)
  /// The default output is no bounds.
  virtual void GetElementBounds(Eigen::VectorXd* lower,
                                Eigen::VectorXd* upper) const {
    lower->resize(0);
    upper->resize(0);
  }

 protected:
  VectorBase() {}

  /// Adds in multiple scaled vectors to this vector. All vectors
  /// are guaranteed to be the same size.
  ///
  /// You should override this method if possible with a more efficient
  /// approach that leverages structure; the default implementation performs
  /// element-by-element computations that are likely inefficient, but even
  /// this implementation minimizes memory accesses for efficiency. If the
  /// vector is contiguous, for example, implementations that leverage SIMD
  /// operations should be far more efficient. Overriding implementations should
  /// ensure that this operation remains O(N) in the size of
  /// the value and allocates no memory.
  virtual void DoPlusEqScaled(const std::initializer_list<
                              std::pair<T, const VectorBase<T>&>>& rhs_scale) {
    const int sz = size();
    for (int i = 0; i < sz; ++i) {
      T value(0);
      for (const auto& operand : rhs_scale)
        value += operand.second.GetAtIndex(i) * operand.first;
      SetAtIndex(i, GetAtIndex(i) + value);
    }
  }
};

// Allows a VectorBase<T> to be streamed into a string. This is useful for
// debugging purposes.
template <typename T>
std::ostream& operator<<(std::ostream& os, const VectorBase<T>& vec) {
  os << "[";

  for (int i = 0; i < vec.size(); ++i) {
    if (i > 0)
      os << ", ";
    os << vec.GetAtIndex(i);
  }

  os << "]";
  return os;
}

}  // namespace systems
}  // namespace drake

DRAKE_DECLARE_CLASS_TEMPLATE_INSTANTIATIONS_ON_DEFAULT_SCALARS(
    class ::drake::systems::VectorBase)
