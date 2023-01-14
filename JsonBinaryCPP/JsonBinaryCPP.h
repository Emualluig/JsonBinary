#ifndef _JSONBINARY_CPP_H_
#define _JSONBINARY_CPP_H_

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <set>
#include <optional>
#include <exception>

namespace JsonBinary {
	namespace Internal {
		template<typename T> concept isTriviallyCopyable = std::is_trivially_copyable<T>::value;
		template <auto A, typename...> auto DelayEvaluation = A;

		template<typename T> concept isVector = 
			std::same_as<T, std::vector<typename T::value_type, typename T::allocator_type>>;

		template<typename T> concept isSet =
			std::same_as<T, std::set<typename T::key_type, typename T::key_compare, typename T::allocator_type>> ||
			std::same_as<T, std::unordered_set<typename T::key_type, typename T::hasher, typename T::key_equal, typename T::allocator_type>>;

		template<typename T> concept isMap =
			std::same_as<T, std::map<typename T::key_type, typename T::mapped_type, typename T::key_compare, typename T::allocator_type>> ||
			std::same_as<T, std::unordered_map<typename T::key_type, typename T::mapped_type, typename T::hasher, typename T::key_equal, typename T::allocator_type>>;
		
		template<typename T> concept isOptional =
			std::same_as<T, std::optional<typename T::value_type>>;

		enum class BinType : uint8_t {
			OBJECT_8 = 1,
			OBJECT_64,
			ARRAY_8,
			ARRAY_64,
			STRING_8, // A string that uses 8-bits as length
			STRING_64, // A string that uses 64-bits as length
			NULL_T,
			BOOLEAN_TRUE,
			BOOLEAN_FALSE,
			NUMBER_FLOAT_32,
			NUMBER_FLOAT_64,
			NUMBER_INTEGER_8,
			NUMBER_INTEGER_16,
			NUMBER_INTEGER_32,
			NUMBER_INTEGER_64,
			NUMBER_UNSIGNED_8,
			NUMBER_UNSIGNED_16,
			NUMBER_UNSIGNED_32,
			NUMBER_UNSIGNED_64,
		};
		class ByteArray {
			std::vector<uint8_t> bytes;
		public:
			template <typename Object> requires isTriviallyCopyable<Object> void push_back(Object object) {
				constexpr std::size_t sizeType = sizeof(Object);
				const uint8_t* ptr = (const uint8_t*)&object;
				for (std::size_t i = 0; i < sizeType; i++) {
					bytes.push_back(ptr[i]);
				}
			}
			std::vector<uint8_t> get() noexcept {
				return bytes;
			}
		};

		template<typename T> requires isTriviallyCopyable<T>
		T readBytesAs(const std::vector<uint8_t>& bytes, std::size_t& index) {
			constexpr std::size_t N = sizeof(T);
			const std::size_t availableSpace = bytes.size() - index;
			if (availableSpace < N) {
				throw std::exception("Not enough requested bytes.");
			}
			const T* ptr = (const T*)&bytes[index];
			index += N;
			return *ptr;
		}
	};

	namespace Deserialization {
		namespace Json {
			nlohmann::json deserialize_impl_json(const std::vector<uint8_t>& bytes, std::size_t& index, int depth) {
				if (depth > 32) {
					throw std::exception("Recursive depth too deep.");
				}
				Internal::BinType type = Internal::readBytesAs<Internal::BinType>(bytes, index);
				switch (type) {
					case Internal::BinType::OBJECT_8:
					{
						nlohmann::json object = nlohmann::json::object();
						const uint8_t length = Internal::readBytesAs<uint8_t>(bytes, index);
						for (std::size_t i = 0; i < length; i++) {
							// Read string
							nlohmann::json strJSON = deserialize_impl_json(bytes, index, depth + 1);
							if (!strJSON.is_string()) {
								throw std::exception("Objects must have strings as keys.");
							}
							// Read the value
							nlohmann::json valueJSON = deserialize_impl_json(bytes, index, depth + 1);
							object.push_back({ strJSON, valueJSON });
						}
						return object;
					}
					case Internal::BinType::OBJECT_64:
					{
						nlohmann::json object = nlohmann::json::object();
						const uint64_t length = Internal::readBytesAs<uint64_t>(bytes, index);
						for (std::size_t i = 0; i < length; i++) {
							// Read string
							nlohmann::json strJSON = deserialize_impl_json(bytes, index, depth + 1);
							if (!strJSON.is_string()) {
								throw std::exception("Objects must have strings as keys.");
							}
							// Read the value
							nlohmann::json valueJSON = deserialize_impl_json(bytes, index, depth + 1);
							object.push_back({ strJSON, valueJSON });
						}
						return object;
					}
					case Internal::BinType::ARRAY_8:
					{
						nlohmann::json array = nlohmann::json::array();
						const uint8_t length = Internal::readBytesAs<uint8_t>(bytes, index);
						for (std::size_t i = 0; i < length; i++) {
							// Read each element
							nlohmann::json element = deserialize_impl_json(bytes, index, depth + 1);
							array.push_back(element);
						}
						return array;
					}
					case Internal::BinType::ARRAY_64:
					{
						nlohmann::json array = nlohmann::json::array();
						const uint64_t length = Internal::readBytesAs<uint64_t>(bytes, index);
						for (std::size_t i = 0; i < length; i++) {
							// Read each element
							nlohmann::json element = deserialize_impl_json(bytes, index, depth + 1);
							array.push_back(element);
						}
						return array;
					}
					case Internal::BinType::STRING_8:
					{
						const uint8_t length = Internal::readBytesAs<uint8_t>(bytes, index);
						if (bytes.size() - index < length) {
							throw std::exception("String length not matched to data length.");
						}
						std::string str = std::string((const char*)&bytes[index], length);
						index += length;
						return str;
					}
					case Internal::BinType::STRING_64:
					{
						const uint64_t length = Internal::readBytesAs<uint64_t>(bytes, index);
						if (bytes.size() - index < length) {
							throw std::exception("String length not matched to data length.");
						}
						std::string str = std::string((const char*)&bytes[index], length);
						index += length;
						return str;
					}
					case Internal::BinType::NULL_T:
						return nullptr;
					case Internal::BinType::BOOLEAN_TRUE:
						return true;
					case Internal::BinType::BOOLEAN_FALSE:
						return false;
					case Internal::BinType::NUMBER_FLOAT_32:
						return static_cast<double>(Internal::readBytesAs<float>(bytes, index));
					case Internal::BinType::NUMBER_FLOAT_64:
						return Internal::readBytesAs<double>(bytes, index);
					case Internal::BinType::NUMBER_INTEGER_8:
						return (int64_t)Internal::readBytesAs<int8_t>(bytes, index);
					case Internal::BinType::NUMBER_INTEGER_16:
						return (int64_t)Internal::readBytesAs<int16_t>(bytes, index);
					case Internal::BinType::NUMBER_INTEGER_32:
						return (int64_t)Internal::readBytesAs<int32_t>(bytes, index);
					case Internal::BinType::NUMBER_INTEGER_64:
						return Internal::readBytesAs<int64_t>(bytes, index);
					case Internal::BinType::NUMBER_UNSIGNED_8:
						return (uint64_t)Internal::readBytesAs<uint8_t>(bytes, index);
					case Internal::BinType::NUMBER_UNSIGNED_16:
						return (uint64_t)Internal::readBytesAs<uint16_t>(bytes, index);
					case Internal::BinType::NUMBER_UNSIGNED_32:
						return (uint64_t)Internal::readBytesAs<uint32_t>(bytes, index);
					case Internal::BinType::NUMBER_UNSIGNED_64:
						return Internal::readBytesAs<uint64_t>(bytes, index);
					default:
						throw std::exception("Malformed json.");
				}
			}
		};
		template<typename T> static T deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) = delete;
		template<> static std::string deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::STRING_8) {
				const auto size = Internal::readBytesAs<uint8_t>(bytes, index);
				std::string retval = "";
				for (std::size_t i = 0; i < size; i++) {
					retval += Internal::readBytesAs<uint8_t>(bytes, index);
				}
				return retval;
			}
			else if (type == Internal::BinType::STRING_64) {
				const auto size = Internal::readBytesAs<uint64_t>(bytes, index);
				std::string retval = "";
				for (std::size_t i = 0; i < size; i++) {
					retval += Internal::readBytesAs<uint8_t>(bytes, index);
				}
				return retval;
			}
			else {
				throw std::exception("Expected string type byte.");
			}
		}
		template<typename T> requires Internal::isVector<T> && Deserializable<typename T::value_type> 
		static T deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			using E = typename T::value_type;
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::ARRAY_8) {
				const auto size = Internal::readBytesAs<uint8_t>(bytes, index);
				T retval = {};
				for (std::size_t i = 0; i < size; i++) {
					retval.push_back(deserialize_impl<E>(bytes, index));
				}
				return retval;
			}
			else if (type == Internal::BinType::ARRAY_64) {
				const auto size = Internal::readBytesAs<uint64_t>(bytes, index);
				T retval = {};
				for (std::size_t i = 0; i < size; i++) {
					retval.push_back(deserialize_impl<E>(bytes, index));
				}
				return retval;
			}
			else {
				throw std::exception("Expected vector type byte.");
			}
		}
		template<typename T> requires Internal::isSet<T> && Deserializable<typename T::value_type> 
		static T deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			using E = typename T::value_type;
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::ARRAY_8) {
				const auto size = Internal::readBytesAs<uint8_t>(bytes, index);
				T retval = {};
				for (std::size_t i = 0; i < size; i++) {
					retval.insert(deserialize_impl<E>(bytes, index));
				}
				return retval;
			}
			else if (type == Internal::BinType::ARRAY_64) {
				const auto size = Internal::readBytesAs<uint64_t>(bytes, index);
				T retval = {};
				for (std::size_t i = 0; i < size; i++) {
					retval.insert(deserialize_impl<E>(bytes, index));
				}
				return retval;
			}
			else {
				throw std::exception("Expected vector (set) type byte.");
			}
		}
		template<typename T> requires Internal::isMap<T> && std::same_as<typename T::key_type, std::string> && Deserializable<typename T::mapped_type> 
		static T deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			using Value = typename T::mapped_type;
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::OBJECT_8) {
				const auto size = Internal::readBytesAs<uint8_t>(bytes, index);
				T retval = {};
				for (std::size_t i = 0; i < size; i++) {
					retval.insert({ 
						deserialize_impl<std::string>(bytes, index), 
						deserialize_impl<Value>(bytes, index) 
					});
				}
				return retval;
			}
			else if (type == Internal::BinType::OBJECT_64) {
				const auto size = Internal::readBytesAs<uint64_t>(bytes, index);
				T retval = {};
				for (std::size_t i = 0; i < size; i++) {
					retval.insert({
						deserialize_impl<std::string>(bytes, index),
						deserialize_impl<Value>(bytes, index)
					});
				}
				return retval;
			}
			else {
				throw std::exception("Expected object type byte.");
			}
		}
		template<typename T> requires Internal::isOptional<T>
		static T deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			using E = typename T::value_type;
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NULL_T) {
				return std::nullopt;
			}
			else {
				return deserialize_impl<E>(bytes, index);
			}
		}
		template<> static bool deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::BOOLEAN_FALSE) {
				return false;
			}
			else if (type == Internal::BinType::BOOLEAN_TRUE) {
				return true;
			}
			else {
				throw std::exception("Expected boolean type.");
			}
		}
		template<> static float deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_FLOAT_32) {
				return Internal::readBytesAs<float>(bytes, index);
			}
			else {
				throw std::exception("Expected float type.");
			}
		}
		template<> static double deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_FLOAT_64) {
				return Internal::readBytesAs<double>(bytes, index);
			}
			else {
				throw std::exception("Expected double type.");
			}
		}
		template<> static int8_t deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_INTEGER_8) {
				return Internal::readBytesAs<int8_t>(bytes, index);
			}
			else {
				throw std::exception("Expected i8 type.");
			}
		}
		template<> static int16_t deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_INTEGER_16) {
				return Internal::readBytesAs<int16_t>(bytes, index);
			}
			else {
				throw std::exception("Expected i16 type.");
			}
		}
		template<> static int32_t deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_INTEGER_32) {
				return Internal::readBytesAs<int32_t>(bytes, index);
			}
			else {
				throw std::exception("Expected i32 type.");
			}
		}
		template<> static int64_t deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_INTEGER_64) {
				return Internal::readBytesAs<int64_t>(bytes, index);
			}
			else {
				throw std::exception("Expected i64 type.");
			}
		}
		template<> static uint8_t deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_UNSIGNED_8) {
				return Internal::readBytesAs<uint8_t>(bytes, index);
			}
			else {
				throw std::exception("Expected u8 type.");
			}
		}
		template<> static uint16_t deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_UNSIGNED_16) {
				return Internal::readBytesAs<uint16_t>(bytes, index);
			}
			else {
				throw std::exception("Expected u16 type.");
			}
		}
		template<> static uint32_t deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_UNSIGNED_32) {
				return Internal::readBytesAs<uint32_t>(bytes, index);
			}
			else {
				throw std::exception("Expected u32 type.");
			}
		}
		template<> static uint64_t deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			const auto type = Internal::readBytesAs<Internal::BinType>(bytes, index);
			if (type == Internal::BinType::NUMBER_UNSIGNED_64) {
				return Internal::readBytesAs<uint64_t>(bytes, index);
			}
			else {
				throw std::exception("Expected u64 type.");
			}
		}
		template<> static nlohmann::json deserialize_impl(const std::vector<uint8_t>& bytes, std::size_t& index) {
			return Json::deserialize_impl_json(bytes, index, 0);
		};
		template<typename T> concept Deserializable = requires (const T& object, const std::vector<uint8_t>& bytes, std::size_t& index) {
			{ Deserialization::deserialize_impl<T>(bytes, index) } -> std::same_as<T>;
		};
	};
	template<typename T> requires Deserialization::Deserializable<T> T deserialize(const std::vector<uint8_t>& bytes) {
		std::size_t index = 0;
		// In the future we will read the header here
		return Deserialization::deserialize_impl<T>(bytes, index);
	}

	namespace Serialization {
		template<typename T> static void serialize_impl(const T& object, Internal::ByteArray& bytes) = delete;
		template<typename T> requires Internal::isTriviallyCopyable<T> static void serialize_impl(const T& object, Internal::ByteArray& bytes) {
			if constexpr (std::is_same<T, uint8_t>::value) {
				bytes.push_back(Internal::BinType::NUMBER_UNSIGNED_8);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, uint16_t>::value) {
				bytes.push_back(Internal::BinType::NUMBER_UNSIGNED_16);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, uint32_t>::value) {
				bytes.push_back(Internal::BinType::NUMBER_UNSIGNED_32);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, uint64_t>::value) {
				bytes.push_back(Internal::BinType::NUMBER_UNSIGNED_64);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, int8_t>::value) {
				bytes.push_back(Internal::BinType::NUMBER_INTEGER_8);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, int16_t>::value) {
				bytes.push_back(Internal::BinType::NUMBER_INTEGER_16);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, int32_t>::value) {
				bytes.push_back(Internal::BinType::NUMBER_INTEGER_32);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, int64_t>::value) {
				bytes.push_back(Internal::BinType::NUMBER_INTEGER_64);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, double>::value) {
				bytes.push_back(Internal::BinType::NUMBER_FLOAT_64);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, float>::value) {
				bytes.push_back(Internal::BinType::NUMBER_FLOAT_32);
				bytes.push_back(object);
			}
			else if constexpr (std::is_same<T, bool>::value) {
				if (object) {
					bytes.push_back(Internal::BinType::BOOLEAN_TRUE);
				}
				else {
					bytes.push_back(Internal::BinType::BOOLEAN_FALSE);
				}
			}
			else {
				static_assert(Internal::DelayEvaluation<false, T>, "Invalid template parameter.");
			}
		}
		template<typename T> requires Serializable<T> static void serialize_impl(const std::optional<T>& object, Internal::ByteArray& bytes) {
			if (object.has_value()) {
				serialize_impl(object.value(), bytes);
			}
			else {
				bytes.push_back(Internal::BinType::NULL_T);
			}
		}
		template<> static void serialize_impl(const std::string& object, Internal::ByteArray& bytes) {
			const auto size = object.size();
			if (std::in_range<uint8_t>(size)) {
				bytes.push_back(Internal::BinType::STRING_8);
				bytes.push_back((uint8_t)size);
			}
			else {
				bytes.push_back(Internal::BinType::STRING_64);
				bytes.push_back((uint64_t)size);
			}
			for (const auto c : object) {
				bytes.push_back(c);
			}
		}
		template<typename T> requires Serializable<T> static void serialize_impl(const std::vector<T>& object, Internal::ByteArray& bytes) {
			const auto size = object.size();
			if (std::in_range<uint8_t>(size)) {
				bytes.push_back(Internal::BinType::ARRAY_8);
				bytes.push_back((uint8_t)size);
			}
			else {
				bytes.push_back(Internal::BinType::ARRAY_64);
				bytes.push_back((uint64_t)size);
			}
			for (const T& c : object) {
				serialize_impl(c, bytes);
			}
		}
		template<typename T, std::size_t N> requires Serializable<T> static void serialize_impl(const std::array<T, N>& object, Internal::ByteArray& bytes) {
			const auto size = object.size();
			if (std::in_range<uint8_t>(size)) {
				bytes.push_back(Internal::BinType::ARRAY_8);
				bytes.push_back((uint8_t)size);
			}
			else {
				bytes.push_back(Internal::BinType::ARRAY_64);
				bytes.push_back((uint64_t)size);
			}
			for (const T& c : object) {
				serialize_impl(c, bytes);
			}
		}
		template<typename T> requires Serializable<T> static void serialize_impl(const std::set<T>& object, Internal::ByteArray& bytes) {
			const auto size = object.size();
			if (std::in_range<uint8_t>(size)) {
				bytes.push_back(Internal::BinType::ARRAY_8);
				bytes.push_back((uint8_t)size);
			}
			else {
				bytes.push_back(Internal::BinType::ARRAY_64);
				bytes.push_back((uint64_t)size);
			}
			for (const T& c : object) {
				serialize_impl(c, bytes);
			}
		}
		template<typename T> requires Serializable<T> static void serialize_impl(const std::unordered_set<T>& object, Internal::ByteArray& bytes) {
			const auto size = object.size();
			if (std::in_range<uint8_t>(size)) {
				bytes.push_back(Internal::BinType::ARRAY_8);
				bytes.push_back((uint8_t)size);
			}
			else {
				bytes.push_back(Internal::BinType::ARRAY_64);
				bytes.push_back((uint64_t)size);
			}
			for (const T& c : object) {
				serialize_impl(c, bytes);
			}
		}
		template<typename V> requires Serializable<V> static void serialize_impl(const std::map<std::string, V>& object, Internal::ByteArray& bytes) {
			const auto size = object.size();
			if (std::in_range<uint8_t>(size)) {
				bytes.push_back(Internal::BinType::OBJECT_8);
				bytes.push_back((uint8_t)size);
			}
			else {
				bytes.push_back(Internal::BinType::OBJECT_64);
				bytes.push_back((uint64_t)size);
			}
			for (const std::pair<std::string, V> &pair : object) {
				serialize_impl(pair.first, bytes);
				serialize_impl(pair.second, bytes);
			}
		}
		template<typename V> requires Serializable<V> static void serialize_impl(const std::unordered_map<std::string, V>& object, Internal::ByteArray& bytes) {
			const auto size = object.size();
			if (std::in_range<uint8_t>(size)) {
				bytes.push_back(Internal::BinType::OBJECT_8);
				bytes.push_back((uint8_t)size);
			}
			else {
				bytes.push_back(Internal::BinType::OBJECT_64);
				bytes.push_back((uint64_t)size);
			}
			for (const std::pair<std::string, V>& pair : object) {
				serialize_impl(pair.first, bytes);
				serialize_impl(pair.second, bytes);
			}
		}
		template<> static void serialize_impl(const nlohmann::json& object, Internal::ByteArray& bytes) {
			const auto type = object.type();
			const auto size = object.size();
			if (type == nlohmann::json::value_t::object) {
				if (std::in_range<uint8_t>(size)) {
					bytes.push_back(Internal::BinType::OBJECT_8);
					bytes.push_back((uint8_t)size);
				}
				else {
					bytes.push_back(Internal::BinType::OBJECT_64);
					bytes.push_back((uint64_t)size);
				}
				for (const auto& kv : object.items()) {
					serialize_impl(kv.key(), bytes);
					serialize_impl(kv.value(), bytes);
				}
			}
			else if (type == nlohmann::json::value_t::array) {
				if (std::in_range<uint8_t>(size)) {
					bytes.push_back(Internal::BinType::ARRAY_8);
					bytes.push_back((uint8_t)size);
				}
				else {
					bytes.push_back(Internal::BinType::ARRAY_64);
					bytes.push_back((uint64_t)size);
				}
				for (const auto& el : object) {
					serialize_impl(el, bytes);
				}
			}
			else if (type == nlohmann::json::value_t::string) {
				const auto str = object.get<std::string>();
				serialize_impl(str, bytes);
			}
			else if (type == nlohmann::json::value_t::null) {
				bytes.push_back(Internal::BinType::NULL_T);
			}
			else if (type == nlohmann::json::value_t::boolean) {
				const auto value = object.get<bool>();
				serialize_impl(value, bytes);
			}
			else if (type == nlohmann::json::value_t::number_float) {
				const auto value = object.get<double>();
				serialize_impl(value, bytes);
			}
			else if (type == nlohmann::json::value_t::number_integer) {
				const int64_t value = object.get<int64_t>();
				serialize_impl(value, bytes);
			}
			else if (type == nlohmann::json::value_t::number_unsigned) {
				const uint64_t value = object.get<uint64_t>();
				serialize_impl(value, bytes);
			}
			else {
				throw std::exception("Unsupported json type.");
			}
		}
		template<typename T> concept Serializable = requires (const T& object, Internal::ByteArray& bytes) {
			{ Serialization::serialize_impl(object, bytes) } -> std::same_as<void>;
		};
	};
	template<typename T> requires Serialization::Serializable<T> std::vector<uint8_t> serialize(const T& object) {
		Internal::ByteArray bytes = {};
		// In the future we will write the header here
		Serialization::serialize_impl(object, bytes);
		return bytes.get();
	}
};

#endif // !_JSONBINARY_CPP_H_

