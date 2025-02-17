#define SUITE arrow_extension_types

#include "vast/detail/overload.hpp"
#include "vast/test/test.hpp"
#include "vast/type.hpp"

#include <arrow/api.h>

namespace vast {

TEST(enum extension type equality) {
  enumeration_type::arrow_type t1{
    enumeration_type{{"one"}, {"two"}, {"three"}}};
  enumeration_type::arrow_type t2{
    enumeration_type{{"one"}, {"two"}, {"three"}}};
  enumeration_type::arrow_type t3{
    enumeration_type{{"one"}, {"three"}, {"two"}}};
  enumeration_type::arrow_type t4{
    enumeration_type{{"one"}, {"two", 3}, {"three"}}};
  enumeration_type::arrow_type t5{
    enumeration_type{{"some"}, {"other"}, {"vals"}}};
  CHECK(t1.ExtensionEquals(t2));
  CHECK(!t1.ExtensionEquals(t3));
  CHECK(!t1.ExtensionEquals(t4));
  CHECK(!t1.ExtensionEquals(t5));
}

namespace {

template <concrete_type Type>
void serde_roundtrip(const Type& type,
                     std::shared_ptr<typename Type::arrow_type> stub
                     = nullptr) {
  const auto& arrow_type = type.to_arrow_type();
  const auto serialized = arrow_type->Serialize();
  if (!stub)
    stub = type.to_arrow_type();
  const auto& deserialized
    = stub->Deserialize(arrow_type->storage_type(), serialized);
  if (!deserialized.status().ok())
    FAIL(deserialized.status().ToString());
  CHECK(arrow_type->Equals(*deserialized.ValueUnsafe(), true));
  CHECK(!stub->Deserialize(arrow::fixed_size_binary(23), serialized).ok());
}

template <class Builder, class T = typename Builder::value_type>
std::shared_ptr<arrow::Array> makeArrowArray(std::vector<T> xs) {
  Builder b{};
  CHECK(b.AppendValues(xs).ok());
  return b.Finish().ValueOrDie();
}

std::shared_ptr<arrow::Array> makeAddressArray() {
  arrow::FixedSizeBinaryBuilder b{arrow::fixed_size_binary(16)};
  return std::make_shared<address_type::array_type>(
    std::make_shared<address_type::arrow_type>(), b.Finish().ValueOrDie());
}

// Returns a visitor that checks whether the expected concrete types are the
// types resulting in the visitation.
template <class... T>
auto is_type() {
  return []<class... U>(const U&...) {
    return (std::is_same_v<T, U> && ...);
  };
}

} // namespace

TEST(arrow enum parse error) {
  const auto& standin
    = enumeration_type::arrow_type(enumeration_type{{"stub"}});
  auto r = standin.Deserialize(arrow::dictionary(arrow::uint8(), arrow::utf8()),
                               R"({ "a": "no_int" })");
  CHECK(r.status().IsSerializationError());
}

TEST(enumeration type serde roundtrip) {
  auto stub = enumeration_type{{"stub"}}.to_arrow_type();
  serde_roundtrip(enumeration_type{{"true"}, {"false"}}, stub);
  serde_roundtrip(enumeration_type{{"1"}, {"2"}, {"3"}, {"4"}}, stub);
}

TEST(address type serde roundtrip) {
  serde_roundtrip(address_type{});
}

TEST(subnet type serde roundtrip) {
  serde_roundtrip(subnet_type{});
}

TEST(pattern type serde roundtrip) {
  serde_roundtrip(pattern_type{});
}

TEST(arrow::DataType sum type) {
  CHECK(caf::visit(is_type<arrow::Int64Type>(), *arrow::int64()));
  CHECK(caf::visit(
    is_type<address_type::arrow_type>(),
    static_cast<const arrow::DataType&>(address_type::arrow_type())));
  CHECK(caf::visit(
    is_type<pattern_type::arrow_type>(),
    static_cast<const arrow::DataType&>(pattern_type::arrow_type())));
  CHECK(caf::visit(is_type<arrow::Int64Type, arrow::UInt64Type>(),
                   *arrow::int64(), *arrow::uint64()));
  CHECK_EQUAL(caf::get_if<arrow::StringType>(arrow::utf8().get()),
              arrow::utf8().get());
  auto et = static_pointer_cast<arrow::DataType>(
    std::make_shared<enumeration_type::arrow_type>(
      enumeration_type{{"A"}, {"B"}, {"C"}}));
  auto pt = static_pointer_cast<arrow::DataType>(
    std::make_shared<pattern_type::arrow_type>());
  CHECK(caf::get_if<enumeration_type::arrow_type>(et.get()));
  CHECK(!caf::get_if<enumeration_type::arrow_type>(pt.get()));
  CHECK(!caf::get_if<pattern_type::arrow_type>(et.get()));
  CHECK(caf::get_if<pattern_type::arrow_type>(pt.get()));
}

TEST(arrow::Array sum type) {
  auto str_arr = makeArrowArray<arrow::StringBuilder, std::string>({"a", "b"});
  auto uint_arr = makeArrowArray<arrow::UInt64Builder>({7, 8});
  auto int_arr = makeArrowArray<arrow::Int64Builder>({3, 2, 1});
  auto addr_arr = makeAddressArray();
  const auto& pattern_arr = std::static_pointer_cast<arrow::Array>(
    std::make_shared<pattern_type::array_type>(
      std::make_shared<pattern_type::arrow_type>(), str_arr));
  CHECK(caf::get_if<arrow::StringArray>(&*str_arr));
  CHECK(!caf::get_if<arrow::UInt64Array>(&*str_arr));
  CHECK(!caf::get_if<arrow::StringArray>(&*uint_arr));
  CHECK(caf::get_if<arrow::UInt64Array>(&*uint_arr));
  CHECK(!caf::get_if<address_type::array_type>(&*uint_arr));
  CHECK(!caf::get_if<pattern_type::array_type>(&*addr_arr));
  CHECK(caf::get_if<address_type::array_type>(&*addr_arr));
  CHECK(!caf::get_if<address_type::array_type>(&*pattern_arr));
  CHECK(caf::get_if<pattern_type::array_type>(&*pattern_arr));
  CHECK(caf::get_if<pattern_type::array_type>(&*pattern_arr));
  caf::visit(is_type<arrow::StringArray>(), *str_arr);
  caf::visit(is_type<pattern_type::array_type>(), *pattern_arr);
  caf::visit(is_type<pattern_type::array_type>(), *str_arr);
  auto f = detail::overload{
    [](const address_type::array_type&) {
      return 99;
    },
    [](const pattern_type::array_type&) {
      return 100;
    },
    [](const arrow::StringArray&) {
      return 101;
    },
    [](const auto&) {
      return -1;
    },
  };
  CHECK_EQUAL(caf::visit(f, *str_arr), 101);
  CHECK_EQUAL(caf::visit(f, *pattern_arr), 100);
  CHECK_EQUAL(caf::visit(f, *addr_arr), 99);
  CHECK_EQUAL(caf::visit(f, *int_arr), -1);
}

} // namespace vast
