#ifndef _JSONBINARY_CPP_H_
#define __JSONBINARY_CPP_H_

#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <exception>

namespace JsonBinary {
	namespace Internal {
		template<class T> concept isTriviallyCopyable = std::is_trivially_copyable<T>::value;
		template<class T, std::size_t N> concept isTypeSize = std::enable_if<sizeof(T) == N>::value;
		enum class BinType : unsigned char {
			OBJECT_8 = 1,
			OBJECT_64,
			ARRAY_8,
			ARRAY_64,
			STRING_8, // A string that uses 8-bits as length
			STRING_64, // A string that uses 64-bits as length
			NULL_T,
			BOOLEAN_TRUE,
			BOOLEAN_FALSE,
			NUMBER_FLOAT,
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
			bool isInvalid = false;
			std::vector<uint8_t> bytes;
		public:
			template <typename Object> requires isTriviallyCopyable<Object>
			void push_back(Object object) {
				if (isInvalid) {
					throw std::exception("ByteArray currwently is invalid.");
				}
				constexpr std::size_t sizeType = sizeof(Object);
				const uint8_t* ptr = (const uint8_t*)&object;
				for (std::size_t i = 0; i < sizeType; i++) {
					bytes.push_back(ptr[i]);
				}
			}
			std::vector<unsigned char> get() noexcept {
				isInvalid = true;
				return bytes;
			}
		};
		void serialize_Impl(const nlohmann::json& json, ByteArray& bytes, int depth = 0) {
			if (depth > 32) {
				throw std::exception("Recursive depth too great. Either the json has a circular reference, or is highly nested.");
			}
			const nlohmann::json::value_t value_t = json.type();
			switch (value_t) {
			case nlohmann::json::value_t::object:
				/*
					For objects we must put the type (1 byte), the length (4 bytes) then we serialize the string-value pairs
				*/
			{
				const uint64_t length = json.size();
				if (std::in_range<uint8_t>(length)) {
					bytes.push_back(BinType::OBJECT_8);
					bytes.push_back((uint8_t)json.size());
				}
				else {
					bytes.push_back(BinType::OBJECT_64);
					bytes.push_back((uint64_t)json.size());
				}
				for (const auto& kv : json.items()) {
					serialize_Impl(kv.key(), bytes, depth + 1);
					serialize_Impl(kv.value(), bytes, depth + 1);
				}
			}
			break;
			case nlohmann::json::value_t::array:
				/*
					For arrays we must put the type, the length, then the elements
				*/
			{
				const uint64_t length = json.size();
				if (std::in_range<uint8_t>(length)) {
					bytes.push_back(BinType::ARRAY_8);
					bytes.push_back((uint8_t)json.size());
				}
				else {
					bytes.push_back(BinType::ARRAY_64);
					bytes.push_back((uint64_t)json.size());
				}
				for (const auto& el : json) {
					serialize_Impl(el, bytes, depth + 1);
				}
			}
			break;
			case nlohmann::json::value_t::string:
				/*
					For strings we put the type, the length, then all the characters
				*/
			{
				const std::string& str = json.get<std::string>();
				const uint64_t length = str.size();
				if (std::in_range<uint8_t>(length)) {
					bytes.push_back(BinType::STRING_8);
					bytes.push_back((uint8_t)str.size());
				}
				else {
					// 64-bit length
					bytes.push_back(BinType::STRING_64);
					bytes.push_back((uint8_t)str.size());
				}
				for (const unsigned char c : str) {
					bytes.push_back(c);
				}
			}
			break;
			case nlohmann::json::value_t::null:
				/*
					For null we put the type and thats it
				*/
				bytes.push_back(BinType::NULL_T);
				break;
			case nlohmann::json::value_t::boolean:
				/*
					For boolean we choose between the BOOLEAN_TRUE and BOOLEAN_FALSE types
				*/
				if (json.get<bool>()) {
					bytes.push_back(BinType::BOOLEAN_TRUE);
				}
				else {
					bytes.push_back(BinType::BOOLEAN_FALSE);
				}
				break;
			case nlohmann::json::value_t::number_float:
				/*
					For floats we put the type, then the 8 bytes of a double
				*/
				bytes.push_back(BinType::NUMBER_FLOAT);
				bytes.push_back(json.get<double>());
				break;
			case nlohmann::json::value_t::number_integer:
				/*
					For integers we put the type, then the 8 bytes of a int64_t
				*/
			{
				const int64_t value = json.get<int64_t>();
				// Check if we can restrict the range
				if (std::in_range<int8_t>(value)) {
					bytes.push_back(BinType::NUMBER_INTEGER_8);
					bytes.push_back((int8_t)value);
				}
				else if (std::in_range<int16_t>(value)) {
					bytes.push_back(BinType::NUMBER_INTEGER_16);
					bytes.push_back((int16_t)value);
				}
				else if (std::in_range<int32_t>(value)) {
					bytes.push_back(BinType::NUMBER_INTEGER_32);
					bytes.push_back((int32_t)value);
				}
				else {
					// It is a 64-bit integer
					bytes.push_back(BinType::NUMBER_INTEGER_64);
					bytes.push_back((int64_t)value);
				}
			}
			break;
			case nlohmann::json::value_t::number_unsigned:
				/*
					For integers we put the type, then the 8 bytes of a uint64_t
				*/
			{
				const uint64_t value = json.get<uint64_t>();
				// Check if we can restrict the range
				if (std::in_range<uint8_t>(value)) {
					bytes.push_back(BinType::NUMBER_UNSIGNED_8);
					bytes.push_back((uint8_t)value);
				}
				else if (std::in_range<uint16_t>(value)) {
					bytes.push_back(BinType::NUMBER_UNSIGNED_16);
					bytes.push_back((uint16_t)value);
				}
				else if (std::in_range<uint32_t>(value)) {
					bytes.push_back(BinType::NUMBER_UNSIGNED_32);
					bytes.push_back((uint32_t)value);
				}
				else {
					// It is a 64-bit integer
					bytes.push_back(BinType::NUMBER_UNSIGNED_64);
					bytes.push_back((uint64_t)value);
				}
			}
			break;
			default:
				throw std::exception("Unsupported type.");
			}
		}

		template<typename T> requires isTriviallyCopyable<T>
		T readBytesAs(const std::vector<unsigned char>& bytes, std::size_t& index) {
			constexpr std::size_t N = sizeof(T);
			const std::size_t availableSpace = bytes.size() - index;
			if (availableSpace < N) {
				throw std::exception("Not enough requested bytes.");
			}
			const T* ptr = (const T*)&bytes[index];
			index += N;
			return *ptr;
		}
		nlohmann::json deserialize_Impl(const std::vector<unsigned char>& bytes, std::size_t& index, int depth) {
			if (depth > 32) {
				throw std::exception("Recursive depth too deep.");
			}
			BinType type = readBytesAs<BinType>(bytes, index);
			switch (type) {
			case BinType::OBJECT_8:
			{
				nlohmann::json object = nlohmann::json::object();
				const uint8_t length = readBytesAs<uint8_t>(bytes, index);
				for (std::size_t i = 0; i < length; i++) {
					// Read string
					nlohmann::json strJSON = deserialize_Impl(bytes, index, depth + 1);
					if (!strJSON.is_string()) {
						throw std::exception("Objects must have strings as keys.");
					}
					// Read the value
					nlohmann::json valueJSON = deserialize_Impl(bytes, index, depth + 1);
					object.push_back({ strJSON, valueJSON });
				}
				return object;
			}
			case BinType::OBJECT_64:
			{
				nlohmann::json object = nlohmann::json::object();
				const uint64_t length = readBytesAs<uint64_t>(bytes, index);
				for (std::size_t i = 0; i < length; i++) {
					// Read string
					nlohmann::json strJSON = deserialize_Impl(bytes, index, depth + 1);
					if (!strJSON.is_string()) {
						throw std::exception("Objects must have strings as keys.");
					}
					// Read the value
					nlohmann::json valueJSON = deserialize_Impl(bytes, index, depth + 1);
					object.push_back({ strJSON, valueJSON });
				}
				return object;
			}
			case BinType::ARRAY_8:
			{
				nlohmann::json array = nlohmann::json::array();
				const uint8_t length = readBytesAs<uint8_t>(bytes, index);
				for (std::size_t i = 0; i < length; i++) {
					// Read each element
					nlohmann::json element = deserialize_Impl(bytes, index, depth + 1);
					array.push_back(element);
				}
				return array;
			}
			case BinType::ARRAY_64:
			{
				nlohmann::json array = nlohmann::json::array();
				const uint64_t length = readBytesAs<uint64_t>(bytes, index);
				for (std::size_t i = 0; i < length; i++) {
					// Read each element
					nlohmann::json element = deserialize_Impl(bytes, index, depth + 1);
					array.push_back(element);
				}
				return array;
			}
			case BinType::STRING_8:
			{
				const uint8_t length = readBytesAs<uint8_t>(bytes, index);
				if (bytes.size() - index < length) {
					throw std::exception("String length not matched to data length.");
				}
				std::string str = std::string((const char*)&bytes[index], length);
				index += length;
				return str;
			}
			case BinType::STRING_64:
			{
				const uint64_t length = readBytesAs<uint64_t>(bytes, index);
				if (bytes.size() - index < length) {
					throw std::exception("String length not matched to data length.");
				}
				std::string str = std::string((const char*)&bytes[index], length);
				index += length;
				return str;
			}
			case BinType::NULL_T:
				return nullptr;
			case BinType::BOOLEAN_TRUE:
				return true;
			case BinType::BOOLEAN_FALSE:
				return false;
			case BinType::NUMBER_FLOAT:
				return readBytesAs<double>(bytes, index);
			case BinType::NUMBER_INTEGER_8:
				return (int64_t)readBytesAs<int8_t>(bytes, index);
			case BinType::NUMBER_INTEGER_16:
				return (int64_t)readBytesAs<int16_t>(bytes, index);
			case BinType::NUMBER_INTEGER_32:
				return (int64_t)readBytesAs<int32_t>(bytes, index);
			case BinType::NUMBER_INTEGER_64:
				return readBytesAs<int64_t>(bytes, index);
			case BinType::NUMBER_UNSIGNED_8:
				return (uint64_t)readBytesAs<uint8_t>(bytes, index);
			case BinType::NUMBER_UNSIGNED_16:
				return (uint64_t)readBytesAs<uint16_t>(bytes, index);
			case BinType::NUMBER_UNSIGNED_32:
				return (uint64_t)readBytesAs<uint32_t>(bytes, index);
			case BinType::NUMBER_UNSIGNED_64:
				return readBytesAs<uint64_t>(bytes, index);
			default:
				throw std::exception("Malformed json.");
			}
		}
	};
	nlohmann::json deserialize(const std::vector<unsigned char>& bytes) {
		std::size_t index = 0;
		return Internal::deserialize_Impl(bytes, index, 0);
	}
	std::vector<unsigned char> serialize(const nlohmann::json& json) {
		Internal::ByteArray bytes = {};
		Internal::serialize_Impl(json, bytes);
		return bytes.get();
	}
};

#endif // !_JSONBINARY_CPP_H_

