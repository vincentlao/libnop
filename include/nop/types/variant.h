#ifndef LIBNOP_INCLUDE_NOP_TYPES_VARIANT_H_
#define LIBNOP_INCLUDE_NOP_TYPES_VARIANT_H_

#include <cstdint>
#include <tuple>
#include <type_traits>

#include <nop/types/detail/variant.h>

namespace nop {

template <typename... Types>
class Variant {
 private:
  // Convenience types.
  template <typename T>
  using TypeTag = detail::TypeTag<T>;
  template <typename T>
  using DecayedTypeTag = TypeTag<std::decay_t<T>>;
  template <std::size_t I>
  using TypeForIndex = detail::TypeForIndex<I, Types...>;
  template <std::size_t I>
  using TypeTagForIndex = detail::TypeTagForIndex<I, Types...>;
  template <typename T>
  using HasType = detail::HasType<T, Types...>;
  template <typename R, typename T>
  using EnableIfElement = detail::EnableIfElement<R, T, Types...>;
  template <typename R, typename T>
  using EnableIfConvertible = detail::EnableIfConvertible<R, T, Types...>;
  template <typename R, typename T>
  using EnableIfAssignable = detail::EnableIfAssignable<R, T, Types...>;

  struct Direct {};
  struct Convert {};
  template <typename T>
  using SelectConstructor = detail::Select<HasType<T>::value, Direct, Convert>;

  // Constructs by type tag when T is an direct element of Types...
  template <typename T>
  explicit Variant(T&& value, Direct)
      : value_(0, &index_, DecayedTypeTag<T>{}, std::forward<T>(value)) {}
  // Conversion constructor when T is not a direct element of Types...
  template <typename T>
  explicit Variant(T&& value, Convert)
      : value_(0, &index_, std::forward<T>(value)) {}

 public:
  // Variants are default construcible, regardless of whether the elements are
  // default constructible. Default consruction yields an empty Variant.
  Variant() {}
  explicit Variant(EmptyVariant) {}
  ~Variant() { Destruct(); }

  // Copy and move construction from Variant types. Each element of OtherTypes
  // must be convertible to an element of Types.
  template <typename... OtherTypes>
  explicit Variant(const Variant<OtherTypes...>& other) {
    other.Visit([this](const auto& value) { Construct(value); });
  }
  template <typename... OtherTypes>
  explicit Variant(Variant<OtherTypes...>&& other) {
    other.Visit([this](auto&& value) { Construct(std::move(value)); });
  }

  // Construction from non-Variant types.
  template <typename T, typename = EnableIfAssignable<void, T>>
  explicit Variant(T&& value)
      : Variant(std::forward<T>(value), SelectConstructor<T>{}) {}

  // Performs assignment from type T belonging to Types. This overload takes
  // priority to prevent implicit conversion in cases where T is implicitly
  // convertible to multiple elements of Types.
  template <typename T>
  EnableIfElement<Variant&, T> operator=(T&& value) {
    Assign(DecayedTypeTag<T>{}, std::forward<T>(value));
    return *this;
  }

  // Performs assignment from type T not belonging to Types. This overload
  // matches in cases where conversion is the only viable option.
  template <typename T>
  EnableIfConvertible<Variant&, T> operator=(T&& value) {
    Assign(std::forward<T>(value));
    return *this;
  }

  // Handles assignment from the empty type. This overload supports assignment
  // in visitors using generic lambdas.
  Variant& operator=(EmptyVariant) {
    Assign(EmptyVariant{});
    return *this;
  }

  // Assignment from Variant types. Each element of OtherTypes must be
  // convertible to an element of Types. Forwards through non-Variant assignment
  // operators to apply conversion checks.
  template <typename... OtherTypes>
  Variant& operator=(const Variant<OtherTypes...>& other) {
    other.Visit([this](const auto& value) { *this = value; });
    return *this;
  }
  template <typename... OtherTypes>
  Variant& operator=(Variant<OtherTypes...>&& other) {
    other.Visit([this](auto&& value) { *this = std::move(value); });
    return *this;
  }

  // Becomes the target type, constructing a new element from the given
  // arguments if necessary. No action is taken if the active element is already
  // the target type. Otherwise the active element is destroyed and replaced by
  // constructing an element of the new type using |Args|. An invalid target
  // type index results in an empty Variant.
  template <typename... Args>
  void Become(std::int32_t target_index, Args&&... args) {
    if (target_index != index()) {
      Destruct();
      index_ = value_.Become(target_index, std::forward<Args>(args)...)
                   ? target_index
                   : kEmptyIndex;
    }
  }

  // Invokes |Op| on the active element. If the Variant is empty |Op| is invoked
  // on EmptyVariant.
  template <typename Op>
  decltype(auto) Visit(Op&& op) {
    return value_.Visit(index_, std::forward<Op>(op));
  }
  template <typename Op>
  decltype(auto) Visit(Op&& op) const {
    return value_.Visit(index_, std::forward<Op>(op));
  }

  // Index returned when the Variant is empty.
  enum : std::int32_t { kEmptyIndex = -1 };

  // Returns the index of the given type.
  template <typename T>
  constexpr std::int32_t index_of() const {
    static_assert(HasType<T>::value, "T is not an element type of Variant.");
    return value_.template index(DecayedTypeTag<T>{});
  }

  // Returns the index of the active type. If the Variant is empty -1 is
  // returned.
  std::int32_t index() const { return index_; }

  // Returns true if the given type is active, false otherwise.
  template <typename T>
  bool is() const {
    static_assert(HasType<T>::value, "T is not an element type of Variant.");
    return index() == index_of<T>();
  }

  // Returns true if the Variant is empty, false otherwise.
  bool empty() const { return index() == kEmptyIndex; }

  // Element accessors. Returns a pointer to the active value if the given
  // type/index is active, otherwise nullptr is returned.
  template <typename T>
  T* get() {
    if (is<T>())
      return &value_.template get(DecayedTypeTag<T>{});
    else
      return nullptr;
  }
  template <typename T>
  const T* get() const {
    if (is<T>())
      return &value_.template get(DecayedTypeTag<T>{});
    else
      return nullptr;
  }
  template <std::size_t I>
  TypeForIndex<I>* get() {
    if (is<TypeForIndex<I>>())
      return &value_.template get(TypeTagForIndex<I>{});
    else
      return nullptr;
  }
  template <std::size_t I>
  const TypeForIndex<I>* get() const {
    if (is<TypeForIndex<I>>())
      return &value_.template get(TypeTagForIndex<I>{});
    else
      return nullptr;
  }

 private:
  std::int32_t index_ = kEmptyIndex;
  detail::Union<std::decay_t<Types>...> value_;

  // Constructs an element from the given arguments and sets the Variant to the
  // resulting type.
  template <typename... Args>
  void Construct(Args&&... args) {
    index_ = value_.template Construct(std::forward<Args>(args)...);
  }
  void Construct(EmptyVariant) {}

  // Destroys the active element of the Variant.
  void Destruct() { value_.Destruct(index_); }

  // Assigns the Variant when non-empty and the current type matches the target
  // type, otherwise destroys the current value and constructs a element of the
  // new type. Tagged assignment is used when T is an element of the Variant to
  // prevent implicit conversion in cases where T is implicitly convertible to
  // multiple element types.
  template <typename T, typename U>
  void Assign(TypeTag<T>, U&& value) {
    if (!value_.template Assign(TypeTag<T>{}, index_, std::forward<U>(value))) {
      Destruct();
      Construct(TypeTag<T>{}, std::forward<U>(value));
    }
  }
  template <typename T>
  void Assign(T&& value) {
    if (!value_.template Assign(index_, std::forward<T>(value))) {
      Destruct();
      Construct(std::forward<T>(value));
    }
  }
  // Handles assignment from an empty Variant.
  void Assign(EmptyVariant) { Destruct(); }
};

// Utility type to extract/convert values from a variant. This class simplifies
// conditional logic to get/move/swap/action values from a variant when one or
// more elements are compatible with the destination type.
//
// Example:
//    Variant<int, bool, std::string> v(10);
//    bool bool_value;
//    if (IfAnyOf<int, bool>::Get(v, &bool_value)) {
//      DoSomething(bool_value);
//    } else {
//      HandleInvalidType();
//    }
//    IfAnyOf<int>::Call(v, [](const auto& value) { DoSomething(value); });
//
template <typename... ValidTypes>
struct IfAnyOf {
  // Calls Op on the underlying value of the variant and returns true when the
  // variant is a valid type, otherwise does nothing and returns false.
  template <typename Op, typename... Types>
  static bool Call(Variant<Types...>* variant, Op&& op) {
    static_assert(
        detail::Set<Types...>::template IsSubset<ValidTypes...>::value,
        "ValidTypes may only contain element types from the Variant.");
    return variant->Visit(CallOp<Op>{std::forward<Op>(op)});
  }
  template <typename Op, typename... Types>
  static bool Call(const Variant<Types...>* variant, Op&& op) {
    static_assert(
        detail::Set<Types...>::template IsSubset<ValidTypes...>::value,
        "ValidTypes may only contain element types from the Variant.");
    return variant->Visit(CallOp<Op>{std::forward<Op>(op)});
  }

  // Gets/converts the underlying value of the variant to type T and returns
  // true when the variant is a valid type, otherwise does nothing and returns
  // false.
  template <typename T, typename... Types>
  static bool Get(const Variant<Types...>* variant, T* value_out) {
    return Call(variant,
                [value_out](const auto& value) { *value_out = value; });
  }

  // Moves the underlying value of the variant and returns true when the variant
  // is a valid type, otherwise does nothing and returns false.
  template <typename T, typename... Types>
  static bool Take(Variant<Types...>* variant, T* value_out) {
    return Call(variant,
                [value_out](auto&& value) { *value_out = std::move(value); });
  }

  // Swaps the underlying value of the variant with |*value_out| and returns
  // true when the variant is a valid type, otherwise does nothing and returns
  // false.
  template <typename T, typename... Types>
  static bool Swap(Variant<Types...>* variant, T* value_out) {
    return Call(variant,
                [value_out](auto&& value) { std::swap(*value_out, value); });
  }

 private:
  template <typename Op>
  struct CallOp {
    Op&& op;
    template <typename U>
    detail::EnableIfNotElement<bool, U, ValidTypes...> operator()(U&&) {
      return false;
    }
    template <typename U>
    detail::EnableIfElement<bool, U, ValidTypes...> operator()(const U& value) {
      std::forward<Op>(op)(value);
      return true;
    }
    template <typename U>
    detail::EnableIfElement<bool, U, ValidTypes...> operator()(U&& value) {
      std::forward<Op>(op)(std::forward<U>(value));
      return true;
    }
  };
};

}  // namespace nop

// Overloads of std::get<T> and std::get<I> for nop::Variant.
namespace std {

template <typename T, typename... Types>
inline T& get(::nop::Variant<Types...>& v) {
  return *v.template get<T>();
}
template <typename T, typename... Types>
inline T&& get(::nop::Variant<Types...>&& v) {
  return std::move(*v.template get<T>());
}
template <typename T, typename... Types>
inline const T& get(const ::nop::Variant<Types...>& v) {
  return *v.template get<T>();
}
template <std::size_t I, typename... Types>
inline ::nop::detail::TypeForIndex<I, Types...>& get(
    ::nop::Variant<Types...>& v) {
  return *v.template get<I>();
}
template <std::size_t I, typename... Types>
inline ::nop::detail::TypeForIndex<I, Types...>&& get(
    ::nop::Variant<Types...>&& v) {
  return std::move(*v.template get<I>());
}
template <std::size_t I, typename... Types>
inline const ::nop::detail::TypeForIndex<I, Types...>& get(
    const ::nop::Variant<Types...>& v) {
  return *v.template get<I>();
}

}  // namespace std

#endif  // LIBNOP_INCLUDE_NOP_TYPES_VARIANT_H_