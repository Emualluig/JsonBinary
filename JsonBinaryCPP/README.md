# JsonBinaryCPP

This is a header-only library that allows to serialize and deserialize JSON to and from a binary format.

It depends on nlohmann::json library for JSON objects.

Serializers for important classes such as vector and map are provided. If you wish to add your own you must provide a specialization of your type for Serialization::serialize_impl.

It is also possible to deserialize into a C++ stl type such as vector or map.
